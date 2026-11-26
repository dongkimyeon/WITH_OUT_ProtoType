#include "EnemyImpactComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "KismetProceduralMeshLibrary.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "ProceduralMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "SkeletalRenderPublic.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyFX, Log, All);

UEnemyImpactComponent::UEnemyImpactComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyImpactComponent::BeginPlay()
{
    Super::BeginPlay();
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        Mesh = Character->GetMesh();
    else
        Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    if (Mesh)
    {
        PhysicalAnimation = NewObject<UPhysicalAnimationComponent>(GetOwner());
        PhysicalAnimation->RegisterComponent();
        PhysicalAnimation->SetSkeletalMeshComponent(Mesh);
    }
}

void UEnemyImpactComponent::Report(const FString& Message)
{
    LastResult = Message;
    UE_LOG(LogEnemyFX, Display, TEXT("%s: %s"), *GetNameSafe(GetOwner()), *Message);
}

void UEnemyImpactComponent::PlayHitReaction(const FHitResult& Hit, FVector ShotDirection)
{
    if (!bEnableHitReaction || bDead || !Mesh || !PhysicalAnimation || !Mesh->GetPhysicsAsset())
        return;
    FHitResult MeshHit = Hit;
    if (Hit.GetComponent() != Mesh && !Hit.TraceStart.Equals(Hit.TraceEnd))
    {
        FHitResult Refined;
        const FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyFXBone), false);
        if (Mesh->LineTraceComponent(Refined, Hit.TraceStart, Hit.TraceEnd, Params))
            MeshHit = Refined;
    }
    FName Bone = MeshHit.BoneName;
    if (Bone.IsNone() || !Mesh->GetBodyInstance(Bone)
        || (Bone != HitReactionRoot && !Mesh->BoneIsChildOf(Bone, HitReactionRoot)))
        Bone = FallbackHitBone;
    if (Mesh->GetBoneIndex(HitReactionRoot) == INDEX_NONE || !Mesh->GetBodyInstance(Bone))
    {
        Report(TEXT("Hit skipped: configure reaction bones and Physics Asset bodies."));
        return;
    }
    // One upper-body simulation avoids overlapping ancestor/child blend ownership.
    FPhysicalAnimationData Settings;
    Settings.bIsLocalSimulation = true;
    Settings.OrientationStrength = OrientationStrength;
    Settings.AngularVelocityStrength = 100.0f;
    PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(HitReactionRoot, Settings);
    if (ActiveReactions.IsEmpty())
        PreviousCollision = Mesh->GetCollisionEnabled();
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetAllBodiesBelowSimulatePhysics(HitReactionRoot, true, true);
    ActiveReactions.FindOrAdd(HitReactionRoot) = 0.0f;
    Mesh->SetAllBodiesBelowPhysicsBlendWeight(HitReactionRoot, MaxPhysicsBlend, false, true);
    Mesh->AddImpulseAtLocation(ShotDirection.GetSafeNormal() * HitImpulse, MeshHit.ImpactPoint, Bone);
    SetComponentTickEnabled(true);
}

void UEnemyImpactComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(Dt, TickType, TickFunction);
    if (bDead || !Mesh)
    {
        SetComponentTickEnabled(false);
        return;
    }
    for (auto It = ActiveReactions.CreateIterator(); It; ++It)
    {
        It.Value() += Dt;
        const float Alpha = FMath::Clamp(It.Value() / FMath::Max(HitDuration, 0.05f), 0.0f, 1.0f);
        Mesh->SetAllBodiesBelowPhysicsBlendWeight(It.Key(), MaxPhysicsBlend * (1.0f - Alpha), false, true);
        if (Alpha >= 1.0f)
        {
            Mesh->SetAllBodiesBelowSimulatePhysics(It.Key(), false, true);
            It.RemoveCurrent();
        }
    }
    SetComponentTickEnabled(!ActiveReactions.IsEmpty());
    if (ActiveReactions.IsEmpty())
        Mesh->SetCollisionEnabled(PreviousCollision);
}

void UEnemyImpactComponent::NotifyDeath()
{
    bDead = true;
    ActiveReactions.Empty();
    if (PhysicalAnimation)
        PhysicalAnimation->SetStrengthMultiplyer(0.0f);
    if (Mesh)
        Mesh->SetAllBodiesPhysicsBlendWeight(1.0f);
    SetComponentTickEnabled(false);
}

