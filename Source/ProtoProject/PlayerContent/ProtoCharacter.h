#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "ProtoCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UInventoryGridComponent;
class AWeaponBase;
class ADropItem;
class AStorageContainer;
class UPlayerDefalutUI;
class UContainerScreenWidget;

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None UMETA(DisplayName = "None"),
    Rifle UMETA(DisplayName = "Rifle"),
    Pistol UMETA(DisplayName = "Pistol"),
    Melee UMETA(DisplayName = "Melee")
};

UCLASS()
class PROTOPROJECT_API AProtoCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AProtoCharacter();
    virtual void Tick(float DeltaTime) override;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UPlayerDefalutUI> DefaultUIClass;

    UPROPERTY()
    UPlayerDefalutUI* DefaultUI = nullptr;

    UPROPERTY()
    TArray<ADropItem*> NearbyDropItems;

    UPROPERTY()
    TArray<AStorageContainer*> NearbyContainers;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UContainerScreenWidget> ContainerWidgetClass;

    UPROPERTY()
    UContainerScreenWidget* ContainerWidgetInstance = nullptr;

    bool bIsContainerOpened = false;

    void OpenContainerScreen(AStorageContainer* Container);
    void CloseContainerScreen();

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Sprint(const FInputActionValue& Value);
    void StartSprint();
    void StopSprint();
    void StartAim();
    void StopAim();
    void ToggleInventory(const FInputActionValue& Value);
    void Interact(const FInputActionValue& Value);
    void SetWeaponTypeNone();
    void SetWeaponTypeRifle();
    void FireWeapon();
    void AttachCurrentWeaponToSocket(FName SocketName);

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    bool bHasWeapon = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AWeaponBase* CurrentWeapon = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponType CurrentWeaponType = EWeaponType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim")
    float AimPitch = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|IK")
    FTransform LeftHandTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    bool bIsSprint = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim")
    bool bIsAiming = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float BaseWalkSpeed = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float SprintWalkSpeed = 600.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float SprintMultiplier = 2.0f;

    void ShowPickupPrompt(ADropItem* Item);
    void HidePickupPrompt(ADropItem* Item);

    void ShowContainerPrompt(AStorageContainer* Container);
    void HideContainerPrompt(AStorageContainer* Container);

private:
    bool bIsInvetoryOpened = false;
};