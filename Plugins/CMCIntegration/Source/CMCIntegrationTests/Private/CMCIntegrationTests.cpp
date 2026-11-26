#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CMCIntegrationTests)

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    UActorComponent* FindTemplateComponent(AActor* Actor, const TCHAR* Name)
    {
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (UActorComponent* Component : Components)
            if (Component->GetClass()->GetName().Contains(Name)) return Component;
        return nullptr;
    }

    bool CallActorInput(UObject* Target, FName Name, AActor* Actor)
    {
        UFunction* Function = Target ? Target->FindFunction(Name) : nullptr;
        if (!Function) return false;
        FStructOnScope Params(Function);
        bool HasActor = false;
        for (TFieldIterator<FProperty> It(Function); It; ++It)
        {
            FProperty* Property = *It;
            if (!Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
                continue;
            if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
            {
                if (Actor->IsA(ObjectProperty->PropertyClass))
                {
                    ObjectProperty->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), Actor);
                    HasActor = true;
                }
            }
            if (FIntProperty* Count = CastField<FIntProperty>(Property))
                Count->SetPropertyValue_InContainer(Params.GetStructMemory(), 1);
        }
        if (!HasActor) return false;
        Target->ProcessEvent(Function, Params.GetStructMemory());
        return true;
    }

    void SendKey(APlayerController* PC, FKey Key, EInputEvent Event)
    {
        const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
        PC->InputKey(FInputKeyEventArgs(nullptr, Device, Key, Event, Event == IE_Released ? 0.0f : 1.0f, false, FPlatformTime::Cycles64()));
    }

    class FCheckCMCShooting : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TWeakObjectPtr<AActor> Weapon;
        double Started = 0;
        float StageTime = 0;
        int32 Stage = 0;
        int32 AmmoBefore = 0;
    public:
        explicit FCheckCMCShooting(FAutomationTestBase* InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            if (!Started) Started = FPlatformTime::Seconds();
            if (FPlatformTime::Seconds()-Started > 90)
            {
                Test->AddError(TEXT("Shooting test timed out."));
                return true;
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
            APawn* Pawn = PC ? PC->GetPawn() : nullptr;
            if (!Pawn || World->GetTimeSeconds() < 2) return false;
            const float Now = World->GetTimeSeconds();
            if (Stage == 0)
            {
                UActorComponent* Inventory = FindTemplateComponent(Pawn, TEXT("AC_InventoryManager"));
                UActorComponent* Equipment = FindTemplateComponent(Pawn, TEXT("AC_EquipmentManager"));
                UClass* Rifle = LoadClass<AActor>(nullptr, TEXT("/CMCIntegration/Blueprints/Inventory_v2/Ranged_Weapon_AR.Ranged_Weapon_AR_C"));
                if (!Inventory || !Equipment || !Rifle)
                {
                    Test->AddError(TEXT("Template inventory/equipment/rifle fixture unavailable."));
                    return true;
                }
                FActorSpawnParameters Spawn;
                Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                Weapon = World->SpawnActor<AActor>(Rifle, Pawn->GetActorLocation()+FVector(100,0,100), FRotator::ZeroRotator, Spawn);
                if (!Weapon.IsValid() || !CallActorInput(Inventory, TEXT("AddElementByActor"), Weapon.Get())
                    || !CallActorInput(Equipment, TEXT("TryEquipElement"), Weapon.Get()))
                {
                    Test->AddError(TEXT("Could not call template add/equip fixture APIs."));
                    return true;
                }
                PC->SetInputMode(FInputModeGameOnly());
                StageTime = Now;
                Stage = 1;
                return false;
            }
            AActor* Gun = Weapon.Get();
            if (!Gun)
            {
                Test->AddError(TEXT("Rifle fixture was destroyed during equip."));
                return true;
            }
            FIntProperty* Ammo = FindFProperty<FIntProperty>(Gun->GetClass(), TEXT("CurrentAmmo"));
            if (!Ammo)
            {
                Test->AddError(TEXT("Rifle ammo property missing."));
                return true;
            }
            if (Stage == 1)
            {
                if (Now-StageTime < 2) return false;
                UActorComponent* Equipment = FindTemplateComponent(Pawn, TEXT("AC_EquipmentManager"));
                FObjectPropertyBase* Current = Equipment ? FindFProperty<FObjectPropertyBase>(Equipment->GetClass(), TEXT("CurrentWeapon")) : nullptr;
                if (!Current || Current->GetObjectPropertyValue_InContainer(Equipment) != Gun)
                {
                    Test->AddError(TEXT("Template equip API did not equip the test rifle."));
                    return true;
                }
                // A disposable PIE loadout; game ammo rules are not changed.
                Ammo->SetPropertyValue_InContainer(Gun, 10);
                AmmoBefore = 10;
                SendKey(PC, EKeys::RightMouseButton, IE_Pressed);
                StageTime = Now; Stage = 2;
                return false;
            }
            if (Stage == 2)
            {
                if (Now-StageTime < .5f) return false;
                SendKey(PC, EKeys::LeftMouseButton, IE_Pressed);
                StageTime = Now; Stage = 3;
                return false;
            }
            if (Now-StageTime < .5f) return false;
            SendKey(PC, EKeys::LeftMouseButton, IE_Released);
            SendKey(PC, EKeys::RightMouseButton, IE_Released);
            const int32 AmmoAfter = Ammo->GetPropertyValue_InContainer(Gun);
            Test->AddInfo(FString::Printf(TEXT("Rifle ammo before=%d after=%d"), AmmoBefore, AmmoAfter));
            Test->TestTrue(TEXT("Mouse input fires the equipped rifle"), AmmoAfter < AmmoBefore);
            return true;
        }
    };

    class FCheckCMCMovement : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        double Start = 0;
        double MoveStarted = 0;
        FVector InitialPosition = FVector::ZeroVector;
    public:
        explicit FCheckCMCMovement(FAutomationTestBase* InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            if (!Start) Start = FPlatformTime::Seconds();
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
            APawn* Pawn = PC ? PC->GetPawn() : nullptr;
            if (!Pawn)
            {
                if (FPlatformTime::Seconds()-Start < 15) return false;
                Test->AddError(TEXT("CMC test pawn was not possessed within 15 seconds."));
                return true;
            }
            if (FPlatformTime::Seconds()-Start > 60)
            {
                Test->AddError(TEXT("PIE smoke test timed out while waiting for simulation ticks."));
                return true;
            }
            // Asset indexing can stall a frame for seconds; use simulation time,
            // not wall-clock time, to hold the key for an actual movement interval.
            if (World->GetTimeSeconds() < 2.0f) return false;
            if (!MoveStarted)
            {
                Test->TestTrue(TEXT("Dedicated CMC pawn is possessed"),
                    Pawn->GetClass()->GetPathName().Contains(TEXT("BP_CMCIntegrationCharacter")));
                InitialPosition = Pawn->GetActorLocation();
                MoveStarted = World->GetTimeSeconds();
                PC->SetIgnoreMoveInput(false);
                PC->SetInputMode(FInputModeGameOnly());
                const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
                PC->InputKey(FInputKeyEventArgs(nullptr, Device, EKeys::W, IE_Pressed, 1.0f, false, FPlatformTime::Cycles64()));
                Test->AddInfo(FString::Printf(TEXT("Pawn=%s Input=%s Start=%s"),
                    *Pawn->GetClass()->GetName(), *GetNameSafe(PC->PlayerInput), *InitialPosition.ToString()));
                return false;
            }
            if (World->GetTimeSeconds()-MoveStarted < 1.5) return false;
            const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
            Test->AddInfo(FString::Printf(TEXT("W held=%d Velocity=%s PendingInput=%s"),
                PC->IsInputKeyDown(EKeys::W), *Pawn->GetVelocity().ToString(), *Pawn->GetPendingMovementInputVector().ToString()));
            if (ACharacter* Character = Cast<ACharacter>(Pawn))
                Test->AddInfo(FString::Printf(TEXT("MovementMode=%d Acceleration=%s"),
                    static_cast<int32>(Character->GetCharacterMovement()->MovementMode),
                    *Character->GetCharacterMovement()->GetCurrentAcceleration().ToString()));
            PC->InputKey(FInputKeyEventArgs(nullptr, Device, EKeys::W, IE_Released, 0.0f, false, FPlatformTime::Cycles64()));
            const float Distance = FVector::Dist2D(InitialPosition, Pawn->GetActorLocation());
            Test->AddInfo(FString::Printf(TEXT("CMC W-key movement distance: %.1f cm"), Distance));
            Test->TestTrue(TEXT("Template input moves the character"), Distance > 30.0f);
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMCIntegrationSmokeTest, "WITHOUT.CMCIntegration.PIE.Movement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMCIntegrationSmokeTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/CMCIntegration/Maps/L_CMCIntegrationTest"));
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Dedicated test map loads"), World)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckCMCMovement(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCMCIntegrationShootingTest, "WITHOUT.CMCIntegration.PIE.Shooting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCMCIntegrationShootingTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/CMCIntegration/Maps/L_CMCIntegrationTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckCMCShooting(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
