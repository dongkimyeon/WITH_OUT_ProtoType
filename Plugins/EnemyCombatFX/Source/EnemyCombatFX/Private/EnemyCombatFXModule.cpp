#include "EnemyImpactComponent.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "EnemyFXAimTrace.h"

class FEnemyCombatFXModule : public IModuleInterface
{
    TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> CutCommand;
    TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> SpawnCommand;
    TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> HitCommand;

    static void Message(const FString& Text)
    {
        UE_LOG(LogTemp, Display, TEXT("EnemyFX: %s"), *Text);
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, Text);
    }
    static void AimAction(const TArray<FString>& Args, UWorld* World, bool bCut)
    {
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        if (!PC)
        {
            Message(TEXT("No player controller. Run this command in PIE or Standalone."));
            return;
        }
        FVector Origin; FRotator Rotation;
        PC->GetPlayerViewPoint(Origin, Rotation);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyFXDebug), false);
        Params.AddIgnoredActor(PC->GetPawn());
        FHitResult Hit;
        if (!EnemyFXAimTrace::Trace(World, Hit, Origin, Origin + Rotation.Vector()*5000.0f, Params))
        {
            Message(TEXT("EnemyFX: no target under crosshair."));
            return;
        }
        UEnemyImpactComponent* FX = Hit.GetActor() ? Hit.GetActor()->FindComponentByClass<UEnemyImpactComponent>() : nullptr;
        if (!FX)
        {
            Message(FString::Printf(TEXT("Target %s / %s has no Enemy Impact component."),
                *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent())));
            return;
        }
        if (!bCut)
        {
            FX->PlayHitReaction(Hit, Rotation.Vector());
            Message(TEXT("EnemyFX: hit reaction requested (no damage). Requires a living skeletal target."));
            return;
        }
        float Roll = 0.0f;
        if ((!Args.IsEmpty() && !LexTryParseString(Roll, *Args[0])) || !FMath::IsFinite(Roll))
        {
            Message(TEXT("Invalid angle. Usage: enemyfx.Cut 45"));
            return;
        }
        const FVector Normal = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z).RotateAngleAxis(Roll, Rotation.Vector());
        DrawDebugSolidPlane(World, FPlane(Hit.ImpactPoint, Normal), Hit.ImpactPoint, 70, FColor(255, 100, 0, 100), false, 2);
        FX->SliceCorpse(Hit.ImpactPoint, Normal, Rotation.Vector()*80.0f);
        Message(FString::Printf(TEXT("%s: %s"), *GetNameSafe(Hit.GetActor()), *FX->LastResult));
    }
public:
    virtual void StartupModule() override
    {
        CutCommand = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(TEXT("enemyfx.Cut"),
            TEXT("Aim at a corpse, then enemyfx.Cut [angle degrees]. Cosmetic local plane slice."),
            FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { AimAction(A,W,true); }));
        HitCommand = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(TEXT("enemyfx.Hit"),
            TEXT("Aim at a living enemy. Test physical hit reaction without damage."),
            FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { AimAction(A,W,false); }));
        SpawnCommand = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(TEXT("enemyfx.SpawnTest"),
            TEXT("Spawn an asset-free procedural slicing box in front of the player."),
            FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W)
            {
                APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
                if (!PC) return;
                FVector P; FRotator R;
                PC->GetPlayerViewPoint(P,R);
                W->SpawnActor<AEnemySliceTestActor>(P+R.Vector()*250, FRotator::ZeroRotator);
                Message(TEXT("EnemyFX: test target spawned. Aim at its center, enemyfx.Cut 0 / 45 / 90."));
            }));
    }
    virtual void ShutdownModule() override
    {
        CutCommand.Reset(); SpawnCommand.Reset(); HitCommand.Reset();
    }
};
IMPLEMENT_MODULE(FEnemyCombatFXModule, EnemyCombatFX)
