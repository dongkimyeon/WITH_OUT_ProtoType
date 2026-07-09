#include "ProtoCharacter.h"
#include "EnhancedInputComponent.h"      
#include "EnhancedInputSubsystems.h"  
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenWidget.h"
#include "InventoryGridComponent.h"
#include "Item/ItemDataBase.h"

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

    // 기본 InputComponent를 EnhancedInputComponent로 캐스팅
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Look);

       
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump); //캐릭터 내의 Jump를 바로 호출
        
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

        // Shift 키를 누를 때와 뗄 때 같은 함수를 부르도록 세팅합니다.
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AProtoCharacter::Sprint);
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AProtoCharacter::Sprint);
        
        EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AProtoCharacter::ToggleInventory);
    }
}

void AProtoCharacter::Move(const FInputActionValue& Value)
{
    // 입력값을 2D 벡터(X, Y)로 가져옵니다.
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // 카메라가 바라보는 방향을 기준으로 앞/뒤/좌/우를 계산합니다.
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        
        AddMovementInput(ForwardDirection, MovementVector.X); // 앞/뒤 (W, S)
        AddMovementInput(RightDirection, MovementVector.Y);   // 좌/우 (A, D)
    }
}

void AProtoCharacter::Look(const FInputActionValue& Value)
{
    if (bIsInvetoryOpened)
    {
        return;
    }
    // 마우스 이동값을 2D 벡터로 가져옵니다.
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
        // 뛸 때는 기본 속도에 배수를 곱한 절대값을 넣습니다.
        GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * SprintMultiplier;
    }
    else
    {
        // 손을 떼면 무조건 안전한 기본 속도로 덮어씌웁니다.
        GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
    }
    
    
}

void AProtoCharacter::ToggleInventory(const FInputActionValue& Value)
{
    if (InventoryWidgetInstance == nullptr && InventoryWidgetClass != nullptr)
    {
        InventoryWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), InventoryWidgetClass);
    }
    UE_LOG(LogTemp, Warning, TEXT("I 키 입력"));

    if (InventoryWidgetInstance != nullptr)
    {
        APlayerController* PlayerController = Cast<APlayerController>(Controller);
        
        
        if (InventoryWidgetInstance->IsInViewport()) //닫기
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
