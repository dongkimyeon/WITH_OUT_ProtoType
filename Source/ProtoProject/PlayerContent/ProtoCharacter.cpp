#include "ProtoCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenWidget.h"
#include "InventoryGridComponent.h"
#include "EquipmentComponent.h"
#include "QuickSlotComponent.h"
#include "RadialQuickSlotWidget.h"
#include "Item/ConsumableItemData.h"
#include "ContainerScreenWidget.h"
#include "Item/ItemDataBase.h"
#include "Item/WeaponItemData.h"
#include "Item/StorageContainer.h"
#include "PlayerDefalutUI.h"
#include "PlayerStatusComponent.h"
#include "InputCoreTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "weapon/WeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "../LevelChange/LevelChanger.h"
#include "../LevelChange/LevelChangeSelectWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "../Network/ProtoNetClientSubsystem.h"

AProtoCharacter::AProtoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    InventoryComponent = CreateDefaultSubobject<UInventoryGridComponent>(TEXT("InventoryComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
    QuickSlotComponent = CreateDefaultSubobject<UQuickSlotComponent>(TEXT("QuickSlotComponent"));
    StatusComponent = CreateDefaultSubobject<UPlayerStatusComponent>(TEXT("StatusComponent"));
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

    static ConstructorHelpers::FObjectFinder<UAnimMontage> RifleReloadMontageFinder(TEXT("/Game/Blueprint/AM_Player_Upper.AM_Player_Upper"));
    if (RifleReloadMontageFinder.Succeeded())
    {
        RifleReloadMontage = RifleReloadMontageFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PickupAnimationFinder(TEXT("/Game/testasset/Picking_Up.Picking_Up"));
    if (PickupAnimationFinder.Succeeded())
    {
        PickupAnimation = PickupAnimationFinder.Object;
    }

}
void AProtoCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (Controller)
    {
        const float NormalizedPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch);
        AimPitch = FMath::Clamp(NormalizedPitch, -30.0f, 30.0f);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            12003,
            0.0f,
            SwappingAlpha ? FColor::Green : FColor::Red,
            FString::Printf(TEXT("SwappingAlpha: %s | Swapping: %.2f | WeaponType: %d"),
                SwappingAlpha ? TEXT("TRUE") : TEXT("FALSE"),
                Swapping,
                static_cast<int32>(CurrentWeaponType)));
    }

    if (Swapping > 0.0f)
    {
        Swapping = FMath::Max(0.0f, Swapping - DeltaTime);
        SwappingAlpha = false;

        if (Swapping <= 0.0f)
        {
            FinishWeaponSwap();
        }
    }

    if (bHasWeapon && Swapping <= 0.0f && CurrentWeapon && GetMesh())
    {
        static const FName RightHandBoneName(TEXT("hand_r"));

        FTransform LeftHandSocketTransform;
        if (CurrentWeapon->GetLeftHandSocketTransform(LeftHandSocketTransform))
        {
            const FVector LeftHandWorldLocation = LeftHandSocketTransform.GetLocation();

            FVector OutPosition;
            FRotator OutRotation;
            GetMesh()->TransformToBoneSpace(
                RightHandBoneName,
                LeftHandWorldLocation,
                LeftHandSocketTransform.Rotator(),
                OutPosition,
                OutRotation);

            LeftHandTransform = FTransform(OutRotation, OutPosition, FVector::OneVector);

            if (GEngine)
            {
                const FVector DebugLeftHandLocation = LeftHandTransform.GetLocation();
                const FRotator DebugLeftHandRotation = LeftHandTransform.Rotator();
                GEngine->AddOnScreenDebugMessage(
                    12004,
                    0.0f,
                    FColor::Orange,
                    FString::Printf(TEXT("LeftHandTransform Loc X %.1f Y %.1f Z %.1f | Rot P %.1f Y %.1f R %.1f"),
                        DebugLeftHandLocation.X,
                        DebugLeftHandLocation.Y,
                        DebugLeftHandLocation.Z,
                        DebugLeftHandRotation.Pitch,
                        DebugLeftHandRotation.Yaw,
                        DebugLeftHandRotation.Roll));
            }

            if (bDebugLeftHandIK)
            {
                const float DebugSize = FMath::Max(LeftHandIKDebugDrawSize, 24.0f);
                const FVector DebugTopLocation = LeftHandWorldLocation + FVector(0.0f, 0.0f, DebugSize * 4.0f);

                DrawDebugSphere(
                    GetWorld(),
                    LeftHandWorldLocation,
                    DebugSize,
                    24,
                    FColor::Magenta,
                    false,
                    0.0f,
                    0,
                    4.0f);

                DrawDebugBox(
                    GetWorld(),
                    LeftHandWorldLocation,
                    FVector(DebugSize * 0.75f),
                    FColor::Cyan,
                    false,
                    0.0f,
                    0,
                    3.0f);

                DrawDebugLine(
                    GetWorld(),
                    LeftHandWorldLocation,
                    DebugTopLocation,
                    FColor::Magenta,
                    false,
                    0.0f,
                    0,
                    5.0f);

                DrawDebugString(
                    GetWorld(),
                    DebugTopLocation,
                    TEXT("LEFT SOCKET"),
                    nullptr,
                    FColor::Magenta,
                    0.0f,
                    true);

                DrawDebugCoordinateSystem(
                    GetWorld(),
                    LeftHandWorldLocation,
                    LeftHandSocketTransform.Rotator(),
                    DebugSize * 3.0f,
                    false,
                    0.0f,
                    0,
                    3.0f);

                const FVector HandSpaceTargetWorldLocation = GetMesh()->GetSocketTransform(RightHandBoneName, RTS_World).TransformPosition(OutPosition);
                DrawDebugSphere(
                    GetWorld(),
                    HandSpaceTargetWorldLocation,
                    LeftHandIKDebugDrawSize * 0.75f,
                    12,
                    FColor::Yellow,
                    false,
                    0.0f,
                    0,
                    1.5f);

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(
                        12001,
                        0.2f,
                        FColor::Cyan,
                        FString::Printf(TEXT("LeftHandSocket World: X %.1f / Y %.1f / Z %.1f | BoneSpace: X %.1f / Y %.1f / Z %.1f"),
                            LeftHandWorldLocation.X,
                            LeftHandWorldLocation.Y,
                            LeftHandWorldLocation.Z,
                            OutPosition.X,
                            OutPosition.Y,
                            OutPosition.Z));
                }
            }
        }
        else if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(12001, 0.2f, FColor::Red, TEXT("LeftHandSocket Missing"));
        }
    }
    UpdateStamina(DeltaTime);

    /*-------------------
     네트워킹: 위치 브로드캐스트 (NetSyncInterval 간격으로 주기 전송)
    -------------------*/
    if (IsLocallyControlled())
    {
        NetSyncTimer -= DeltaTime;
        if (NetSyncTimer <= 0.0f)
        {
            NetSyncTimer = NetSyncInterval;
            if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
            {
                if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
                {
                    NetClient->SendMoveInput(GetActorLocation(), GetControlRotation());
                }
            }
        }
    }

    if (bIsReloading)
    {
        SwappingAlpha = false;
    }
}

