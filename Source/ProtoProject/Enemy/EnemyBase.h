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

UENUM(BlueprintType)
enum class EEnemyType : uint8
{
    Walker,
    Runner,
    Caller
};

UCLASS()
class PROTOPROJECT_API AEnemyBase : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemyBase();

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

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

    UFUNCTION(BlueprintPure, Category = "Enemy|Call")
    bool CanCall() const;

    // 주변 CallRadius 안의 타겟 없는 살아있는 좀비들을 자신과 같은 타겟으로 즉시 반응시킨다.
    UFUNCTION(BlueprintCallable, Category = "Enemy|Call")
    virtual void DoCall();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void BeginAttackHitWindow();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void EndAttackHitWindow();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
    TObjectPtr<UAIPerceptionStimuliSourceComponent> PerceptionStimuliSource;

    // 걷는 좀비/뛰는 좀비/주변 좀비를 불러모으는 좀비를 구분한다. 실제 수치 차이는 이 값과
    // MoveSpeed/AttackDamage 등 인스턴스별 프로퍼티를 블루프린트 자식에서 조정해 표현한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Type")
    EEnemyType EnemyType = EEnemyType::Walker;

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

    // Caller 타입 전용: 이 반경 안의 다른 살아있는 좀비를 즉시 같은 타겟으로 반응시킨다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call", meta = (EditCondition = "EnemyType == EEnemyType::Caller"))
    float CallRadius = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call", meta = (EditCondition = "EnemyType == EEnemyType::Caller"))
    float CallCooldown = 8.0f;

    // 처치 시 이 중 하나를 랜덤으로 드랍한다(비어있으면 드랍 없음).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Loot")
    TArray<TObjectPtr<UItemDataBase>> LootTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LootDropChance = 1.0f;

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

    // 거리(SightRange)는 이미 통과했다고 가정하고, 시야각 + 장애물 차단(라인오브사이트)만 검사한다.
    bool CanSeeCandidate(const AActor* Candidate) const;

    UFUNCTION()
    void OnAttackBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

    UFUNCTION()
    void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
    TSharedPtr<TBTNode<AEnemyBase>> BehaviorTreeRoot;
    float DebugPrintTimer = 0.0f;
    float MoveRequestTimer = 0.0f;
    float LastCallTime = -999.0f;
    FString LastBehaviorDebugMessage;
    TSet<TWeakObjectPtr<AActor>> DamagedActorsThisSwing;
};




