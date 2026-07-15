#include "ProtoCharacter.h"
#include "EnhancedInputComponent.h"      
#include "EnhancedInputSubsystems.h"  
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenWidget.h"
#include "InventoryGridComponent.h"
#include "Item/ItemDataBase.h"
#include "InputCoreTypes.h"
#include "weapon/WeaponBase.h"

AProtoCharacter::AProtoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    InventoryComponent = CreateDefaultSubobject<UInventoryGridComponent>(TEXT("InventoryComponent"));
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
        if (TestArmor) InventoryComponent->AddItem(TestArmor);   // 2x2
        if (TestRifle) InventoryComponent->AddItem(TestRifle);   // 3x1
        if (TestBandage) InventoryComponent->AddItem(TestBandage); // 1x1
        if (TestBandage) InventoryComponent->AddItem(TestBandage); 
    }
    
}

void AProtoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // 湲곕낯 InputComponent瑜?EnhancedInputComponent濡?罹먯뒪??
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Look);

       
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump); //罹먮┃???댁쓽 Jump瑜?諛붾줈 ?몄텧
        
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

        // Shift ?ㅻ? ?꾨? ?뚯? ????媛숈? ?⑥닔瑜?遺瑜대룄濡??명똿?⑸땲??
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AProtoCharacter::Sprint);
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AProtoCharacter::Sprint);
        
        EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AProtoCharacter::ToggleInventory);
    }

    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AProtoCharacter::SetWeaponTypeNone);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AProtoCharacter::SetWeaponTypeRifle);
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AProtoCharacter::FireWeapon);
}

void AProtoCharacter::Move(const FInputActionValue& Value)
{
    // ?낅젰媛믪쓣 2D 踰≫꽣(X, Y)濡?媛?몄샃?덈떎.
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // 移대찓?쇨? 諛붾씪蹂대뒗 諛⑺뼢??湲곗??쇰줈 ????醫??곕? 怨꾩궛?⑸땲??
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        
        AddMovementInput(ForwardDirection, MovementVector.X); // ????(W, S)
        AddMovementInput(RightDirection, MovementVector.Y);   // 醫???(A, D)
    }
}

void AProtoCharacter::Look(const FInputActionValue& Value)
{
    if (bIsInvetoryOpened)
    {
        return;
    }
    // 留덉슦???대룞媛믪쓣 2D 踰≫꽣濡?媛?몄샃?덈떎.
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
    
   
}



void AProtoCharacter::Sprint(const FInputActionValue& Value)
{
    bool bIsSprinting = Value.Get<bool>();
    
    if (bIsSprinting)
    {
        // ???뚮뒗 湲곕낯 ?띾룄??諛곗닔瑜?怨깊븳 ?덈?媛믪쓣 ?ｌ뒿?덈떎.
        GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * SprintMultiplier;
    }
    else
    {
        // ?먯쓣 ?쇰㈃ 臾댁“嫄??덉쟾??湲곕낯 ?띾룄濡???뼱?뚯썎?덈떎.
        GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
    }
    
    
}

void AProtoCharacter::ToggleInventory(const FInputActionValue& Value)
{
    if (InventoryWidgetInstance == nullptr && InventoryWidgetClass != nullptr)
    {
        InventoryWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), InventoryWidgetClass);
    }
    UE_LOG(LogTemp, Warning, TEXT("I ???낅젰"));

    if (InventoryWidgetInstance != nullptr)
    {
        APlayerController* PlayerController = Cast<APlayerController>(Controller);
        
        
        if (InventoryWidgetInstance->IsInViewport()) //?リ린
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
