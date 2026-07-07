#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h" // FInputActionValue를 쓰기 위해 필수!
#include "ProtoCharacter.generated.h"

// 전방 선언 (헤더 파일이 무거워지는 것을 방지)
class UInputMappingContext;
class UInputAction;

UCLASS()
class PROTOPROJECT_API AProtoCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AProtoCharacter();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// -----------------------------------------------------------------------------
	// 향상된 입력 (Enhanced Input) 세팅
	// -----------------------------------------------------------------------------
private:
	// 1. 입력 매핑 컨텍스트 (키보드/마우스 설정 묶음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	// 2. 이동 액션 (WASD)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	// 3. 시점 회전 액션 (마우스 이동)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	
	// 4. 점프 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;
	
	// 5. 달리기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;
	
	
	
	// 4. 실제 동작할 함수들
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Sprint(const FInputActionValue& Value);
public:
	UPROPERTY(EditAnywhere, Category = "Movement")
	float BaseWalkSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintMultiplier = 2.0f;
};