#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SandboxCharacterCMCBase.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMotionWarpingComponent;
class USpringArmComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class PROTO_TYPE_API ASandboxCharacterCMCBase : public ACharacter
{
	GENERATED_BODY()

public:
	ASandboxCharacterCMCBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category = "Sandbox|Components")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	UFUNCTION(BlueprintCallable, Category = "Sandbox|Input")
	void SetMovementInputEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Sandbox|Input")
	void SetLookInputEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintNativeEvent, Category = "Sandbox|Input")
	void OnMoveInput(const FVector2D& Input);
	virtual void OnMoveInput_Implementation(const FVector2D& Input);

	UFUNCTION(BlueprintNativeEvent, Category = "Sandbox|Input")
	void OnLookInput(const FVector2D& Input);
	virtual void OnLookInput_Implementation(const FVector2D& Input);

	UFUNCTION(BlueprintNativeEvent, Category = "Sandbox|Input")
	void OnJumpInputStarted();
	virtual void OnJumpInputStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Sandbox|Input")
	void OnJumpInputCompleted();
	virtual void OnJumpInputCompleted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Sandbox|Input")
	void OnCrouchInputStarted();
	virtual void OnCrouchInputStarted_Implementation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sandbox|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

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
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Movement")
	bool bMoveRelativeToControllerYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Movement")
	bool bToggleCrouch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Camera")
	float LookYawRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox|Camera")
	float LookPitchRate = 1.0f;

private:
	void HandleMoveTriggered(const FInputActionValue& Value);
	void HandleMoveCompleted(const FInputActionValue& Value);
	void HandleLookTriggered(const FInputActionValue& Value);
	void HandleLookCompleted(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);
	void HandleCrouchStarted(const FInputActionValue& Value);

	bool bMovementInputEnabled = true;
	bool bLookInputEnabled = true;
};
