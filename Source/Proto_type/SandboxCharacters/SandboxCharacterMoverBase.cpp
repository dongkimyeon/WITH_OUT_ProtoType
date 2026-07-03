#include "SandboxCharacterMoverBase.h"

#include "Backends/MoverNetworkPredictionLiaison.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"
#include "MoverDataModelTypes.h"

ASandboxCharacterMoverBase::ASandboxCharacterMoverBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicatingMovement(false);

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	CapsuleComponent->InitCapsuleSize(42.0f, 96.0f);
	CapsuleComponent->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CapsuleComponent;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CapsuleComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
	Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CapsuleComponent);
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	CharacterMoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("CharacterMoverComponent"));
	CharacterMoverComponent->SetUpdatedComponent(CapsuleComponent);
	CharacterMoverComponent->InputProducer = this;
	CharacterMoverComponent->BackendClass = UMoverNetworkPredictionLiaisonComponent::StaticClass();

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void ASandboxCharacterMoverBase::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->PlayerCameraManager->ViewPitchMax = 89.0f;
		PlayerController->PlayerCameraManager->ViewPitchMin = -89.0f;

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

void ASandboxCharacterMoverBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (CharacterMoverComponent && CapsuleComponent)
	{
		CharacterMoverComponent->SetUpdatedComponent(CapsuleComponent);
	}
}

void ASandboxCharacterMoverBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->AddYawInput(CachedLookInput.Yaw * LookYawRate * DeltaSeconds);
		PlayerController->AddPitchInput(-CachedLookInput.Pitch * LookPitchRate * DeltaSeconds);
	}

	CachedLookInput = FRotator::ZeroRotator;
}

void ASandboxCharacterMoverBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASandboxCharacterMoverBase::HandleMoveTriggered);
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASandboxCharacterMoverBase::HandleMoveCompleted);
		}

		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASandboxCharacterMoverBase::HandleLookTriggered);
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Completed, this, &ASandboxCharacterMoverBase::HandleLookCompleted);
		}

		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ASandboxCharacterMoverBase::HandleJumpStarted);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASandboxCharacterMoverBase::HandleJumpCompleted);
		}

		if (FlyAction)
		{
			EnhancedInput->BindAction(FlyAction, ETriggerEvent::Triggered, this, &ASandboxCharacterMoverBase::HandleFlyTriggered);
		}
	}
}

FVector ASandboxCharacterMoverBase::GetNavAgentLocation() const
{
	return CapsuleComponent ? CapsuleComponent->GetComponentLocation() - FVector(0.0f, 0.0f, CapsuleComponent->GetScaledCapsuleHalfHeight()) : Super::GetNavAgentLocation();
}

void ASandboxCharacterMoverBase::RequestMoveByIntent(const FVector& DesiredIntent)
{
	CachedMoveInputIntent = DesiredIntent;
	CachedMoveInputVelocity = FVector::ZeroVector;
}

void ASandboxCharacterMoverBase::RequestMoveByVelocity(const FVector& DesiredVelocity)
{
	CachedMoveInputVelocity = DesiredVelocity;
}

void ASandboxCharacterMoverBase::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceMoverInput(static_cast<float>(SimTimeMs), InputCmdResult);
}

void ASandboxCharacterMoverBase::OnProduceMoverInput_Implementation(float DeltaMs, FMoverInputCmdContext& InputCmd)
{
	FCharacterDefaultInputs& CharacterInputs = InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	if (Controller == nullptr)
	{
		return;
	}

	CharacterInputs.ControlRotation = Controller->GetControlRotation();

	const bool bUsingIntent = CachedMoveInputVelocity.IsZero();
	if (bUsingIntent)
	{
		const FVector MoveInput = bMoveRelativeToControlRotation
			? FRotator(0.0f, CharacterInputs.ControlRotation.Yaw, 0.0f).RotateVector(CachedMoveInputIntent)
			: CachedMoveInputIntent;

		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, MoveInput);
	}
	else
	{
		CharacterInputs.SetMoveInput(EMoveInputType::Velocity, CachedMoveInputVelocity);
	}

	const bool bHasMoveInput = CharacterInputs.GetMoveInput().SizeSquared() > KINDA_SMALL_NUMBER;
	CharacterInputs.OrientationIntent = FVector::ZeroVector;

	if (bHasMoveInput)
	{
		CharacterInputs.OrientationIntent = bOrientRotationToMovement
			? CharacterInputs.GetMoveInput().GetSafeNormal()
			: CharacterInputs.ControlRotation.Vector().GetSafeNormal();

		LastAffirmativeOrientationIntent = CharacterInputs.OrientationIntent;
	}
	else if (bMaintainLastInputOrientation)
	{
		CharacterInputs.OrientationIntent = LastAffirmativeOrientationIntent;
	}

	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;

	if (bShouldToggleFlying)
	{
		CharacterInputs.SuggestedMovementMode = bIsFlyingActive ? DefaultModeNames::Falling : DefaultModeNames::Flying;
		bIsFlyingActive = !bIsFlyingActive;
	}
	else
	{
		CharacterInputs.SuggestedMovementMode = NAME_None;
	}

	bIsJumpJustPressed = false;
	bShouldToggleFlying = false;
}

void ASandboxCharacterMoverBase::HandleMoveTriggered(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	CachedMoveInputIntent.X = FMath::Clamp(MovementVector.Y, -1.0f, 1.0f);
	CachedMoveInputIntent.Y = FMath::Clamp(MovementVector.X, -1.0f, 1.0f);
	CachedMoveInputIntent.Z = 0.0f;
	CachedMoveInputVelocity = FVector::ZeroVector;
}

void ASandboxCharacterMoverBase::HandleMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void ASandboxCharacterMoverBase::HandleLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	CachedLookInput.Yaw = FMath::Clamp(LookVector.X, -1.0f, 1.0f);
	CachedLookInput.Pitch = FMath::Clamp(LookVector.Y, -1.0f, 1.0f);
}

void ASandboxCharacterMoverBase::HandleLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput = FRotator::ZeroRotator;
}

void ASandboxCharacterMoverBase::HandleJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void ASandboxCharacterMoverBase::HandleJumpCompleted(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

void ASandboxCharacterMoverBase::HandleFlyTriggered(const FInputActionValue& Value)
{
	bShouldToggleFlying = true;
}
