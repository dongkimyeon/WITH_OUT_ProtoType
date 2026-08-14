#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Interactable.h"
#include "Inventory/EquipmentComponent.h"
#include "ProtoCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UInventoryGridComponent;
class UQuickSlotComponent;
class URadialQuickSlotWidget;
class UConsumableItemData;
class AWeaponBase;
class AStorageContainer;
class UPlayerDefalutUI;
class UContainerScreenWidget;
class UAnimMontage;
class UAnimSequenceBase;
class UCameraShakeBase;
class UPlayerStatusComponent;
class ULevelChangeSelectWidget;
class ALevelChanger;
struct FBranchingPointNotifyPayload;
enum class EConsumableTargetStat : uint8;

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
    void SetWeaponSlot1();
    void SetWeaponSlot2();
    void SetWeaponFromSlot(EEquipmentSlot Slot);
    void BeginWeaponSwap(EWeaponType TargetWeaponType, AWeaponBase* TargetWeaponActor = nullptr);
    void FinishWeaponSwap();
    void StartFireWeapon();
    void StopFireWeapon();
    void FireWeapon();
    void ApplyWeaponRecoil();
    void ReloadWeapon();

    UFUNCTION()
    void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    UFUNCTION()
    void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void AttachCurrentWeaponToSocket(FName SocketName);
    AWeaponBase* GetWeaponByType(EWeaponType WeaponType) const;

    void DebugDecreaseHealth();
    void DebugDecreaseHunger();
    void DebugDecreaseThirst();
    void DebugIncreaseInfection();
    void DebugDecreaseStamina();

    void UpdateStamina(float DeltaTime);

    // 소비 아이템의 OverTime 효과를 반복 타이머로 서서히 적용한다 (자가 종료).
    void ApplyOverTimeStatEffect(EConsumableTargetStat TargetStat, float TotalAmount, float Duration);

    // OverTime 반복 타이머가 끝나면 호출되어 진행 상태를 정리한다.
    void OnOverTimeEffectFinished();

    // OverTime 회복이 진행 중인 동안에는 (같은 아이템이라도) 다른 소비 아이템 사용을 막는다.
    bool bOverTimeEffectActive = false;

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

    // 인벤토리 장비 슬롯(Weapon1/Weapon2)에 대응해 스폰된 무기 액터
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TMap<EEquipmentSlot, AWeaponBase*> EquippedWeaponActors;

    // UEquipmentComponent::OnEquipmentChanged 구독 콜백: 인벤토리 장착/해제를 무기 액터 스폰/파괴로 반영
    UFUNCTION()
    void HandleEquipmentChanged(EEquipmentSlot ChangedSlot);

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Animation")
    UAnimSequenceBase* PickupAnimation = nullptr;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Animation")
    FName BandageSectionName = TEXT("Bandage");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Animation")
    FName EatingSectionName = TEXT("eating");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Animation")
    FName DrinkingSectionName = TEXT("Drinking");

    /*-------------------
     네트워킹: 원격 플레이어 시각 동기화
     (UProtoNetClientSubsystem이 S2C_ItemUseBroadcast 수신 시 호출)
    -------------------*/
    // Plays just the reload montage section; unlike ReloadWeapon(), never
    // touches bIsReloading/ammo-attach state (local-player-only concerns).
    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    void PlayRemoteReloadMontage(EWeaponType ForWeaponType);

    // Instantly snaps the held weapon; unlike BeginWeaponSwap(), skips the
    // swap timer/montage (local-input-driven state).
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void ApplyRemoteWeaponEquip(EWeaponType ForWeaponType);

    UFUNCTION(BlueprintCallable, Category = "Interaction|Animation")
    void PlayPickupAnimationIfUnarmed();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim")
    float AimPitch = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|IK")
    FTransform LeftHandTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|IK")
    FVector Joint = FVector(1000.0f, -2000.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|IK")
    bool bDebugLeftHandIK = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float RifleRecoilPitch = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float RifleRecoilYaw = 0.07f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float PistolRecoilPitch = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float PistolRecoilYaw = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
    TSubclassOf<UCameraShakeBase> RifleFireCameraShakeClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
    TSubclassOf<UCameraShakeBase> PistolFireCameraShakeClass;

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

    // 소비 아이템의 효과를 스탯에 적용한다. 우클릭 사용과 퀵슬롯 사용이 공유한다.
    // 다른 OverTime 효과가 이미 진행 중이면(같은 아이템 연타 포함) 실패(false)한다.
    bool UseConsumable(UConsumableItemData* ConsumableData);

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