void AProtoCharacter::BeginPlay()
{
    Super::BeginPlay();

    StopAim();
    StopSprint();

    if (EquipmentComponent)
    {
        EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &AProtoCharacter::HandleEquipmentChanged);
    }

    /*-------------------
     네트워킹: 서버 접속 프롬프트 표시 (로컬 플레이어만)
    -------------------*/
    if (IsLocallyControlled())
    {
        if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
        {
            if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
            {
                NetClient->ShowConnectPrompt();
            }
        }
    }

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
        {
            AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AProtoCharacter::HandleMontageNotifyBegin);
            AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &AProtoCharacter::HandleMontageEnded);
        }
    }
    if (InventoryComponent)
    {
        if (TestArmor) InventoryComponent->AddItem(TestArmor);
        if (TestRifle) InventoryComponent->AddItem(TestRifle);
        if (TestBandage) InventoryComponent->AddItem(TestBandage);
        if (TestBandage) InventoryComponent->AddItem(TestBandage);
    }

    // Only the locally-controlled player's own HUD should go on screen; this
    // BeginPlay also runs for remote players spawned by
    // UProtoNetClientSubsystem (see ProtoNetClientSubsystem.cpp), which have
    // no local controller.
    if (IsLocallyControlled())
    {
        if (DefaultUIClass)
        {
            DefaultUI = CreateWidget<UPlayerDefalutUI>(GetWorld(), DefaultUIClass);
            if (DefaultUI)
            {
                DefaultUI->AddToViewport();
            }
        }
        else if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("DefaultUIClass is NULL"));
        }
    }
}

void AProtoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Look);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AProtoCharacter::Sprint);
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AProtoCharacter::Sprint);
        EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AProtoCharacter::ToggleInventory);

        if (InteractAction)
        {
            EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AProtoCharacter::Interact);
        }
        else if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("InteractAction is NULL"));
        }
    }

    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AProtoCharacter::SetWeaponSlot1);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AProtoCharacter::SetWeaponSlot2);
    PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AProtoCharacter::SetWeaponTypeNone);
    PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AProtoCharacter::OnQuickSlotKeyPressed);
    PlayerInputComponent->BindKey(EKeys::Four, IE_Released, this, &AProtoCharacter::OnQuickSlotKeyReleased);
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AProtoCharacter::StartFireWeapon);
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AProtoCharacter::StopFireWeapon);
    PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AProtoCharacter::ReloadWeapon);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AProtoCharacter::StartSprint);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AProtoCharacter::StopSprint);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AProtoCharacter::StartAim);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AProtoCharacter::StopAim);

    PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AProtoCharacter::DebugDecreaseHealth);
    PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AProtoCharacter::DebugDecreaseHunger);
    PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AProtoCharacter::DebugDecreaseThirst);
    PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AProtoCharacter::DebugIncreaseInfection);
    PlayerInputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &AProtoCharacter::DebugDecreaseStamina);
}

void AProtoCharacter::DebugDecreaseHealth()
{
    if (StatusComponent) StatusComponent->SetHealth(StatusComponent->GetHealth() - 5.0f);
}

void AProtoCharacter::DebugDecreaseHunger()
{
    if (StatusComponent) StatusComponent->SetHunger(StatusComponent->GetHunger() - 5.0f);
}

