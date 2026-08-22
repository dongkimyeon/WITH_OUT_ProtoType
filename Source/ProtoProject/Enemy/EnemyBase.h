#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "../AI/SimpleBehaviorTree.h"
#include "EnemyBase.generated.h"

class UAIPerceptionStimuliSourceComponent;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
    float LastAttackTime = -999.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float SightRange = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float MoveAcceptanceRadius = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI")
    float MoveRequestInterval = 0.25f;

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

private:
    TSharedPtr<TBTNode<AEnemyBase>> BehaviorTreeRoot;
    float DebugPrintTimer = 0.0f;
    float MoveRequestTimer = 0.0f;
    FString LastBehaviorDebugMessage;
};
