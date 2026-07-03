#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "SandboxCharacterMoverBase.generated.h"

class UCapsuleComponent;
class UCameraComponent;
class UCharacterMoverComponent;
class UInputAction;
class UInputMappingContext;
class UMotionWarpingComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class PROTO_TYPE_API ASandboxCharacterMoverBase : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	ASandboxCharacterMoverBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual FVector GetNavAgentLocation() const override;

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	UCharacterMoverComponent* GetMoverComponent() const { return CharacterMoverComponent; }

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	UFUNCTION(BlueprintCallable, Category = "Sandbox|Movement")
	void RequestMoveByIntent(const FVector& DesiredIntent);

	UFUNCTION(BlueprintCallable, Category = "Sandbox|Movement")
	void RequestMoveByVelocity(const FVector& DesiredVelocity);

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	UFUNCTION(BlueprintNativeEvent, Category = "Sandbox|Mover")
	void OnProduceMoverInput(float DeltaMs, UPARAM(ref) FMoverInputCmdContext& InputCmd);
	virtual void OnProduceMoverInput_Implementation(float DeltaMs, FMoverInputCmdContext& InputCmd);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<UCharacterMoverComponent> CharacterMoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sandbox|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sandbox|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sandbox|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sandbox|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sandbox|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sandbox|Input")
	TObjectPtr<UInputAction> FlyAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Mover")
	bool bMoveRelativeToControlRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Mover")
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Mover")
	bool bMaintainLastInputOrientation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Camera")
	float LookYawRate = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Camera")
	float LookPitchRate = 100.0f;

private:
	void HandleMoveTriggered(const FInputActionValue& Value);
	void HandleMoveCompleted(const FInputActionValue& Value);
	void HandleLookTriggered(const FInputActionValue& Value);
	void HandleLookCompleted(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);
	void HandleFlyTriggered(const FInputActionValue& Value);

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FVector CachedMoveInputVelocity = FVector::ZeroVector;
	FVector LastAffirmativeOrientationIntent = FVector::ZeroVector;
	FRotator CachedLookInput = FRotator::ZeroRotator;

	bool bIsJumpJustPressed = false;
	bool bIsJumpPressed = false;
	bool bIsFlyingActive = false;
	bool bShouldToggleFlying = false;
};