void AProtoCharacter::DebugDecreaseThirst()
{
    if (StatusComponent) StatusComponent->SetThirst(StatusComponent->GetThirst() - 5.0f);
}

void AProtoCharacter::DebugIncreaseInfection()
{
    if (StatusComponent) StatusComponent->SetInfection(StatusComponent->GetInfection() + 5.0f);
}

void AProtoCharacter::DebugDecreaseStamina()
{
    if (StatusComponent) StatusComponent->SetStamina(StatusComponent->GetStamina() - 5.0f);
}

void AProtoCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        AddMovementInput(ForwardDirection, MovementVector.X);
        AddMovementInput(RightDirection, MovementVector.Y);
    }
}

void AProtoCharacter::Look(const FInputActionValue& Value)
{
    if (bIsInvetoryOpened)
    {
        return;
    }

    const FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void AProtoCharacter::Sprint(const FInputActionValue& Value)
{
    if (Value.Get<bool>())
    {
        StartSprint();
    }
    else
    {
        StopSprint();
    }
}

void AProtoCharacter::StartSprint()
{
    if (StatusComponent && StatusComponent->GetStamina() <= 0.0f) return;

    bIsSprint = true;
    GetCharacterMovement()->MaxWalkSpeed = SprintWalkSpeed;
}

void AProtoCharacter::StopSprint()
{
    bIsSprint = false;
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void AProtoCharacter::UpdateStamina(float DeltaTime)
{
    if (!StatusComponent) return;

    const bool bIsMovingWhileSprint = bIsSprint && GetVelocity().SizeSquared2D() > 1.0f;

    if (bIsMovingWhileSprint)
    {
        StatusComponent->SetStamina(StatusComponent->GetStamina() - StaminaDrainRate * DeltaTime);

        if (StatusComponent->GetStamina() <= 0.0f)
        {
            bStaminaDepleted = true;
            StaminaRegenTimer = StaminaRegenDelay;
            StopSprint();
        }
        return;
    }

    if (StatusComponent->GetStamina() >= StatusComponent->GetMaxStamina()) return;

    if (bStaminaDepleted)
    {
        StaminaRegenTimer -= DeltaTime;
        if (StaminaRegenTimer > 0.0f) return;
    }

    StatusComponent->SetStamina(StatusComponent->GetStamina() + StaminaRegenRate * DeltaTime);
    if (StatusComponent->GetStamina() >= StatusComponent->GetMaxStamina())
    {
        bStaminaDepleted = false;
    }
}

void AProtoCharacter::StartAim()
{
    bIsAiming = true;

    if (USpringArmComponent* SpringArm = FindComponentByClass<USpringArmComponent>())
    {
        SpringArm->TargetArmLength = AimCameraArmLength;
    }

    if (UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
    {
        Camera->SetRelativeLocation(AimCameraRelativeLocation);
    }

    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AProtoCharacter::StopAim()
{
    bIsAiming = false;

    if (USpringArmComponent* SpringArm = FindComponentByClass<USpringArmComponent>())
    {
        SpringArm->TargetArmLength = DefaultCameraArmLength;
    }

    if (UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
    {
        Camera->SetRelativeLocation(DefaultCameraRelativeLocation);
    }

    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AProtoCharacter::Interact(const FInputActionValue& Value)
{
    if (bIsContainerOpened)
    {
        CloseContainerScreen();
        return;
    }

    if (bIsLevelChangeOpened)
    {
        CloseLevelChangeScreen();
        return;
    }

    if (NearbyInteractables.IsEmpty()) return;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC) return;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const FVector TraceStart = CamLoc + CamRot.Vector() * 400.f;
    const FVector TraceEnd   = CamLoc + CamRot.Vector() * 700.f;
    GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
    DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green, false, 2.f);

    AActor* HitActor = Hit.GetActor();
    if (!IsValid(HitActor) || !NearbyInteractables.Contains(HitActor)) return;

    if (HitActor->Implements<UInteractable>() && IInteractable::Execute_CanInteract(HitActor, this))
    {
        IInteractable::Execute_OnInteract(HitActor, this);
    }
}

void AProtoCharacter::PlayPickupAnimationIfUnarmed()
{
    if (CurrentWeaponType != EWeaponType::None || !PickupAnimation || !GetMesh())
    {
        return;
    }

    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->PlaySlotAnimationAsDynamicMontage(PickupAnimation, TEXT("DefaultSlot"));
    }
}

void AProtoCharacter::OnInteractableEnter(AActor* Actor)
{
    if (!Actor) return;
    NearbyInteractables.AddUnique(Actor);
    if (DefaultUI && Actor->Implements<UInteractable>())
    {
        DefaultUI->AddInteractPrompt(Actor, IInteractable::Execute_GetInteractPrompt(Actor));
    }
}

void AProtoCharacter::OnInteractableExit(AActor* Actor)
{
    if (!Actor) return;
    NearbyInteractables.Remove(Actor);
    if (DefaultUI) DefaultUI->RemoveInteractPrompt(Actor);

    if (bIsContainerOpened) CloseContainerScreen();
    if (bIsLevelChangeOpened) CloseLevelChangeScreen();
}

void AProtoCharacter::OpenContainerScreen(AStorageContainer* Container)
{
    if (!ContainerWidgetClass) return;

    if (ContainerWidgetInstance == nullptr)
    {
        ContainerWidgetInstance = CreateWidget<UContainerScreenWidget>(GetWorld(), ContainerWidgetClass);
        GEngine->AddOnScreenDebugMessage(-1,5.f,FColor::Cyan,"OpenContainerScreen");
    }

    if (!ContainerWidgetInstance) return;

    ContainerWidgetInstance->InitializeScreen(InventoryComponent, Container->ContainerInventory);
    ContainerWidgetInstance->AddToViewport();
    bIsContainerOpened = true;
    bIsInvetoryOpened = true;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(true);
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(ContainerWidgetInstance->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
        GEngine->AddOnScreenDebugMessage(-1,5.f,FColor::Cyan,"MouseSetting");
    }
}

void AProtoCharacter::CloseContainerScreen()
{
    if (ContainerWidgetInstance) ContainerWidgetInstance->RemoveFromParent();
    bIsContainerOpened = false;
    bIsInvetoryOpened = false;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AProtoCharacter::OpenLevelChangeScreen(ALevelChanger* Changer)
{
    if (!LevelChangeWidgetClass) return;

    if (LevelChangeWidgetInstance == nullptr)
    {
        LevelChangeWidgetInstance = CreateWidget<ULevelChangeSelectWidget>(GetWorld(), LevelChangeWidgetClass);
    }

    if (!LevelChangeWidgetInstance) return;

    LevelChangeWidgetInstance->AddToViewport();
    bIsLevelChangeOpened = true;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(true);
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(LevelChangeWidgetInstance->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
    }
}

void AProtoCharacter::CloseLevelChangeScreen()
{
    if (LevelChangeWidgetInstance) LevelChangeWidgetInstance->RemoveFromParent();
    bIsLevelChangeOpened = false;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AProtoCharacter::ToggleInventory(const FInputActionValue& Value)
{
    // 상자가 열려있는 동안엔 인벤토리를 별도로 열 수 없다 (상자 화면 안에서 인벤토리 그리드를 같이 보여주고 있음).
    if (bIsContainerOpened) return;

    if (InventoryWidgetInstance == nullptr && InventoryWidgetClass != nullptr)
    {
        InventoryWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), InventoryWidgetClass);
    }

    if (InventoryWidgetInstance != nullptr)
    {
        APlayerController* PlayerController = Cast<APlayerController>(Controller);
        if (InventoryWidgetInstance->IsInViewport())
        {
            bIsInvetoryOpened = false;
            InventoryWidgetInstance->RemoveFromParent();
            if (PlayerController)
            {
                PlayerController->SetShowMouseCursor(false);
                FInputModeGameOnly InputMode;
                PlayerController->SetInputMode(InputMode);
            }
        }
        else
        {
            InventoryWidgetInstance->AddToViewport();
            bIsInvetoryOpened = true;
            if (UInventoryScreenWidget* InvUI = Cast<UInventoryScreenWidget>(InventoryWidgetInstance))
            {
                InvUI->InitializeGrid(InventoryComponent);
                InvUI->InitializeEquipment(EquipmentComponent);
                InvUI->InitializeQuickSlots(QuickSlotComponent);
            }
            if (PlayerController)
            {
                PlayerController->SetShowMouseCursor(true);
                FInputModeGameAndUI InputMode;
                InputMode.SetWidgetToFocus(InventoryWidgetInstance->TakeWidget());
                InputMode.SetHideCursorDuringCapture(false);
                PlayerController->SetInputMode(InputMode);
                InventoryWidgetInstance->SetUserFocus(PlayerController);
            }
        }
    }
}

void AProtoCharacter::SetWeaponTypeNone()
{
    if (Swapping > 0.0f || CurrentWeaponType == EWeaponType::None)
    {
        return;
    }

    CurrentWeapon = GetWeaponByType(CurrentWeaponType);
    BeginWeaponSwap(EWeaponType::None);
}

void AProtoCharacter::SetWeaponSlot1()
{
    SetWeaponFromSlot(EEquipmentSlot::Weapon1);
}

void AProtoCharacter::SetWeaponSlot2()
{
    SetWeaponFromSlot(EEquipmentSlot::Weapon2);
}

void AProtoCharacter::SetWeaponFromSlot(EEquipmentSlot Slot)
{
    if (Swapping > 0.0f)
    {
        return;
    }

    if (CurrentWeaponType != EWeaponType::None)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, TEXT("Store current weapon first"));
        }
        return;
    }

    AWeaponBase* const* Found = EquippedWeaponActors.Find(Slot);
    AWeaponBase* SlotWeapon = Found ? *Found : nullptr;
    if (!SlotWeapon)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, TEXT("No weapon in slot"));
        }
        return;
    }

    CurrentWeapon = SlotWeapon;
    BeginWeaponSwap(SlotWeapon->WeaponType, SlotWeapon);
}

