#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "../AI/SimpleBehaviorTree.h"

struct FBranchingPointNotifyPayload;
#include "EnemyBase.generated.h"

class UAIPerceptionStimuliSourceComponent;
class UAnimMontage;
class UBoxComponent;
class UItemDataBase;
class USoundBase;

UCLASS()
class PROTOPROJECT_API AEnemyBase : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemyBase();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    static void ToggleEnemySoundsEnabled();
    static bool AreEnemySoundsEnabled();

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    bool HasTarget() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    bool IsTargetInAttackRange() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    bool CanAttack() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    virtual void Attack();

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    virtual void MoveToTarget();

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    virtual void Patrol();

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    virtual void Die();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    virtual void TakeEnemyDamage(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    virtual void OnHit(float DamageAmount);

    UFUNCTION(BlueprintPure, Category = "Enemy|Stats")
    bool IsDead() const { return bIsDead; }

    // 기본값은 false를 반환한다. Caller 타입(AEnemyCaller)에서만 오버라이드되어 실제로 동작한다.
    UFUNCTION(BlueprintPure, Category = "Enemy|Call")
    virtual bool CanCall() const;

    // 기본 구현은 아무것도 하지 않는다. Caller 타입(AEnemyCaller)에서만 오버라이드되어
    // 주변 CallRadius 안의 타겟 없는 살아있는 좀비들을 자신과 같은 타겟으로 즉시 반응시킨다.
    UFUNCTION(BlueprintCallable, Category = "Enemy|Call")
    virtual void DoCall();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void BeginAttackHitWindow();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void EndAttackHitWindow();

    // Caller가 주변 좀비를 호출할 때 사용: 죽지 않았다면 타겟을 강제로 세팅한다.
    UFUNCTION(BlueprintCallable, Category = "Enemy|Call")
    void ReceiveCallTarget(AActor* NewTarget);

    // AEnemyCaller overrides these to report ITS OWN CallRadius/CallCooldown
    // (this base class has no such fields itself -- only AEnemyCaller does).
    // Used only when registering with the server (SendEnemyRegister, see
    // BeginPlay) so the server-driven call sweep (EnemyAI::Tick) knows
    // whether/how this enemy should call others. The base defaults
    // (false/0/0) are only ever actually read for a non-Caller subclass,
    // where the server ignores them anyway (is_caller=false).
    virtual bool IsCallerType() const { return false; }
    virtual float GetCallRadius() const { return 0.0f; }
    virtual float GetCallCooldown() const { return 0.0f; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
    TObjectPtr<UAIPerceptionStimuliSourceComponent> PerceptionStimuliSource;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Stats")
    float CurrentHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    float AttackDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    float AttackCooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
    float AttackMoveSpeed = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
    float AttackAcceleration = 4096.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
    TObjectPtr<UBoxComponent> LeftHandAttackBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    FVector LeftHandAttackBoxExtent = FVector(18.0f, 18.0f, 18.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
    float LastAttackTime = -999.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
    bool bIsAttacking = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float SightRange = 1200.0f;

    // 정면 기준 좌우로 이 각도(도) 안에 들어와야 시야에 포착된 것으로 인정한다(전체 시야각).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float SightAngle = 110.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float MoveAcceptanceRadius = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float MoveRequestInterval = 0.25f;

    // 걷는 좀비는 낮게, 뛰는 좀비는 높게 잡아 타입별 이동속도를 구분한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float MoveSpeed = 300.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot")
    bool bUseCombatSlots = true;

    // Target distance where the enemy stops chasing the actor directly and reserves a surrounding combat slot.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0"))
    float SlotClaimDistance = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "1"))
    int32 SlotFootprint = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0"))
    float SlotFirstRingRadius = 170.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0"))
    float SlotRingSpacing = 120.0f;

    // Keeps the first combat ring inside melee reach even when AttackRange is tuned shorter than the visual slot radius.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0"))
    float SlotAttackRangePadding = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0"))
    float SlotMoveAcceptanceRadius = 65.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0"))
    float SlotNavigationProjectionExtent = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Stuck", meta = (ClampMin = "0.0"))
    float SlotStuckTimeout = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Stuck", meta = (ClampMin = "0.0"))
    float SlotStuckVelocityThreshold = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Stuck", meta = (ClampMin = "0.0"))
    float SlotStuckProgressTolerance = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Stuck", meta = (ClampMin = "0.0"))
    float SlotReclaimDelay = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.1"))
    float SlotRecheckInterval = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Stuck", meta = (ClampMin = "0.1"))
    float SlotBlockedRetryDelay = 3.0f;

    // Resume movement only after leaving the arrival radius by this margin.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0"))
    float SlotDepartureMargin = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float SlotAttackFacingAngle = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "0.1"))
    float SlotFacingInterpSpeed = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot", meta = (ClampMin = "1"))
    int32 MaxSlotRings = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Debug")
    bool bDrawCombatSlots = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Debug", meta = (ClampMin = "0.0"))
    float CombatSlotDebugSphereRadius = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat Slot|Debug", meta = (ClampMin = "0.0"))
    float CombatSlotDebugZOffset = 12.0f;

    // 처치 시 이 중 하나를 랜덤으로 드랍한다(비어있으면 드랍 없음).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Loot")
    TArray<TObjectPtr<UItemDataBase>> LootTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LootDropChance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Death")
    bool bEnableRagdollOnDeath = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Death", meta = (ClampMin = "0.0"))
    float RagdollLifeSpan = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Sound")
    TObjectPtr<USoundBase> IdleSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Sound")
    TObjectPtr<USoundBase> AttackSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Sound")
    TObjectPtr<USoundBase> ScreamSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Sound")
    TObjectPtr<USoundBase> DieSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Sound", meta = (ClampMin = "0.0"))
    float EnemySoundVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Sound", meta = (ClampMin = "0.0"))
    float EnemySoundPitch = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Sound", meta = (ClampMin = "0.0"))
    float IdleSoundInterval = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Debug")
    bool bEnableBehaviorDebug = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Debug")
    float DebugPrintInterval = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Stats")
    bool bIsDead = false;

    void BuildBehaviorTree();
    void UpdateTarget();
    void PrintBehaviorDebug(const FString& Message, const FColor& Color = FColor::Cyan);
    void SpawnLoot();
    void PlayEnemySound(USoundBase* Sound) const;
    void PlayIdleSoundIfReady();
    void PauseMovementForMontage(UAnimMontage* Montage);
    void RestoreMovementAfterMontage(UAnimMontage* Montage = nullptr);
    void UpdateCombatSlotClaim();
    void ReleaseCombatSlot();
    bool HasCombatSlot() const;
    FVector GetCombatSlotLocation() const;
    float GetCombatSlotRingRadius(int32 Ring) const;
    bool ProjectCombatSlotLocation(const FVector& RawLocation, FVector& OutLocation) const;
    bool TryClaimCombatSlot(bool bInnerOnly = false);
    FVector GetRawCombatSlotLocation(int32 Index) const;
    bool EvaluateCombatSlot(int32 Index, FVector& OutLocation, float& OutPathLength) const;
    bool FindCombatSlotPath(const FVector& Location, float& OutLength) const;
    bool IsCombatSlotAttackReady() const;
    float GetCombatSlotArrivalRadius() const;
    void RejectCombatSlot(const TCHAR* Reason);
    void UpdateCombatSlotArrival();
    bool CanUseCombatSlotsForCurrentTarget() const;
    void UpdateCombatSlotStuck(float DeltaTime);
    void ResetCombatSlotStuckTracking();
    void DrawCombatSlotsDebug() const;
    // 거리(SightRange)는 이미 통과했다고 가정하고, 시야각 + 장애물 차단(라인오브사이트)만 검사한다.
    bool CanSeeCandidate(const AActor* Candidate) const;

    UFUNCTION()
    void OnAttackBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    UFUNCTION()
    void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // Stable id shared across every client's copy of this level, derived
    // from the placed actor's own in-level name -- same idea as
    // AItemContainerBase::GetContainerId. Used to key the server-mediated
    // AI ownership claim (see SendEnemyClaimRequest).
    int32 GetEnemyId() const;

