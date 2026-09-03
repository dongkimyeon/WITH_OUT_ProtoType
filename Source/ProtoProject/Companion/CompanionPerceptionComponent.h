// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "CompanionPerceptionComponent.generated.h"

class UPlayerStatusComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionEnemySpotted, AActor*, EnemyActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionEnemyLost, AActor*, EnemyActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCompanionPlayerHealthLow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCompanionPlayerHealthRecovered);

// UAIPerceptionComponent를 상속해 시야 감지(적)와 플레이어 UPlayerStatusComponent 델리게이트 구독을
// 하나로 묶어 "현재 상황" 스냅샷을 유지한다. CompanionCommandRouter/CompanionAIComponent/
// CompanionReportComponent/CompanionBrainComponent가 모두 이 컴포넌트를 참조해 상황을 읽는다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionPerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	UCompanionPerceptionComponent();

	// 참고: CompanionCombatComponent::AttackRange 기본값(2000)보다 넉넉하게 커야 사거리 안에
	// 들어온 적을 놓치지 않고 미리 발견할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float SightRadius = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float LoseSightRadius = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float PeripheralVisionAngleDegrees = 120.0f;

	// 임계값을 넘어야 "체력 낮음"으로 판단(0~1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float LowHealthThresholdPct = 0.3f;

	// 시야각 경계에서의 순간적인 감지 끊김을 흡수하기 위한 유예시간. 마지막 성공 감지 후 이 시간
	// 동안은 CurrentEnemyTarget을 유지하고, 초과해야 실제로 OnEnemyLost를 브로드캐스트한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float EnemyLostGraceTime = 1.0f;

	// 플레이어가 조준 중인 적을 찾을 때 카메라 전방으로 쏘는 스피어트레이스의 사거리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float PlayerAimTraceRange = 5000.0f;

	// 위 트레이스의 반경. 완전히 정조준하지 않아도(화면 중앙 근처) 인정되도록 어느 정도 여유를 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float PlayerAimTraceRadius = 25.0f;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Perception")
	FOnCompanionEnemySpotted OnEnemySpotted;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Perception")
	FOnCompanionEnemyLost OnEnemyLost;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Perception")
	FOnCompanionPlayerHealthLow OnPlayerHealthLow;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Perception")
	FOnCompanionPlayerHealthRecovered OnPlayerHealthRecovered;

	UFUNCTION(BlueprintPure, Category = "Companion|Perception")
	AActor* GetCurrentEnemyTarget() const { return CurrentEnemyTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Companion|Perception")
	bool HasEnemyTarget() const { return CurrentEnemyTarget.IsValid(); }

	// Brain의 system_instruction / Report에 넣을 실시간 상황 요약.
	UFUNCTION(BlueprintPure, Category = "Companion|Perception")
	FString BuildSituationSummary() const;

	// companion.SightRadius 콘솔 변수로 덮어써진 경우 그 값을, 아니면 SightRadius를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Companion|Perception")
	float GetEffectiveSightRadius() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<class UAISenseConfig_Sight> SightSenseConfig;

	TWeakObjectPtr<AActor> CurrentEnemyTarget;
	TWeakObjectPtr<class UPlayerStatusComponent> PlayerStatus;
	bool bLowHealthReported = false;

	// 현재 감지 중인 모든 적과 각각의 마지막 성공 감지 시각. 시야각 경계에서의 순간적인 감지 끊김을
	// 개별 적 단위로 흡수하기 위해(EnemyLostGraceTime), 단일 CurrentEnemyTarget이 아니라 목록으로
	// 유지한다 - 그래야 여러 적이 보일 때 그중 하나(플레이어 조준 대상 등)를 우선순위로 고를 수 있다.
	TMap<TWeakObjectPtr<AActor>, float> SensedEnemyLastSeenTime;

	UFUNCTION()
	void HandlePerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandlePlayerHealthChanged(float NewValue, float MaxValue);

	// 플레이어 카메라 전방을 스피어트레이스해 조준 중인 적(살아있는 AEnemyBase)을 찾는다. 없으면 nullptr.
	AActor* FindPlayerAimedEnemy() const;

	// SensedEnemyLastSeenTime 중에서 우선순위(1.플레이어 조준 대상 2.기존 타겟 유지 3.최단거리)로
	// 다음 CurrentEnemyTarget이 될 적을 고른다. 감지된 적이 하나도 없으면 nullptr.
	AActor* ResolveBestTarget() const;
};