AWeaponBase* AProtoCharacter::GetWeaponByType(EWeaponType WeaponType) const
{
    switch (WeaponType)
    {
    case EWeaponType::Rifle:
        return CurrentRifle;
    case EWeaponType::Pistol:
        return CurrentPistol;
    default:
        return nullptr;
    }
}
void AProtoCharacter::BeginWeaponSwap(EWeaponType TargetWeaponType, AWeaponBase* TargetWeaponActor)
{
    StopFireWeapon();

    if (Swapping > 0.0f)
    {
        return;
    }

    if (CurrentWeaponType == TargetWeaponType)
    {
        return;
    }

    AWeaponBase* SwapWeapon = TargetWeaponType == EWeaponType::None
        ? CurrentWeapon
        : (TargetWeaponActor ? TargetWeaponActor : GetWeaponByType(TargetWeaponType));

    if (!SwapWeapon)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, TEXT("No weapon in slot"));
        }
        return;
    }

    CurrentWeapon = SwapWeapon;

    const EWeaponType PreviousWeaponType = CurrentWeaponType;
    SwapFromWeaponType = PreviousWeaponType;

    PendingWeaponType = TargetWeaponType;
    CurrentWeaponType = TargetWeaponType;
    bHasWeapon = CurrentWeaponType != EWeaponType::None;

    /*-------------------
     네트워킹: 무기 장착 브로드캐스트
    -------------------*/
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendWeaponEquip(static_cast<uint8>(TargetWeaponType));
        }
    }

    const bool bIsEquippingWeapon = TargetWeaponType != EWeaponType::None;
    Swapping = bIsEquippingWeapon ? SwapWeapon->EquipSwapTime : SwapWeapon->UnequipSwapTime;
    Swapping = FMath::Max(0.0f, Swapping);
    SwappingAlpha = false;

    if (WeaponSwapMontage)
    {
        if (TargetWeaponType == EWeaponType::None)
        {
            if (PreviousWeaponType == EWeaponType::Rifle)
            {
                PlayAnimMontage(WeaponSwapMontage, 1.0f, RifleToHandSectionName);
            }
            else if (PreviousWeaponType == EWeaponType::Pistol)
            {
                PlayAnimMontage(WeaponSwapMontage, 1.0f, PistolToHandSectionName);
            }
        }
        else if (PreviousWeaponType == EWeaponType::None && TargetWeaponType == EWeaponType::Rifle)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToRifleSectionName);
        }
        else if (PreviousWeaponType == EWeaponType::None && TargetWeaponType == EWeaponType::Pistol)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToPistolSectionName);
        }
    }
    if (Swapping <= 0.0f)
    {
        FinishWeaponSwap();
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan, TEXT("Weapon Swapping"));
    }
}

