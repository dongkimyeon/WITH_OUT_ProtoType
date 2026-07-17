#include "ProtoCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenWidget.h"
#include "InventoryGridComponent.h"
#include "Item/ItemDataBase.h"
#include "Item/DropItem.h"
#include "PlayerDefalutUI.h"
#include "InputCoreTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "weapon/WeaponBase.h"

AProtoCharacter::AProtoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    InventoryComponent = CreateDefaultSubobject<UInventoryGridComponent>(TEXT("InventoryComponent"));
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = true;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void AProtoCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (Controller)
    {
        const float NormalizedPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch);
        AimPitch = FMath::Clamp(NormalizedPitch, -30.0f, 30.0f);
    }

    if (CurrentWeapon && CurrentWeapon->GetRootComponent() && GetMesh())
    {
        static const FName LeftHandSocketName(TEXT("LeftHandSocket"));
        static const FName RightHandBoneName(TEXT("hand_r"));

        const FTransform LeftHandSocketTransform = CurrentWeapon->GetRootComponent()->GetSocketTransform(
            LeftHandSocketName,
            RTS_World);

        FVector OutPosition;
        FRotator OutRotation;
        GetMesh()->TransformToBoneSpace(
            RightHandBoneName,
            LeftHandSocketTransform.GetLocation(),
            LeftHandSocketTransform.Rotator(),
            OutPosition,
            OutRotation);

        LeftHandTransform = FTransform(OutRotation, OutPosition, FVector::OneVector);
    }
}

void AProtoCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    if (InventoryComponent)
    {
        if (TestArmor) InventoryComponent->AddItem(TestArmor);
        if (TestRifle) InventoryComponent->AddItem(TestRifle);
        if (TestBandage) InventoryComponent->AddItem(TestBandage);
        if (TestBandage) InventoryComponent->AddItem(TestBandage);
    }

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

    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AProtoCharacter::SetWeaponTypeNone);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AProtoCharacter::SetWeaponTypeRifle);
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AProtoCharacter::FireWeapon);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AProtoCharacter::StartSprint);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AProtoCharacter::StopSprint);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AProtoCharacter::StartAim);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AProtoCharacter::StopAim);
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
    bIsSprint = true;
    GetCharacterMovement()->MaxWalkSpeed = SprintWalkSpeed;
}

void AProtoCharacter::StopSprint()
{
    bIsSprint = false;
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void AProtoCharacter::StartAim()
{
    bIsAiming = true;
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AProtoCharacter::StopAim()
{
    bIsAiming = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = true;
    GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AProtoCharacter::Interact(const FInputActionValue& Value)
{
    if (NearbyDropItems.IsEmpty())
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC)
    {
        return;
    }

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const FVector TraceStart = CamLoc + CamRot.Vector() * 400.f;
    const FVector TraceEnd = CamLoc + CamRot.Vector() * 700.f;
    GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
    DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green, false, 2.f);

    ADropItem* HitItem = Cast<ADropItem>(Hit.GetActor());
    if (IsValid(HitItem) && NearbyDropItems.Contains(HitItem))
    {
        InventoryComponent->AddItem(HitItem->ItemData);
        HidePickupPrompt(HitItem);
        HitItem->Destroy();
    }
}

void AProtoCharacter::ShowPickupPrompt(ADropItem* Item)
{
    NearbyDropItems.AddUnique(Item);
    if (DefaultUI)
    {
        DefaultUI->AddPickupPrompt(Item);
    }
}

void AProtoCharacter::HidePickupPrompt(ADropItem* Item)
{
    NearbyDropItems.Remove(Item);
    if (DefaultUI)
    {
        DefaultUI->RemovePickupPrompt(Item);
    }
}

void AProtoCharacter::ToggleInventory(const FInputActionValue& Value)
{
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
    bHasWeapon = false;
    CurrentWeaponType = EWeaponType::None;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, TEXT("Weapon Type: None"));
    }
}

void AProtoCharacter::SetWeaponTypeRifle()
{
    bHasWeapon = true;
    CurrentWeaponType = EWeaponType::Rifle;
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("Weapon Type: Rifle"));
    }
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
}