private:
    friend class FEnemyCombatSlotStateTest;

    UFUNCTION()
    void HandleEnemyClaimResult(int32 EnemyId, bool bGranted);

    UFUNCTION()
    void HandleEnemyState(int32 EnemyId, FVector Position, FRotator Look, float Health, bool bIsDeadState);

    UFUNCTION()
    void HandleEnemyOwnerLeft(int32 EnemyId);

    UFUNCTION()
    void HandleEnemyDamage(int32 EnemyId, float Damage);

    // Bound unconditionally (any client with this enemy_id placed, owner
    // or not) to UProtoNetClientSubsystem::OnEnemyAttackBroadcast -- the
    // server's EnemyAI landed a hit with this enemy_id, so play the attack
    // animation here too (visual only; see HandleEnemyAttackPlayer on
    // AProtoCharacter for the actual damage, which only reaches the
    // target). Never fires for a claim-owned (Single map) enemy: the
    // server only ever sends this for enemy ids registered via
    // C2S_EnemyRegister.
    UFUNCTION()
    void HandleEnemyAttackBroadcast(int32 EnemyId);

    TSharedPtr<TBTNode<AEnemyBase>> BehaviorTreeRoot;
    float DebugPrintTimer = 0.0f;
    float MoveRequestTimer = 0.0f;
    float LastIdleSoundTime = -999.0f;
    float DefaultMaxAcceleration = 2048.0f;
    FString LastBehaviorDebugMessage;
    TSet<TWeakObjectPtr<AActor>> DamagedActorsThisSwing;
    TObjectPtr<UAnimMontage> MovementPausedMontage = nullptr;
    float SavedMontageMaxWalkSpeed = 0.0f;
    float SavedMontageMaxAcceleration = 0.0f;
    bool bMovementPausedForMontage = false;
    TWeakObjectPtr<AActor> CombatSlotTarget;
    int32 CombatSlotIndex = INDEX_NONE;
    float CombatSlotStuckTimer = 0.0f;
    float CombatSlotLastDistance = -1.0f;
    float CombatSlotReclaimBlockTimer = 0.0f;
    float CombatSlotRecheckTimer = 0.0f;
    float CombatSlotProjectionTimer = 0.0f;
    float CombatSlotWaitStartedAt = 0.0f;
    bool bCombatSlotArrived = false;
    FVector CombatSlotLocation = FVector::ZeroVector;
    FVector CombatSlotProgressLocation = FVector::ZeroVector;
    TWeakObjectPtr<AActor> CombatSlotSearchTarget;
    TMap<int32, float> BlockedCombatSlots;
    FString CombatSlotStatus = TEXT("No slot");
    // Whether THIS client's copy is the one actually running the behavior
    // tree/pathing for this enemy (see HandleEnemyClaimResult). Defaults to
    // true so offline/not-yet-connected play behaves exactly as before this
    // feature existed -- it only ever flips to false, and only once a
    // claim comes back denied (i.e. some other client already owns it).
    bool bIsNetworkOwner = true;

    // Throttled position/facing/health reporting to the server, same idea
    // as AProtoCharacter's NetSyncInterval. Only sent while bIsNetworkOwner.
    float NetSyncTimer = 0.0f;
    static constexpr float NetSyncInterval = 0.15f;

    // Latest position/facing HandleEnemyState was told about, for a
    // non-owned enemy (server-driven, or another client's claimed copy).
    // Tick()'s !bIsNetworkOwner branch walks toward this every frame
    // (AddMovementInput, same technique
    // UProtoNetClientSubsystem::TickRemotePlayers uses for remote players/
    // companions) instead of HandleEnemyState teleporting straight to it
    // with SetActorLocationAndRotation -- a raw teleport skips
    // CharacterMovementComponent's floor sweep/gravity (visible as the
    // enemy floating whenever the server's tracked Z doesn't exactly match
    // this client's copy of the ground) and produces no real velocity for
    // the walk/run animation blend (visible as it sliding in place instead
    // of playing its walk cycle).
    FVector MirroredTargetLocation = FVector::ZeroVector;
    FRotator MirroredTargetRotation = FRotator::ZeroRotator;
    bool bHasMirroredTarget = false;

    // Turn rate (see FMath::RInterpTo) Tick() uses to rotate toward
    // MirroredTargetRotation instead of snapping straight to it. The server
    // only broadcasts S2C_EnemyState every NetSyncInterval-ish 150ms (see
    // EchoServer::kTickInterval), so a hard SetActorRotation() every time a
    // new one arrives visibly whips the enemy's facing around in discrete
    // ~150ms jumps -- confirmed jerky in a live multiplayer test. Position
    // doesn't need the same treatment: it's already smoothed by walking
    // there continuously via AddMovementInput below, same as
    // UProtoNetClientSubsystem::TickRemotePlayers.
    static constexpr float MirroredRotationInterpSpeed = 10.0f;
};