void AProtoCharacter::FinishWeaponSwap()
{
    Swapping = 0.0f;
    SwappingAlpha = true;
    CurrentWeaponType = PendingWeaponType;

    if (CurrentWeapon)
    {
        Joint = CurrentWeapon->LeftHandJointTarget;
    }

    if (CurrentWeaponType == EWeaponType::None)
    {
        bHasWeapon = false;
        AttachCurrentWeaponToSocket(SwapFromWeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Weapon Stored"));
        }
        return;
    }

    if (CurrentWeaponType == EWeaponType::Rifle)
    {
        bHasWeapon = true;
        AttachCurrentWeaponToSocket(TEXT("WeaponSocket"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("Rifle Equipped"));
        }
        return;
    }

    if (CurrentWeaponType == EWeaponType::Pistol)
    {
        bHasWeapon = true;
        AttachCurrentWeaponToSocket(TEXT("PistolSocket"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("Pistol Equipped"));
        }
    }
}
void AProtoCharacter::StartFireWeapon()
{
    FireWeapon();

    if (!CurrentWeapon || !CurrentWeapon->bAutomatic || CurrentWeapon->FireRate <= 0.0f)
    {
        return;
    }

    const float FireInterval = 1.0f / CurrentWeapon->FireRate;
    GetWorldTimerManager().SetTimer(AutoFireTimerHandle, this, &AProtoCharacter::FireWeapon, FireInterval, true, FireInterval);
}

void AProtoCharacter::StopFireWeapon()
{
    GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);
}

void AProtoCharacter::FireWeapon()
{
    if (!CurrentWeapon)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("No CurrentWeapon"));
        }
        return;
    }
    CurrentWeapon->Fire();
    ApplyWeaponRecoil();
}

void AProtoCharacter::ApplyWeaponRecoil()
{
    if (!IsLocallyControlled() || !Controller)
    {
        return;
    }

    float PitchKick = RifleRecoilPitch;
    float YawKick = FMath::RandRange(-RifleRecoilYaw, RifleRecoilYaw);
    TSubclassOf<UCameraShakeBase> ShakeClass = RifleFireCameraShakeClass;

    if (CurrentWeaponType == EWeaponType::Pistol)
    {
        PitchKick = PistolRecoilPitch;
        YawKick = FMath::RandRange(-PistolRecoilYaw, PistolRecoilYaw);
        ShakeClass = PistolFireCameraShakeClass;
    }

    AddControllerPitchInput(-PitchKick);
    AddControllerYawInput(YawKick);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (PC->PlayerCameraManager && ShakeClass)
        {
            PC->PlayerCameraManager->StartCameraShake(ShakeClass);
        }
    }
}

