#if WITH_DEV_AUTOMATION_TESTS
#include "EnemyImpactComponent.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Components/BoxComponent.h"
#include "EnemyFXAimTrace.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyFXSliceTest, "EnemyCombatFX.Slicing.AssetFreeBox",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyFXSliceTest::RunTest(const FString&)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AEnemySliceTestActor* Target = World->SpawnActor<AEnemySliceTestActor>();
    if (!TestNotNull(TEXT("Asset-free target"), Target)) return false;
    Target->DispatchBeginPlay();
    UEnemyImpactComponent* FX = Target->Impact;
    TestFalse(TEXT("Zero normal rejected"), FX->SliceCorpse(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector));
    TestFalse(TEXT("Missed plane rejected"), FX->SliceCorpse(FVector(0,0,1000), FVector::UpVector, FVector::ZeroVector));
    TestEqual(TEXT("Failed cut preserves mesh"), Target->TestMesh->GetNumSections(), 1);
    TestTrue(TEXT("Plane divides box"), FX->SliceCorpse(FVector::ZeroVector, FVector::UpVector, FVector::ZeroVector));
    TInlineComponentArray<UProceduralMeshComponent*> Pieces(Target);
    TestEqual(TEXT("Two pieces"), Pieces.Num(), 2);
    for (UProceduralMeshComponent* Piece : Pieces)
    {
        TestTrue(TEXT("Cap section created"), Piece->GetNumSections() >= 2);
        const FProcMeshSection* Cap = Piece->GetProcMeshSection(Piece->GetNumSections()-1);
        TestTrue(TEXT("Cap contains triangles"), Cap && Cap->ProcIndexBuffer.Num() >= 3);
        TestEqual(TEXT("Pieces ignore camera"), Piece->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
        TestEqual(TEXT("Pieces ignore pawns"), Piece->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
    }
    TestFalse(TEXT("Cooldown rejects immediate cut"), FX->SliceCorpse(FVector::ZeroVector, FVector::ForwardVector, FVector::ZeroVector));
    FX->SliceCooldown = 0;
    FX->MaxPieces = 2;
    TestFalse(TEXT("Piece budget rejects further cut"), FX->SliceCorpse(FVector::ZeroVector, FVector::ForwardVector, FVector::ZeroVector));
    AEnemySliceTestActor* DiagonalTarget = World->SpawnActor<AEnemySliceTestActor>(FVector(300,0,0), FRotator::ZeroRotator);
    DiagonalTarget->DispatchBeginPlay();
    const FVector DiagonalNormal = FVector::UpVector.RotateAngleAxis(45, FVector::ForwardVector);
    TestTrue(TEXT("45-degree plane cuts box"), DiagonalTarget->Impact->SliceCorpse(FVector(300,0,0), DiagonalNormal, FVector::ZeroVector));
    TestFalse(TEXT("Dead flag required on ordinary actor"), NewObject<UEnemyImpactComponent>(Target)
        ->SliceCorpse(FVector::ZeroVector, FVector::UpVector, FVector::ZeroVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyFXAimTest, "EnemyCombatFX.Slicing.RagdollSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyFXAimTest::RunTest(const FString&)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto MakeBody = [World](FVector Position, ECollisionChannel Type)
    {
        AActor* Owner = World->SpawnActor<AActor>();
        UBoxComponent* Body = NewObject<UBoxComponent>(Owner);
        Owner->SetRootComponent(Body);
        Body->SetBoxExtent(FVector(20));
        Body->SetCollisionObjectType(Type);
        Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Body->SetCollisionResponseToAllChannels(ECR_Block);
        Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        Body->RegisterComponent();
        Body->SetWorldLocation(Position);
        return Body;
    };
    UBoxComponent* Corpse = MakeBody(FVector::ZeroVector, ECC_PhysicsBody);
    const FVector Start(-200,0,0), End(200,0,0);
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyFXAimTest), false);
    FHitResult Hit;
    TestFalse(TEXT("Old visibility query misses ragdoll"), World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params));
    TestTrue(TEXT("Command query finds physics body"), EnemyFXAimTrace::Trace(World, Hit, Start, End, Params));
    TestEqual(TEXT("Correct corpse selected"), Hit.GetComponent(), static_cast<UPrimitiveComponent*>(Corpse));
    UBoxComponent* Wall = MakeBody(FVector(-100,0,0), ECC_WorldStatic);
    TestTrue(TEXT("Trace still sees walls"), EnemyFXAimTrace::Trace(World, Hit, Start, End, Params));
    TestEqual(TEXT("Wall blocks corpse selection"), Hit.GetComponent(), static_cast<UPrimitiveComponent*>(Wall));
    return true;
}
#endif
