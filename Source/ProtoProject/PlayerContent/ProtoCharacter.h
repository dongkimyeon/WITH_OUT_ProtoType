#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h" // FInputActionValue를 쓰기 위해 필수!
#include "ProtoCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget; 
class UInventoryGridComponent;

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
	
	// 6. 인벤토리 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* ToggleInventoryAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> InventoryWidgetClass;
	
	UPROPERTY()
	UUserWidget* InventoryWidgetInstance;
	
	// 7. 인벤토리 컴포넌트 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryGridComponent* InventoryComponent;
	
	//테스트용
	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestBandage;

	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestArmor;

	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestRifle;
	
	
	//함수 -----------------------------------------------------
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Sprint(const FInputActionValue& Value);
	void ToggleInventory(const FInputActionValue& Value);
public:
	UPROPERTY(EditAnywhere, Category = "Movement")
	float BaseWalkSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintMultiplier = 2.0f;
	private:
	bool bIsInvetoryOpened = false;
};