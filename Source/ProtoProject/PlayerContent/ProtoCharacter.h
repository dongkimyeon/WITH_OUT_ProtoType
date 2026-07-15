#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h" // FInputActionValue瑜??곌린 ?꾪빐 ?꾩닔!
#include "ProtoCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UInventoryGridComponent;
class AAK47;
class ADropItem;
class UPlayerDefalutUI;

UCLASS()
class PROTOPROJECT_API AProtoCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AProtoCharacter();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction; 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* ToggleInventoryAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* InteractAction;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> InventoryWidgetClass;
	
	
	
	UPROPERTY()
	UUserWidget* InventoryWidgetInstance;
	 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryGridComponent* InventoryComponent;
	
	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestBandage;

	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestArmor;

	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestRifle;
	
	
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Sprint(const FInputActionValue& Value);
	void ToggleInventory(const FInputActionValue& Value);
	void Interact(const FInputActionValue& Value);

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UPlayerDefalutUI> DefaultUIClass;

	UPROPERTY()
	UPlayerDefalutUI* DefaultUI = nullptr;

	ADropItem* NearbyDropItem = nullptr;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bHasWeapon = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	AAK47* CurrentWeapon = nullptr;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float BaseWalkSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintMultiplier = 2.0f;

	void ShowPickupPrompt(ADropItem* Item);
	void HidePickupPrompt();

	private:
	bool bIsInvetoryOpened = false;
};