void AProtoCharacter::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    static const FName AmmoAttachNotifyName(TEXT("AmmoAttach"));
    static const FName AmmoDetachNotifyName(TEXT("AmmoDetach"));
    static const FName NewAmmoNotifyName(TEXT("NewAmmo"));
    static const FName NewAmmoAttachNotifyName(TEXT("NewAmmoAttach"));
    static const FName NewAmmoDetachNotifyName(TEXT("NewAmmoDetach"));

    if (NotifyName == AmmoAttachNotifyName)
    {
        ReloadAmmoAttach();
    }
    else if (NotifyName == AmmoDetachNotifyName)
    {
        ReloadAmmoDetach();
    }
    else if (NotifyName == NewAmmoNotifyName)
    {
        ReloadNewAmmo();
    }
    else if (NotifyName == NewAmmoAttachNotifyName)
    {
        if (CurrentWeaponType == EWeaponType::Pistol)
        {
            ReloadNewAmmo();
        }
        else
        {
            ReloadNewAmmoAttach();
        }
    }
    else if (NotifyName == NewAmmoDetachNotifyName)
    {
        ReloadNewAmmoAttach();
    }
    else
    {
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            12002,
            1.0f,
            FColor::Yellow,
            FString::Printf(TEXT("Reload Notify: %s"), *NotifyName.ToString()));
    }
}
void AProtoCharacter::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != RifleReloadMontage || !bIsReloading)
    {
        return;
    }

    bIsReloading = false;
    SwappingAlpha = true;

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            12005,
            1.0f,
            bInterrupted ? FColor::Red : FColor::Green,
            bInterrupted ? TEXT("Reload Montage Interrupted") : TEXT("Reload Montage Ended"));
    }
}

void AProtoCharacter::ReloadWeapon()
{
    if (Swapping > 0.0f || !CurrentWeapon)
    {
        return;
    }

    const bool bCanReload = CurrentWeaponType == EWeaponType::Rifle || CurrentWeaponType == EWeaponType::Pistol;
    if (!bCanReload)
    {
        return;
    }

    StopFireWeapon();

    if (!RifleReloadMontage)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, TEXT("No AM_Player_Upper Montage"));
        }
        return;
    }

    bIsReloading = true;
    SwappingAlpha = false;

    const FName ReloadSectionName = CurrentWeaponType == EWeaponType::Pistol ? PistolReloadSectionName : RifleReloadSectionName;
    const float MontageLength = PlayAnimMontage(RifleReloadMontage, 1.0f, ReloadSectionName);
    if (MontageLength <= 0.0f)
    {
        bIsReloading = false;
        SwappingAlpha = true;
    }

    /*-------------------
     네트워킹: 재장전 브로드캐스트
    -------------------*/
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendWeaponReload(static_cast<uint8>(CurrentWeaponType));
        }
    }
}

/*-------------------
 네트워킹: 원격 플레이어 시각 동기화
-------------------*/
void AProtoCharacter::PlayRemoteReloadMontage(EWeaponType ForWeaponType)
{
    if (!RifleReloadMontage)
    {
        return;
    }

    const FName ReloadSectionName = ForWeaponType == EWeaponType::Pistol ? PistolReloadSectionName : RifleReloadSectionName;
    PlayAnimMontage(RifleReloadMontage, 1.0f, ReloadSectionName);
}

void AProtoCharacter::ApplyRemoteWeaponEquip(EWeaponType ForWeaponType)
{
    if (ForWeaponType == CurrentWeaponType)
    {
        return;
    }

    const EWeaponType PreviousWeaponType = CurrentWeaponType;
    // Storing (ForWeaponType == None) re-attaches the weapon that was just
    // holstered, so it's still the one from PreviousWeaponType.
    CurrentWeapon = ForWeaponType == EWeaponType::None ? GetWeaponByType(PreviousWeaponType) : GetWeaponByType(ForWeaponType);
    CurrentWeaponType = ForWeaponType;
    bHasWeapon = CurrentWeaponType != EWeaponType::None;

    if (!CurrentWeapon)
    {
        // The remote character doesn't have this weapon slot filled: nothing
        // to attach.
        return;
    }

    switch (CurrentWeaponType)
    {
    case EWeaponType::Rifle:
        AttachCurrentWeaponToSocket(TEXT("WeaponSocket"));
        break;
    case EWeaponType::Pistol:
        AttachCurrentWeaponToSocket(TEXT("PistolSocket"));
        break;
    default:
        AttachCurrentWeaponToSocket(PreviousWeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage"));
        break;
    }
}

void AProtoCharacter::AttachCurrentWeaponToSocket(FName SocketName)
{
    if (!CurrentWeapon || !GetMesh())
    {
        return;
    }

    if (!GetMesh()->DoesSocketExist(SocketName))
    {
        if (GEngine)
        {
            const FString Message = FString::Printf(TEXT("No socket: %s"), *SocketName.ToString());
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, Message);
        }
        return;
    }

    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        true);

    CurrentWeapon->AttachToComponent(GetMesh(), AttachRules, SocketName);
    CurrentWeapon->SetActorEnableCollision(false);
}