UProceduralMeshComponent* UEnemyImpactComponent::CapturePose()
{
    if (!Mesh || !Mesh->GetSkeletalMeshAsset())
    {
        Report(TEXT("Capture failed: no skeletal mesh assigned to the character mesh component."));
        return nullptr;
    }
    if (!Mesh->IsRegistered() || !FApp::CanEverRender())
    {
        Report(TEXT("Capture failed: a registered mesh and a rendering game session are required."));
        return nullptr;
    }
    const FSkeletalMeshRenderData* RenderData = Mesh->GetSkeletalMeshAsset()->GetResourceForRendering();
    if (!RenderData || RenderData->LODRenderData.IsEmpty())
    {
        Report(TEXT("Capture failed: LOD0 render data unavailable."));
        return nullptr;
    }
    const FSkeletalMeshLODRenderData& LOD = RenderData->LODRenderData[0];
    const FRawStaticIndexBuffer16or32Interface* Indices = LOD.MultiSizeIndexContainer.GetIndexBuffer();
    int64 TriangleCount = 0;
    for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
        TriangleCount += Section.NumTriangles;
    // The CPU buffers must be retained by the asset at cook time, never toggled here.
    if (TriangleCount > MaxSliceTriangles)
    {
        Report(FString::Printf(TEXT("Capture refused: %lld triangles exceeds Max Slice Triangles %d."),
            TriangleCount, MaxSliceTriangles));
        return nullptr;
    }
    if (!Indices || TriangleCount <= 0 || !LOD.SkinWeightVertexBuffer.GetNeedsCPUAccess()
        || LOD.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices() == 0)
    {
        Report(TEXT("Capture refused: LOD0 CPU data unavailable. Enable Allow CPU Access in skeletal mesh LOD settings and save/reload."));
        return nullptr;
    }
    TArray<FFinalSkinVertex> Skinned;
    Mesh->GetCPUSkinnedVertices(Skinned, 0);
    if (Skinned.Num() != static_cast<int32>(LOD.GetNumVertices()))
    {
        Report(TEXT("Capture failed: posed vertex count does not match LOD0."));
        return nullptr;
    }
    UProceduralMeshComponent* Result = NewObject<UProceduralMeshComponent>(GetOwner());
    Result->bUseComplexAsSimpleCollision = false;
    Result->SetWorldTransform(Mesh->GetComponentTransform());
    Result->RegisterComponent();
    Result->SetVisibility(false);
    int32 OutSection = 0;
    for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
    {
        if (Section.bDisabled || Section.NumTriangles == 0)
            continue;
        TArray<FVector> Positions, Normals;
        TArray<FVector2D> UV;
        TArray<FProcMeshTangent> Tangents;
        TArray<int32> Triangles;
        TMap<uint32, int32> Remap;
        for (uint32 I = 0; I < Section.NumTriangles * 3; ++I)
        {
            const uint32 SourceIndex = Indices->Get(Section.BaseIndex + I);
            if (!Skinned.IsValidIndex(SourceIndex))
            {
                Result->DestroyComponent();
                Report(TEXT("Capture failed: invalid LOD0 triangle index."));
                return nullptr;
            }
            int32* Existing = Remap.Find(SourceIndex);
            if (!Existing)
            {
                const FFinalSkinVertex& V = Skinned[SourceIndex];
                const int32 NewIndex = Positions.Add(FVector(V.Position));
                Normals.Add(FVector(V.TangentZ.ToFVector3f()));
                UV.Add(FVector2D(V.U, V.V));
                Tangents.Emplace(FVector(V.TangentX.ToFVector3f()), V.TangentZ.Vector.W < 128);
                Remap.Add(SourceIndex, NewIndex);
                Triangles.Add(NewIndex);
            }
            else
                Triangles.Add(*Existing);
        }
        Result->CreateMeshSection(OutSection, Positions, Triangles, Normals, UV, TArray<FColor>(), Tangents, false);
        Result->SetMaterial(OutSection++, Mesh->GetMaterial(Section.MaterialIndex));
    }
    if (OutSection == 0)
    {
        Result->DestroyComponent();
        Report(TEXT("Capture failed: no visible mesh sections."));
        return nullptr;
    }
    return Result;
}

void UEnemyImpactComponent::ConfigurePiece(UProceduralMeshComponent* Piece, const FVector& Velocity, const FVector& Impulse)
{
    TArray<FVector> Points;
    // Cheap collision proxy: extremal vertices in 26 directions, not all skin vertices.
    for (int32 X = -1; X <= 1; ++X)
    for (int32 Y = -1; Y <= 1; ++Y)
    for (int32 Z = -1; Z <= 1; ++Z)
    {
        if (X == 0 && Y == 0 && Z == 0) continue;
        const FVector Direction(X, Y, Z);
        double Best = -TNumericLimits<double>::Max();
        FVector BestPoint = FVector::ZeroVector;
        for (int32 S = 0; S < Piece->GetNumSections(); ++S)
        {
            const FProcMeshSection* Section = Piece->GetProcMeshSection(S);
            for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
            {
                const double Score = FVector::DotProduct(Vertex.Position, Direction);
                if (Score > Best) { Best = Score; BestPoint = Vertex.Position; }
            }
        }
        Points.AddUnique(BestPoint);
    }
    Piece->SetCollisionConvexMeshes({Points});
    Piece->SetCollisionObjectType(ECC_PhysicsBody);
    Piece->SetCollisionResponseToAllChannels(ECR_Ignore);
    Piece->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Piece->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Piece->SetVisibility(true);
    Piece->SetSimulatePhysics(true);
    Piece->SetPhysicsLinearVelocity(Velocity);
    Piece->AddImpulse(Impulse, NAME_None, true);
}

