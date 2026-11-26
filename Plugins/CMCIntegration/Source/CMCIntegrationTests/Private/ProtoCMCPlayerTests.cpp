#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    void Key(APlayerController* PC, FKey Value, EInputEvent Event)
    {
        auto Device = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PC->GetPlatformUserId());
        PC->InputKey(FInputKeyEventArgs(nullptr, Device, Value, Event, Event == IE_Released ? 0.f : 1.f, false, FPlatformTime::Cycles64()));
    }
    bool Bool(UObject* Object, FName Name)
    {
        auto Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name);
        return Property && Property->GetPropertyValue_InContainer(Object);
    }
    class FCheckProtoPlayer : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        double Start = FPlatformTime::Seconds();
        float StageTime = 0;
        int32 Stage = 0;
        float RunSpeed = 0;
        FVector Origin;
    public:
        explicit FCheckProtoPlayer(FAutomationTestBase* InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            if (FPlatformTime::Seconds() - Start > 120) { Test->AddError(TEXT("Integrated player test timeout")); return true; }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
            ACharacter* Pawn = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
            if (!Pawn || World->TimeSeconds < 2) return false;
            const float Now = World->TimeSeconds;
            if (Stage == 0)
            {
                UClass* Native = LoadClass<ACharacter>(nullptr, TEXT("/Script/ProtoProject.ProtoCharacter"));
                Test->TestTrue(TEXT("Player retains native ProtoCharacter contract"), Native && Pawn->IsA(Native));
                Test->TestTrue(TEXT("CMC enabled on existing gameplay player"), Bool(Pawn, TEXT("bUseCMCLocomotion")));
                Test->TestTrue(TEXT("CMC animation instance is active"), Pawn->GetMesh()->GetAnimClass() && Pawn->GetMesh()->GetAnimClass()->GetName().Contains(TEXT("SandboxCharacter_CMC_ABP")));
                TInlineComponentArray<UActorComponent*> Components(Pawn);
                int32 Inventory = 0, Equipment = 0, VendorInventory = 0;
                for (auto Component : Components)
                {
                    const FString Name = Component->GetClass()->GetName();
                    Inventory += Name == TEXT("InventoryGridComponent");
                    Equipment += Name == TEXT("EquipmentComponent");
                    VendorInventory += Name.Contains(TEXT("AC_InventoryManager"));
                }
                Test->TestEqual(TEXT("Existing inventory component retained exactly once"), Inventory, 1);
                Test->TestEqual(TEXT("Existing equipment component retained exactly once"), Equipment, 1);
                Test->TestEqual(TEXT("Vendor inventory not adopted"), VendorInventory, 0);
                Test->AddInfo(FString::Printf(TEXT("Camera view target: %s"), *GetNameSafe(PC->GetViewTarget())));
                PC->SetInputMode(FInputModeGameOnly()); PC->SetIgnoreMoveInput(false);
                Origin = Pawn->GetActorLocation();
                Key(PC, EKeys::W, IE_Pressed);
                StageTime = Now; Stage = 1; return false;
            }
            if (Now - StageTime < (Stage == 8 ? .15f : .8f)) return false;
            if (Stage == 1)
            {
                float Distance = FVector::Dist2D(Origin, Pawn->GetActorLocation());
                Test->AddInfo(FString::Printf(TEXT("W movement %.1f cm, speed %.1f, max %.1f"), Distance, Pawn->GetVelocity().Size2D(), Pawn->GetCharacterMovement()->MaxWalkSpeed));
                Test->TestTrue(TEXT("CMC movement responds to W"), Distance > 30);
                RunSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed;
                Key(PC, EKeys::LeftShift, IE_Pressed); Stage = 2;
            }
            else if (Stage == 2)
            {
                Test->TestTrue(TEXT("Existing sprint state responds to Shift"), Bool(Pawn, TEXT("bIsSprint")));
                Test->AddInfo(FString::Printf(TEXT("Run %.1f Sprint %.1f"), RunSpeed, Pawn->GetCharacterMovement()->MaxWalkSpeed));
                Test->TestTrue(TEXT("CMC sprint speed exceeds run speed"), Pawn->GetCharacterMovement()->MaxWalkSpeed > RunSpeed);
                Key(PC, EKeys::LeftShift, IE_Released); Key(PC, EKeys::W, IE_Released);
                Key(PC, EKeys::RightMouseButton, IE_Pressed); Stage = 3;
            }
            else if (Stage == 3)
            {
                Test->TestTrue(TEXT("Existing aim state retained"), Bool(Pawn, TEXT("bIsAiming")));
                auto Input = FindFProperty<FStructProperty>(Pawn->GetClass(), TEXT("CharacterInputState"));
                bool Aim = false;
                if (Input) for (TFieldIterator<FBoolProperty> It(Input->Struct); It; ++It)
                    if (It->GetAuthoredName() == TEXT("WantsToAim")) Aim = It->GetPropertyValue_InContainer(Input->ContainerPtrToValuePtr<void>(Pawn));
                Test->TestTrue(TEXT("Aim state reaches template locomotion"), Aim);
                Key(PC, EKeys::RightMouseButton, IE_Released);
                Key(PC, EKeys::I, IE_Pressed); Stage = 4;
            }
            else if (Stage == 4)
            {
                Key(PC, EKeys::I, IE_Released);
                auto Widget = FindFProperty<FObjectPropertyBase>(Pawn->GetClass(), TEXT("InventoryWidgetInstance"));
                Test->TestTrue(TEXT("I opens the original inventory widget"), Widget && Widget->GetObjectPropertyValue_InContainer(Pawn));
                Stage = 5;
            }
            else if (Stage == 5)
            {
                Key(PC, EKeys::I, IE_Pressed); Stage = 6;
            }
            else if (Stage == 6)
            {
                Key(PC, EKeys::I, IE_Released);
                Stage = 7;
            }
            else if (Stage == 7)
            {
                Key(PC, EKeys::SpaceBar, IE_Pressed); Stage = 8;
            }
            else if (Stage == 8)
            {
                Test->TestTrue(TEXT("CMC space input jumps"), Pawn->GetCharacterMovement()->IsFalling());
                Key(PC, EKeys::SpaceBar, IE_Released); Stage = 9;
            }
            else if (Stage == 9)
            {
                if (Pawn->GetCharacterMovement()->IsFalling()) return false;
                Key(PC, EKeys::C, IE_Pressed); Stage = 10;
            }
            else if (Stage == 10)
            {
                Key(PC, EKeys::C, IE_Released);
                Test->TestTrue(TEXT("CMC C input crouches"), Pawn->bIsCrouched);
                return true;
            }
            StageTime = Now; return false;
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtoCMCPlayerTest, "WITHOUT.ProtoCMC.Player.GameplayAndMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProtoCMCPlayerTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/CMCIntegration/Maps/L_ProtoCMCPlayerAcceptance"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckProtoPlayer(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