void AProtoCharacter::HandleEquipmentChanged(EEquipmentSlot ChangedSlot)
{
    if (ChangedSlot != EEquipmentSlot::Weapon1 && ChangedSlot != EEquipmentSlot::Weapon2)
    {
        return;
    }

    if (AWeaponBase** ExistingPtr = EquippedWeaponActors.Find(ChangedSlot))
    {
        AWeaponBase* ExistingActor = *ExistingPtr;
        if (ExistingActor)
        {
            if (CurrentWeapon == ExistingActor)
            {
                GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);
                bHasWeapon = false;
                CurrentWeapon = nullptr;
                CurrentWeaponType = EWeaponType::None;
                PendingWeaponType = EWeaponType::None;
                Swapping = 0.0f;
                SwappingAlpha = true;
            }

            if (CurrentRifle == ExistingActor)
            {
                CurrentRifle = nullptr;
            }
            if (CurrentPistol == ExistingActor)
            {
                CurrentPistol = nullptr;
            }

            ExistingActor->Destroy();
        }
        EquippedWeaponActors.Remove(ChangedSlot);
    }

    if (!EquipmentComponent)
    {
        return;
    }

    const FEquippedItem& Equipped = EquipmentComponent->GetEquippedItem(ChangedSlot);
    const UWeaponItemData* WeaponItem = Equipped.ItemData ? Cast<UWeaponItemData>(Equipped.ItemData) : nullptr;
    if (!WeaponItem || !WeaponItem->WeaponActorClass || !GetMesh())
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;

    AWeaponBase* WeaponActor = GetWorld()->SpawnActor<AWeaponBase>(WeaponItem->WeaponActorClass, GetActorTransform(), SpawnParams);
    if (!WeaponActor)
    {
        return;
    }

    WeaponActor->SetOwner(this);
    WeaponActor->SetInstigator(this);
    WeaponActor->SetActorEnableCollision(false);

    if (UStaticMeshComponent* WeaponMesh = WeaponActor->WeaponMesh)
    {
        WeaponMesh->SetSimulatePhysics(false);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        WeaponMesh->SetGenerateOverlapEvents(false);
    }
    if (UBoxComponent* CollisionBox = WeaponActor->CollisionBox)
    {
        CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
        CollisionBox->SetGenerateOverlapEvents(false);
    }

    const FName StorageSocketName = WeaponActor->WeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage");
    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        true);
    WeaponActor->AttachToComponent(GetMesh(), AttachRules, StorageSocketName);

    if (WeaponActor->WeaponType == EWeaponType::Rifle)
    {
        CurrentRifle = WeaponActor;
    }
    else if (WeaponActor->WeaponType == EWeaponType::Pistol)
    {
        CurrentPistol = WeaponActor;
    }

    EquippedWeaponActors.Add(ChangedSlot, WeaponActor);
}



void AProtoCharacter::ReloadAmmoAttach()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadAmmoAttach(GetMesh());
    }
}

void AProtoCharacter::ReloadAmmoDetach()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadAmmoDetach();
    }
}

void AProtoCharacter::ReloadNewAmmo()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadNewAmmo(GetMesh());
    }
}

void AProtoCharacter::ReloadNewAmmoAttach()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadNewAmmoAttach();
    }
}

bool AProtoCharacter::UseConsumable(UConsumableItemData* ConsumableData)
{
    if (!ConsumableData || !StatusComponent) return false;

    // OverTime 효과가 이미 진행 중이면 사용 불가 (같은 아이템 연타로 중첩 섭취하는 것도 막는다).
    if (bOverTimeEffectActive)
    {
        return false;
    }

    if (RifleReloadMontage)
    {
        FName ConsumableSectionName = NAME_None;
        switch (ConsumableData->TargetStat)
        {
        case EConsumableTargetStat::Health:
            ConsumableSectionName = BandageSectionName;
            break;
        case EConsumableTargetStat::Hunger:
            ConsumableSectionName = EatingSectionName;
            break;
        case EConsumableTargetStat::Thirst:
            ConsumableSectionName = DrinkingSectionName;
            break;
        default:
            break;
        }

        if (ConsumableSectionName != NAME_None)
        {
            PlayAnimMontage(RifleReloadMontage, 1.0f, ConsumableSectionName);
        }
    }

    if (ConsumableData->Application == EEffectApplication::Instant ||
        ConsumableData->Application == EEffectApplication::InstantThenOverTime)
    {
        switch (ConsumableData->TargetStat)
        {
        case EConsumableTargetStat::Health:
            StatusComponent->SetHealth(StatusComponent->GetHealth() + ConsumableData->InstantAmount);
            break;
        case EConsumableTargetStat::Hunger:
            StatusComponent->SetHunger(StatusComponent->GetHunger() + ConsumableData->InstantAmount);
            break;
        case EConsumableTargetStat::Thirst:
            StatusComponent->SetThirst(StatusComponent->GetThirst() + ConsumableData->InstantAmount);
            break;
        case EConsumableTargetStat::Infection:
            StatusComponent->SetInfection(StatusComponent->GetInfection() + ConsumableData->InstantAmount);
            break;
        }
    }

    if (ConsumableData->SideEffect.bAppliesDebuff)
    {
        StatusComponent->SetInfection(StatusComponent->GetInfection() + ConsumableData->SideEffect.InfectionIncrease);
    }

    if ((ConsumableData->Application == EEffectApplication::OverTime ||
         ConsumableData->Application == EEffectApplication::InstantThenOverTime) &&
        ConsumableData->OverTimeDurationSeconds > 0.0f)
    {
        bOverTimeEffectActive = true;
        ApplyOverTimeStatEffect(ConsumableData->TargetStat, ConsumableData->OverTimeAmount, ConsumableData->OverTimeDurationSeconds);
    }

    return true;
}