bool UEnemyImpactComponent::SliceCorpse(FVector Position, FVector Normal, FVector Impulse)
{
    if (!bEnableCorpseSlicing)
    {
        Report(TEXT("Cut rejected: Enable Corpse Slicing is disabled."));
        return false;
    }
    if (!bDead)
    {
        Report(TEXT("Cut rejected: target is alive. This prototype cuts corpses only."));
        return false;
    }
    if (!GetWorld() || Normal.IsNearlyZero()
        || Position.ContainsNaN() || Normal.ContainsNaN() || Impulse.ContainsNaN())
    {
        Report(TEXT("Cut rejected: invalid world or cutting plane."));
        return false;
    }
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastSliceTime < SliceCooldown)
    {
        Report(TEXT("Cut rejected: slice cooldown is active."));
        return false;
    }
    if (Pieces.Num() >= MaxPieces)
    {
        Report(TEXT("Cut rejected: Max Pieces limit reached."));
        return false;
    }
    Normal.Normalize();
    const bool bCaptured = Pieces.IsEmpty() && Mesh;
    UProceduralMeshComponent* Source = bCaptured ? CapturePose() : nullptr;
    if (bCaptured && !Source)
        return false; // Keep the specific capture error rather than overwriting it.
    if (!bCaptured)
    {
        for (UProceduralMeshComponent* Piece : Pieces)
        {
            const FBox Bounds = Piece->Bounds.GetBox();
            if (FMath::Abs(FVector::DotProduct(Bounds.GetCenter() - Position, Normal))
                < FVector::DotProduct(Bounds.GetExtent(), Normal.GetAbs()))
            {
                Source = Piece;
                break;
            }
        }
    }
    if (!Source)
    {
        Report(TEXT("No sliceable corpse mesh. Check CPU data or spawn the test target."));
        return false;
    }
    const FVector LocalNormal = (Source->GetComponentTransform().InverseTransformVectorNoScale(Normal)
        * Source->GetComponentScale()).GetSafeNormal();
    const FVector LocalPoint = Source->GetComponentTransform().InverseTransformPosition(Position);
    bool Positive = false, Negative = false;
    for (int32 S = 0; S < Source->GetNumSections(); ++S)
    for (const FProcMeshVertex& V : Source->GetProcMeshSection(S)->ProcVertexBuffer)
    {
        const double Side = FVector::DotProduct(V.Position - LocalPoint, LocalNormal);
        Positive |= Side > 0.1;
        Negative |= Side < -0.1;
    }
    if (!Positive || !Negative)
    {
        if (bCaptured) Source->DestroyComponent();
        Report(TEXT("Plane missed mesh interior; original corpse kept."));
        return false;
    }
    const FVector Velocity = bCaptured ? Mesh->GetPhysicsLinearVelocity() : Source->GetPhysicsLinearVelocity();
    Source->SetSimulatePhysics(false);
    UProceduralMeshComponent* Other = nullptr;
    UKismetProceduralMeshLibrary::SliceProceduralMesh(Source, Position, Normal, true, Other,
        EProcMeshSliceCapOption::CreateNewSectionForCap, CapMaterial);
    if (!Other)
    {
        if (bCaptured) Source->DestroyComponent();
        else Source->SetSimulatePhysics(true);
        Report(TEXT("Slice produced no second half."));
        return false;
    }
    if (bCaptured)
    {
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetVisibility(false, true);
        Pieces.Add(Source);
    }
    Pieces.Add(Other);
    ConfigurePiece(Source, Velocity, Impulse);
    ConfigurePiece(Other, Velocity, Impulse);
    LastSliceTime = Now;
    // The pieces share the corpse actor; keep it alive while they are visible.
    if (PieceLifetime > 0.0f) GetOwner()->SetLifeSpan(PieceLifetime);
    Report(FString::Printf(TEXT("Cut succeeded: %d rigid pieces (cosmetic, local only)."), Pieces.Num()));
    return true;
}

AEnemySliceTestActor::AEnemySliceTestActor()
{
    TestMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SliceTestMesh"));
    SetRootComponent(TestMesh);
    Impact = CreateDefaultSubobject<UEnemyImpactComponent>(TEXT("Impact"));
}

void AEnemySliceTestActor::BeginPlay()
{
    Super::BeginPlay();
    TArray<FVector> V, N;
    TArray<int32> T;
    TArray<FVector2D> UV;
    TArray<FProcMeshTangent> Tangents;
    UKismetProceduralMeshLibrary::GenerateBoxMesh(FVector(35, 35, 90), V, T, N, UV, Tangents);
    TestMesh->CreateMeshSection(0, V, T, N, UV, TArray<FColor>(), Tangents, false);
    TestMesh->bUseComplexAsSimpleCollision = false;
    TestMesh->SetCollisionConvexMeshes({V});
    TestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TestMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Impact->NotifyDeath();
    // The test source is intentionally procedural and needs no imported assets.
    Impact->Pieces.Add(TestMesh);
}
