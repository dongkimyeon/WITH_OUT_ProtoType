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
	// ?μ긽???낅젰 (Enhanced Input) ?명똿
	// -----------------------------------------------------------------------------
private:
	// 1. ?낅젰 留ㅽ븨 而⑦뀓?ㅽ듃 (?ㅻ낫??留덉슦???ㅼ젙 臾띠쓬)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	// 2. ?대룞 ?≪뀡 (WASD)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	// 3. ?쒖젏 ?뚯쟾 ?≪뀡 (留덉슦???대룞)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;
	
	// 4. ?먰봽 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;
	
	// 5. ?щ━湲?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* SprintAction;
	
	// 6. ?몃깽?좊━ 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputAction* ToggleInventoryAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> InventoryWidgetClass;
	
	UPROPERTY()
	UUserWidget* InventoryWidgetInstance;
	
	// 7. ?몃깽?좊━ 而댄룷?뚰듃 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	UInventoryGridComponent* InventoryComponent;
	
	//?뚯뒪?몄슜
	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestBandage;

	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestArmor;

	UPROPERTY(EditAnywhere, Category = "Test Inventory")
	class UItemDataBase* TestRifle;
	
	
	//?⑥닔 -----------------------------------------------------
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Sprint(const FInputActionValue& Value);
	void ToggleInventory(const FInputActionValue& Value);
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bHasWeapon = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	AAK47* CurrentWeapon = nullptr;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float BaseWalkSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintMultiplier = 2.0f;
	private:
	bool bIsInvetoryOpened = false;
};
