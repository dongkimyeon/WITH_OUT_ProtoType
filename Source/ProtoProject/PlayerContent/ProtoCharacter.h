#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Interactable.h"
#include "Inventory/EquipmentComponent.h"
#include "../Network/ProtoNetClientSubsystem.h" // FProtoInventoryItemEntry (HandleInventoryRestored)
#include "ProtoCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UInventoryGridComponent;
class UQuickSlotComponent;
class URadialQuickSlotWidget;
class UConsumableItemData;
class AWeaponBase;
class AItemContainerBase;
class UPlayerDefalutUI;
class UContainerScreenWidget;
class UAnimMontage;
class UAnimSequenceBase;
class UCameraShakeBase;
class UPlayerStatusComponent;
class ULevelChangeSelectWidget;
class ALevelChanger;
class ACompanionNPC;
class SProtoDebugPanel;
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

    // 명시적으로 선언해서 .cpp(ProtoDebugPanel.h가 완전히 포함된 곳)에서만 정의한다 - 헤더에는
    // SProtoDebugPanel을 전방 선언만 해뒀는데, 소멸자가 암시적으로 생성되면 이 클래스를 생성/파괴
    // 하는 다른 모든 TU에서 TSharedPtr<SProtoDebugPanel> 멤버를 파괴하려다 불완전 타입 에러가 난다.
    virtual ~AProtoCharacter() override;

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Caches this player's weapon/inventory state into the net subsystem
    // right before a level travel destroys this actor, so the character
    // that spawns in the new level can restore it -- see
    // UProtoNetClientSubsystem::CacheStateForLevelTransition.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

    // 동료 NPC와 PTT(누르고 말하기) 대화를 위한 매핑 컨텍스트/액션.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Companion", meta = (AllowPrivateAccess = "true"))
    UInputMappingContext* CompanionMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Companion", meta = (AllowPrivateAccess = "true"))
    UInputAction* TalkToCompanionAction;

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
    void TalkToCompanionPressed(const FInputActionValue& Value);
    void TalkToCompanionReleased(const FInputActionValue& Value);

    // 이 캐릭터가 로컬로 스폰하는 동료 NPC 클래스. BP_ProtoCharacter 디테일 패널에서 BP_CompanionNPC를
    // 직접 지정해야 한다(생성자에서 자동으로 못 채움 - ProtoCharacter.cpp의 관련 주석 참고).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<ACompanionNPC> CompanionClass;

    // 스폰 시 플레이어 뒤쪽으로 이 거리만큼 떨어진 위치에 동료를 놓는다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion", meta = (AllowPrivateAccess = "true"))
    float CompanionSpawnDistance = 200.0f;

    void SpawnCompanion();

    // 이 캐릭터가 스폰한 동료 NPC(레벨마다 BeginPlay에서 새로 스폰됨).
    UPROPERTY()
    ACompanionNPC* CachedCompanionNPC = nullptr;
    ACompanionNPC* GetCompanionNPC();
    void SetWeaponTypeNone();
    void SetWeaponSlot1();
    void SetWeaponSlot2();
    void SetWeaponFromSlot(EEquipmentSlot Slot);
    void BeginWeaponSwap(EWeaponType TargetWeaponType, AWeaponBase* TargetWeaponActor = nullptr);
    void BeginWeaponToWeaponSwap(EWeaponType TargetWeaponType, AWeaponBase* TargetWeaponActor);
    void FinishWeaponSwap();
    void StartFireWeapon();
    void StopFireWeapon();
    void FireWeapon();
    void ApplyWeaponRecoil();
    void ReloadWeapon();
    void BeginConsumableAnimationState();
    void EndConsumableAnimationState();

    UFUNCTION()
    void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    UFUNCTION()
    void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void AttachCurrentWeaponToSocket(FName SocketName);
    AWeaponBase* GetWeaponByType(EWeaponType WeaponType) const;

    void DebugCommandCompanionEngage();
    void DebugCommandCompanionExplore();
    void DebugCommandCompanionEquipWeapon1();
    void DebugCommandCompanionEquipWeapon2();
    void DebugCommandCompanionHolsterWeapon();
    void DebugCommandCompanionJump();
    void DebugCommandCompanionReload();
    void ToggleEnemySoundsDebug();
    void DebugDecreaseHealth();
    void DebugDecreaseHunger();
    void DebugDecreaseThirst();
    void DebugIncreaseInfection();
    void DebugDecreaseStamina();

    // 위 Debug* 함수들 + companion.* CVar 튜닝값을 한 곳에 모아 클릭으로 조작하는 Slate 패널.
    // 0번 키로 열고 닫는다(SetupPlayerInputComponent 참고). 패널은 순수 UI만 담당하고, 실제
    // 액션 목록(무엇을 누르면 무슨 함수가 불리는지)은 여기 ProtoCharacter.cpp가 구성해 넘겨준다.
    void ToggleDebugPanel();