void AProtoCharacter::OnOverTimeEffectFinished()
{
    bOverTimeEffectActive = false;
}

void AProtoCharacter::ApplyOverTimeStatEffect(EConsumableTargetStat TargetStat, float TotalAmount, float Duration)
{
    if (!StatusComponent || Duration <= 0.0f) return;
    

    constexpr float TickInterval = 0.5f;
    const int32 TotalTicks = FMath::Max(1, FMath::RoundToInt(Duration / TickInterval));
    const float AmountPerTick = TotalAmount / TotalTicks;

    TSharedPtr<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();
    TSharedPtr<int32> RemainingTicks = MakeShared<int32>(TotalTicks);
    TWeakObjectPtr<UPlayerStatusComponent> WeakStatus(StatusComponent);
    TWeakObjectPtr<AProtoCharacter> WeakThis(this);

    FTimerDelegate Delegate;
    Delegate.BindLambda([WeakThis, WeakStatus, TargetStat, AmountPerTick, RemainingTicks, TimerHandle]()
    {
        AProtoCharacter* Character = WeakThis.Get();
        UPlayerStatusComponent* Status = WeakStatus.Get();
        if (!Character || !Status)
        {
            if (Character)
            {
                Character->GetWorldTimerManager().ClearTimer(*TimerHandle);
            }
            return;
        }

        switch (TargetStat)
        {
        case EConsumableTargetStat::Health:
            Status->SetHealth(Status->GetHealth() + AmountPerTick);
            break;
        case EConsumableTargetStat::Hunger:
            Status->SetHunger(Status->GetHunger() + AmountPerTick);
            break;
        case EConsumableTargetStat::Thirst:
            Status->SetThirst(Status->GetThirst() + AmountPerTick);
            break;
        case EConsumableTargetStat::Infection:
            Status->SetInfection(Status->GetInfection() + AmountPerTick);
            break;
        }

        --(*RemainingTicks);
        if (*RemainingTicks <= 0)
        {
            Character->GetWorldTimerManager().ClearTimer(*TimerHandle);
            Character->OnOverTimeEffectFinished();
        }
    });

    GetWorldTimerManager().SetTimer(*TimerHandle, Delegate, TickInterval, true);
}

void AProtoCharacter::OnQuickSlotKeyPressed()
{
    if (bIsInvetoryOpened) return;

    bQuickSlotRadialOpen = false;
    GetWorldTimerManager().SetTimer(QuickSlotHoldTimerHandle, this, &AProtoCharacter::OpenRadialQuickSlotMenu, QuickSlotHoldThreshold, false);
}

void AProtoCharacter::OpenRadialQuickSlotMenu()
{
    if (!RadialQuickSlotWidgetClass || !QuickSlotComponent) return;

    bQuickSlotRadialOpen = true;

    if (!RadialQuickSlotWidgetInstance)
    {
        RadialQuickSlotWidgetInstance = CreateWidget<URadialQuickSlotWidget>(GetWorld(), RadialQuickSlotWidgetClass);
    }
    if (!RadialQuickSlotWidgetInstance) return;

    RadialQuickSlotWidgetInstance->OpenRadial(QuickSlotComponent);
    if (!RadialQuickSlotWidgetInstance->IsInViewport())
    {
        RadialQuickSlotWidgetInstance->AddToViewport(500);
    }

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->SetShowMouseCursor(true);
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
    }
}

void AProtoCharacter::OnQuickSlotKeyReleased()
{
    if (bIsInvetoryOpened) return;

    GetWorldTimerManager().ClearTimer(QuickSlotHoldTimerHandle);

    if (bQuickSlotRadialOpen)
    {
        int32 Selected = INDEX_NONE;
        if (RadialQuickSlotWidgetInstance)
        {
            Selected = RadialQuickSlotWidgetInstance->GetHighlightedSlotIndex();
            RadialQuickSlotWidgetInstance->RemoveFromParent();
        }
        bQuickSlotRadialOpen = false;

        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            PC->SetShowMouseCursor(false);
            PC->SetInputMode(FInputModeGameOnly());
        }

        if (Selected != INDEX_NONE && QuickSlotComponent)
        {
            // 꾹 눌러 고르는 동작은 "다음에 쓸 슬롯"을 지정(등록)하는 것뿐이며 그 자리에서 소모하지 않는다.
            // 실제 사용은 이후 4번 키를 짧게 탭했을 때 일어난다.
            QuickSlotComponent->LastUsedSlotIndex = Selected;
            QuickSlotComponent->OnQuickSlotChanged.Broadcast(Selected);
        }
    }
    else if (QuickSlotComponent && QuickSlotComponent->LastUsedSlotIndex != INDEX_NONE)
    {
        QuickSlotComponent->UseQuickSlot(QuickSlotComponent->LastUsedSlotIndex, this);
    }
}
