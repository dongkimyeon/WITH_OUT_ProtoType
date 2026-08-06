#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Interactable.h"
#include "ProtoCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UInventoryGridComponent;
class UEquipmentComponent;
class UQuickSlotComponent;
class URadialQuickSlotWidget;
class UConsumableItemData;
class AWeaponBase;
class AStorageContainer;
class UPlayerDefalutUI;
class UContainerScreenWidget;
class UAnimMontage;
class UPlayerStatusComponent;
class ULevelChangeSelectWidget;
class ALevelChanger;
struct FBranchingPointNotifyPayload;

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None = 0 UMETA(DisplayName = "None"),
    Rifle = 1 UMETA(DisplayName = "Rifle"),
    Pistol = 2 UMETA(DisplayName = "Pistol"),
    Melee = 3 UMETA(DisplayName = "Melee")
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|QuickSlot", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<URadialQuickSlotWidget> RadialQuickSlotWidgetClass;

    UPROPERTY()
    URadialQuickSlotWidget* RadialQuickSlotWidgetInstance = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|QuickSlot", meta = (AllowPrivateAccess = "true"))
    float QuickSlotHoldThreshold = 0.3f;

    FTimerHandle QuickSlotHoldTimerHandle;
    bool bQuickSlotRadialOpen = false;

    void OnQuickSlotKeyPressed();
    void OnQuickSlotKeyReleased();
    void OpenRadialQuickSlotMenu();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    UInventoryGridComponent* InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    UEquipmentComponent* EquipmentComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    UQuickSlotComponent* QuickSlotComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    UPlayerStatusComponent* StatusComponent;

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
    TArray<AActor*> NearbyInteractables;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UContainerScreenWidget> ContainerWidgetClass;

    UPROPERTY()
    UContainerScreenWidget* ContainerWidgetInstance = nullptr;

    bool bIsContainerOpened = false;

    void CloseContainerScreen();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<ULevelChangeSelectWidget> LevelChangeWidgetClass;

    UPROPERTY()
    ULevelChangeSelectWidget* LevelChangeWidgetInstance = nullptr;

    bool bIsLevelChangeOpened = false;

    void CloseLevelChangeScreen();

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
    void SetWeaponTypePistol();
    void BeginWeaponSwap(EWeaponType TargetWeaponType);
    void FinishWeaponSwap();
    void StartFireWeapon();
    void StopFireWeapon();
    void FireWeapon();
    void ReloadWeapon();

    UFUNCTION()
    void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
    void AttachCurrentWeaponToSocket(FName SocketName);
    AWeaponBase* GetWeaponByType(EWeaponType WeaponType) const;

    void DebugDecreaseHealth();
    void DebugDecreaseHunger();
    void DebugDecreaseThirst();
    void DebugIncreaseInfection();
    void DebugDecreaseStamina();

    void UpdateStamina(float DeltaTime);

    float StaminaRegenTimer = 0.0f;
    bool bStaminaDepleted = false;

    FTimerHandle AutoFireTimerHandle;

    // Throttles how often this client reports its position to the server
    // (SendMoveInput), so other connected players can see it move.
    float NetSyncInterval = 0.1f;
    float NetSyncTimer = 0.0f;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    bool bHasWeapon = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AWeaponBase* CurrentWeapon = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AWeaponBase* CurrentRifle = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AWeaponBase* CurrentPistol = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponType CurrentWeaponType = EWeaponType::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponType PendingWeaponType = EWeaponType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponType SwapFromWeaponType = EWeaponType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float Swapping = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    bool SwappingAlpha = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    UAnimMontage* WeaponSwapMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    UAnimMontage* RifleReloadMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    FName RifleToHandSectionName = TEXT("rifletohand");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    FName HandToRifleSectionName = TEXT("handtorifle");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    FName PistolToHandSectionName = TEXT("pistoltohand");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    FName HandToPistolSectionName = TEXT("handtopistol");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    FName RifleReloadSectionName = TEXT("RifleReload");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
    FName PistolReloadSectionName = TEXT("pistolreload");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim")
    float AimPitch = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|IK")
    FTransform LeftHandTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|IK")
    FVector Joint = FVector(1000.0f, -2000.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|IK")
    bool bDebugLeftHandIK = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|IK")
    float LeftHandIKDebugDrawSize = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool bIsSprint = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    bool bIsReloading = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
    bool bIsAiming = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Camera")
    float DefaultCameraArmLength = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Camera")
    float AimCameraArmLength = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Camera")
    FVector DefaultCameraRelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim|Camera")
    FVector AimCameraRelativeLocation = FVector(0.0f, 100.0f, 50.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float BaseWalkSpeed = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float SprintWalkSpeed = 600.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float SprintMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Stamina")
    float StaminaDrainRate = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Stamina")
    float StaminaRegenRate = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Stamina")
    float StaminaRegenDelay = 2.0f;

    void OnInteractableEnter(AActor* Actor);
    void OnInteractableExit(AActor* Actor);

    void OpenContainerScreen(AStorageContainer* Container);
    void OpenLevelChangeScreen(ALevelChanger* Changer);

    UInventoryGridComponent* GetInventoryComponent() const { return InventoryComponent; }
    UPlayerStatusComponent* GetStatusComponent() const { return StatusComponent; }
    UEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }
    UQuickSlotComponent* GetQuickSlotComponent() const { return QuickSlotComponent; }

    // 소비 아이템의 즉시 효과를 스탯에 적용한다. 우클릭 사용과 퀵슬롯 사용이 공유한다.
    void UseConsumable(UConsumableItemData* ConsumableData);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    void ReloadAmmoAttach();

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    void ReloadAmmoDetach();

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    void ReloadNewAmmo();

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    void ReloadNewAmmoAttach();

private:
    bool bIsInvetoryOpened = false;
};