#if !UE_BUILD_SHIPPING
    TSharedPtr<SProtoDebugPanel> DebugPanelWidget;
#endif

    // PIE 콘솔(~)에서 "die" 입력 시 즉시 사망(실제 사망 경로와 동일: 래그돌 + 지닌 것 소실 +
    // RaidManager가 있으면 사망 화면 + 허브 복귀).
    UFUNCTION(Exec)
    void Die();

    void UpdateStamina(float DeltaTime);

    // 소비 아이템의 OverTime 효과를 반복 타이머로 서서히 적용한다 (자가 종료).
    void ApplyOverTimeStatEffect(EConsumableTargetStat TargetStat, float TotalAmount, float Duration);

    // OverTime 반복 타이머가 끝나면 호출되어 진행 상태를 정리한다.
    void OnOverTimeEffectFinished();

    // OverTime 회복이 진행 중인 동안에는 (같은 아이템이라도) 다른 소비 아이템 사용을 막는다.
    bool bOverTimeEffectActive = false;

    bool bIsUsingConsumable = false;
    bool bSavedConsumableSwappingAlpha = false;
    bool bHiddenConsumableWeaponWasHidden = false;

    UPROPERTY()
    AWeaponBase* HiddenConsumableWeapon = nullptr;
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

    // Fallback weapon classes for remote characters (see SpawnFallbackRemoteWeapon):
    // a remote proxy's CurrentRifle/CurrentPistol never get populated through
    // HandleEquipmentChanged, since that's only ever driven by this player's
    // own inventory/input -- a remote character never equips anything
    // locally. Defaults wired to the same Blueprints the inventory system
    // spawns (BP_AK47 / Pistol); overridable per-Blueprint if needed.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TSubclassOf<AWeaponBase> RemoteRifleClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TSubclassOf<AWeaponBase> RemotePistolClass;

    // Spawns+attaches (to the storage socket, holstered) a weapon actor of
    // WeaponType using RemoteRifleClass/RemotePistolClass, and remembers it
    // in CurrentRifle/CurrentPistol so subsequent GetWeaponByType() calls
    // find it. Only meant for a remote character (no controller) whose
    // weapon slot was never filled through the inventory system. Returns
    // nullptr for EWeaponType::None or if no fallback class is set.
    AWeaponBase* SpawnFallbackRemoteWeapon(EWeaponType WeaponType);

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

    // Mirrors StartAim()/StopAim() for a remote character -- bIsAiming and
    // AimPitch drive ABP_Unarmed_Test's aim offset, but for a remote proxy
    // (no Controller) nothing was ever setting them, so every remote
    // character's aim state just sat at its default instead of tracking the
    // sender. Called from UProtoNetClientSubsystem::UpdateRemotePlayer with
    // the ADS bit + look pitch out of S2C_MoveState -- see
    // UProtoNetClientSubsystem::kMoveFlagADS.
    UFUNCTION(BlueprintCallable, Category = "Aim")
    void SetRemoteAiming(bool bAiming, float Pitch);

    // Bound (locally-controlled instance only, in BeginPlay) to
    // UProtoNetClientSubsystem::OnProgressRestored (S2C_LoginSuccess only).
    // 어댑터: 서버 저장 위치이므로 bApplyTransform=true로 HandleProgressRestored 호출.
    UFUNCTION()
    void HandleProgressRestoredFromServer(FVector Position, FRotator Look, uint8 WeaponType);

    // bApplyTransform=true: 저장된 MSSQL 위치로 이동 + 저장 무기 표시(초기 스폰 상태, 스왑 애니 없음).
    // bApplyTransform=false: 레벨 이동 이월 -- 위치/시선은 건드리지 않고(목적지 PlayerStart 유지)
    //   무기 비주얼만 복원. CurrentRifle/CurrentPistol이 아직이면 SpawnFallbackRemoteWeapon() 폴백.
    void HandleProgressRestored(FVector Position, FRotator Look, uint8 WeaponType, bool bApplyTransform);

    /*-------------------
     네트워킹: 인벤토리 동기화 (로컬 플레이어만)
    -------------------*/
    // Bound to UProtoNetClientSubsystem::OnInventoryRestored: rebuilds the
    // saved grid by resolving each entry's item asset name back to a
    // UItemDataBase* (see ResolveItemDataByName) and calling AddItemAt().
    UFUNCTION()
    void HandleInventoryRestored(const TArray<FProtoInventoryItemEntry>& Items);

    // Bound to InventoryComponent->OnInventoryChanged: pushes the whole
    // current grid to the server so it's never more than one change behind.
    // No-op while bIsRestoringInventory is true, so applying a restore
    // doesn't immediately re-save the exact same data right back.
    UFUNCTION()
    void HandleInventoryChanged();

    // Same "push the full current contents, no-op while restoring" idea as
    // HandleInventoryChanged, for EquipmentComponent's/QuickSlotComponent's
    // own separate storage -- bound to their OnEquipmentChanged/
    // OnQuickSlotChanged (a second listener alongside HandleEquipmentChanged's
    // existing weapon-mesh-visual one; multicast delegates support both).
    // Guarded by bIsRestoringEquipment rather than bIsRestoringInventory:
    // RestoreEquipmentAndQuickSlots runs after HandleInventoryRestored has
    // already finished (and reset bIsRestoringInventory), so that flag is
    // already back to false by the time these would fire.
    UFUNCTION()
    void HandleEquipmentChangedForSave(EEquipmentSlot ChangedSlot);

    UFUNCTION()
    void HandleQuickSlotChangedForSave(int32 SlotIndex);

    // Bound (locally-controlled instance only, in BeginPlay) to
    // UProtoNetClientSubsystem::OnEnemyAttackPlayer -- a server-driven
    // (Multi map) zombie's own melee timing landed on this player. The
    // server decided if/when/how much (see S2C_EnemyAttackResult's schema
    // comment); this just applies it, same as the old local overlap path did.
    UFUNCTION()
    void HandleEnemyAttackPlayer(int32 EnemyId, float Damage);

    // UPlayerStatusComponent::OnPlayerDied 구독(로컬 플레이어만). 입력 차단 + 이동 정지 +
    // 지닌 것 소실. 래그돌/사망 애니메이션은 별도 작업 예정. SafePlace 복귀·사망 화면은 ARaidManager.
    // Also reports this to the server (SendPlayerDied) so other clients'
    // mirror of this player ragdolls too -- see HandleRemotePlayerDied.
    UFUNCTION()
    void HandleDeath();

    // Bound (called directly, not through a delegate) from
    // UProtoNetClientSubsystem's S2C_PlayerDied handler on this player's
    // REMOTE mirror instance on every OTHER client. Purely visual (just
    // the ragdoll -- see ApplyRagdollVisual): unlike HandleDeath(), this
    // must NOT touch input/inventory/save-to-server side effects, since a
    // remote mirror has no real controller or populated inventory of its
    // own to safely act on (see this function's .cpp comment for why that
    // was a real risk, not just unnecessary).
    UFUNCTION(BlueprintCallable, Category = "Death")
    void HandleRemotePlayerDied();

    bool IsDead() const { return bIsDead; }

    // Finds a UItemDataBase asset by its own object name (e.g.
    // "DA_Item_AK47") via the Asset Registry -- not by ItemId, which isn't
    // guaranteed to be filled in (see inventory.fbs's item_id comment).
    // Scans the whole content tree the first time it's called and caches
    // the result, so repeated restores don't re-scan.
    UItemDataBase* ResolveItemDataByName(const FString& AssetName) const;

    // Shared by HandleInventoryChanged (network save) and EndPlay
    // (level-transition local cache) -- both need the same "current grid,
    // as wire-format entries" snapshot.
    TArray<FProtoInventoryItemEntry> BuildInventorySnapshot() const;

    // Same idea as BuildInventorySnapshot, for EquipmentComponent's/
    // QuickSlotComponent's own separate storage -- EndPlay-only (unlike the
    // grid, these never went through the server, see
    // ConsumePendingInventoryRestore's comment), so there's no
    // OnInventoryChanged-equivalent save path to share this with.
    TArray<FProtoEquipmentEntry> BuildEquipmentSnapshot() const;
    TArray<FProtoQuickSlotEntry> BuildQuickSlotSnapshot() const;

    // Restores EndPlay's Equipment/QuickSlot snapshots after a level
    // transition. Both APIs only know how to move an item that's already
    // sitting in the grid (EquipFromInventory/RegisterFromInventory), so
    // this adds each one to InventoryComponent first (temporarily) and
    // immediately equips/registers it out again -- reusing that
    // already-tested move logic instead of a separate "set this slot
    // directly" path. Known gap: if the just-restored grid has no room left
    // for one of these, that item is silently dropped (falls back to just
    // not being equipped) rather than displacing something else.
    void RestoreEquipmentAndQuickSlots(const TArray<FProtoEquipmentEntry>& Equipment, const TArray<FProtoQuickSlotEntry>& QuickSlots);

    // 인벤토리 그리드/장비/퀵슬롯 어디에도 DA_Item_AK47이 없으면 그리드에 1정 추가한다.
    // TrySetupLocalPlayerOnce 말미(복원 완료 + 저장 델리게이트 바인딩 후)에서 1회 호출 --
    // 신규 시작과 사망 후 빈손 복귀 모두에서 "기본 무기 1정"을 보장하되, 레벨 이동으로
    // 이미 소지한 AK47이 복원된 경우 중복 지급하지 않는다.
    void GrantStartingRifleIfMissing();

    // 레벨 이동을 시작하는 코드(ULevelChangeSelectWidget::RequestLevelChange,
    // AExitPoint 익스트랙션 성공)가 OpenLevel 직전에 호출한다. 현재 무기/인벤토리/장비/
    // 퀵슬롯/위치를 UProtoNetClientSubsystem에 캐시해 다음 레벨에서 복원되게 한다.
    // EndPlay(LevelTransition)에서도 백업으로 호출되며, 사망 상태면 아무것도 하지 않는다.
    void CacheTravelStateToNetClient();

    bool bIsRestoringInventory = false;

    // See HandleEquipmentChangedForSave/HandleQuickSlotChangedForSave's
    // comment: set for the duration of RestoreEquipmentAndQuickSlots only.
    bool bIsRestoringEquipment = false;

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

    void OpenContainerScreen(AItemContainerBase* Container);
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
    bool bIsDead = false;

    // BeginPlay() calls this once immediately, same as it always has --
    // but IsLocallyControlled() isn't guaranteed to already report true at
    // that exact moment (ARaidManager::TryAcquireLocalPlayer has to poll
    // Tick() for the very same reason: "로컬 플레이어는 아직 스폰 전일 수
    // 있으므로"). If it's not ready yet, BeginPlay leaves
    // bLocalPlayerSetupDone false and Tick() retries this every frame
    // until it succeeds, instead of this character silently never binding
    // OnInventoryChanged (so nothing it does ever gets saved) or consuming
    // its restored position/inventory (so nothing carries over) for its
    // entire lifetime. No-ops (returns false without doing anything) if
    // IsLocallyControlled() still isn't true yet; safe to call repeatedly
    // since it only actually runs once bLocalPlayerSetupDone is set.
    bool bLocalPlayerSetupDone = false;
    bool TrySetupLocalPlayerOnce();

    // 사망 시 지니고 있던 것을 전부 소실시킨다: 그리드 인벤토리 비우기 + 장비 슬롯 ClearAll +
    // 퀵슬롯 ClearAll. 허브의 StorageContainer(스태시)는 별개라 손대지 않는다.
    void WipeCarriedInventoryOnDeath();

    // Shared by HandleDeath() (local) and HandleRemotePlayerDied() (other
    // clients' mirror of this player): capsule collision off, movement
    // disabled, mesh detached and switched to ragdoll physics. Everything
    // else HandleDeath() does (stop fire, disable input, wipe inventory,
    // report to server) is local-player-only and stays out of this helper.
    void ApplyRagdollVisual();
};


