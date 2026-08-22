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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float SightRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float LoseSightRadius = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float PeripheralVisionAngleDegrees = 90.0f;

	// 임계값을 넘어야 "체력 낮음"으로 판단(0~1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Perception")
	float LowHealthThresholdPct = 0.3f;

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

	UFUNCTION()
	void HandlePerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandlePlayerHealthChanged(float NewValue, float MaxValue);
};
