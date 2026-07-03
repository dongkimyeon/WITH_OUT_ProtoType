#include "SandboxCharacterCMCBase.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"

ASandboxCharacterCMCBase::ASandboxCharacterCMCBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.0f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void ASandboxCharacterCMCBase::BeginPlay()
{
	Super::BeginPlay();

	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DefaultMappingContext)
				{
					Subsystem->AddMappingContext(DefaultMappingContext, MappingPriority);
				}
			}
		}
	}
}

void ASandboxCharacterCMCBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ASandboxCharacterCMCBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASandboxCharacterCMCBase::HandleMoveTriggered);
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASandboxCharacterCMCBase::HandleMoveCompleted);
		}

		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASandboxCharacterCMCBase::HandleLookTriggered);
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Completed, this, &ASandboxCharacterCMCBase::HandleLookCompleted);
		}

		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ASandboxCharacterCMCBase::HandleJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASandboxCharacterCMCBase::HandleJumpCompleted);
		}

		if (CrouchAction)
		{
			EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ASandboxCharacterCMCBase::HandleCrouchStarted);
		}
	}
}

void ASandboxCharacterCMCBase::SetMovementInputEnabled(bool bEnabled)
{
	bMovementInputEnabled = bEnabled;
}

void ASandboxCharacterCMCBase::SetLookInputEnabled(bool bEnabled)
{
	bLookInputEnabled = bEnabled;
}

void ASandboxCharacterCMCBase::OnMoveInput_Implementation(const FVector2D& Input)
{
	if (!bMovementInputEnabled || Controller == nullptr)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator MovementRotation(0.0f, bMoveRelativeToControllerYaw ? ControlRotation.Yaw : GetActorRotation().Yaw, 0.0f);

	AddMovementInput(FRotationMatrix(MovementRotation).GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(FRotationMatrix(MovementRotation).GetUnitAxis(EAxis::Y), Input.X);
}

void ASandboxCharacterCMCBase::OnLookInput_Implementation(const FVector2D& Input)
{
	if (!bLookInputEnabled)
	{
		return;
	}

	AddControllerYawInput(Input.X * LookYawRate);
	AddControllerPitchInput(Input.Y * LookPitchRate);
}

void ASandboxCharacterCMCBase::OnJumpInputStarted_Implementation()
{
	Jump();
}

void ASandboxCharacterCMCBase::OnJumpInputCompleted_Implementation()
{
	StopJumping();
}

void ASandboxCharacterCMCBase::OnCrouchInputStarted_Implementation()
{
	if (bToggleCrouch)
	{
		bIsCrouched ? UnCrouch() : Crouch();
	}
	else
	{
		Crouch();
	}
}

void ASandboxCharacterCMCBase::HandleMoveTriggered(const FInputActionValue& Value)
{
	OnMoveInput(Value.Get<FVector2D>());
}

void ASandboxCharacterCMCBase::HandleMoveCompleted(const FInputActionValue& Value)
{
	OnMoveInput(FVector2D::ZeroVector);
}

void ASandboxCharacterCMCBase::HandleLookTriggered(const FInputActionValue& Value)
{
	OnLookInput(Value.Get<FVector2D>());
}

void ASandboxCharacterCMCBase::HandleLookCompleted(const FInputActionValue& Value)
{
	OnLookInput(FVector2D::ZeroVector);
}

void ASandboxCharacterCMCBase::HandleJumpStarted(const FInputActionValue& Value)
{
	OnJumpInputStarted();
}

void ASandboxCharacterCMCBase::HandleJumpCompleted(const FInputActionValue& Value)
{
	OnJumpInputCompleted();
}

void ASandboxCharacterCMCBase::HandleCrouchStarted(const FInputActionValue& Value)
{
	OnCrouchInputStarted();
}
