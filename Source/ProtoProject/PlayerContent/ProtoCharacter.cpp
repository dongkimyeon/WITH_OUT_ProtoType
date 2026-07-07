#include "ProtoCharacter.h"
#include "EnhancedInputComponent.h"      // 향상된 입력 컴포넌트
#include "EnhancedInputSubsystems.h"   // 로컬 플레이어 서브시스템
#include "GameFramework/CharacterMovementComponent.h"

AProtoCharacter::AProtoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
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
    // 마우스 이동값을 2D 벡터로 가져옵니다.
    FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // 컨트롤러의 Yaw(좌우)와 Pitch(상하) 회전값을 더해줍니다.
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
