// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../AI/SimpleBehaviorTree.h"
#include "CompanionAIComponent.generated.h"

class AAIController;
class UCompanionCombatComponent;
class UCompanionPerceptionComponent;

// EnemyBase와 같은 방식(TBTNode<UCompanionAIComponent> 커스텀 트리)으로 Idle/Follow/
// MoveToLocation/MoveToTarget(Combat)/Attack 상태를 매 틱 평가하고 AIController 이동을 호출한다.
// 명령(CommandRouter)과 지각(Perception)이 세팅하는 bool 플래그로 상태 전이를 표현한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionAIComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float FollowDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float MinFollowDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float MoveAcceptanceRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float MoveRequestInterval = 0.25f;

	// 적을 시야에 두면(Perception) 자동으로 교전 상태로 전환할지 여부. Stop 명령은 이 값과 무관하게
	// bCombatSuppressed를 세워 강제로 교전을 억제한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	bool bAutoEngageEnabled = true;

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandFollow();

	// 최우선으로 관철되는 정지 명령: 이동/교전을 모두 즉시 중단하고 재교전을 억제한다.
	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandStop();

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandMoveToLocation(const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandMoveToActor(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandEngage();

	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	bool IsCombatEngaged() const { return bCombatEngaged; }

	// companion.FollowDistance 콘솔 변수로 덮어써진 경우 그 값을, 아니면 FollowDistance를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	float GetEffectiveFollowDistance() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TSharedPtr<TBTNode<UCompanionAIComponent>> BehaviorTreeRoot;

	TWeakObjectPtr<AAIController> CachedAIController;
	TWeakObjectPtr<APawn> CachedPlayerPawn;
	TWeakObjectPtr<UCompanionCombatComponent> CombatComponent;
	TWeakObjectPtr<UCompanionPerceptionComponent> PerceptionComponent;

	bool bFollowEnabled = true;
	bool bHasCommandedDestination = false;
	bool bCommandedDestinationIsActor = false;
	bool bCombatEngaged = false;
	bool bCombatSuppressed = false;

	FVector CommandedLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> CommandedTargetActor;

	float MoveRequestTimer = 0.0f;
	FVector LastMoveRequestLocation = FVector::ZeroVector;

	void BuildBehaviorTree();
	AAIController* GetAIController();

	bool HasEnemyTarget() const;
	AActor* GetCurrentEnemyTarget() const;
	bool IsEnemyInAttackRangeWithLineOfSight() const;

	EBTNodeResult DoAttack(float DeltaTime);
	EBTNodeResult DoMoveToEnemy(float DeltaTime);
	EBTNodeResult DoMoveToCommanded(float DeltaTime);
	EBTNodeResult DoFollow(float DeltaTime);
	EBTNodeResult DoIdle(float DeltaTime);

	void RequestMoveToActor(AActor* Target, float AcceptRadius);
	void RequestMoveToLocation(const FVector& Location, float AcceptRadius);
};
