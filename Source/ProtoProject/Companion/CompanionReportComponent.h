// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompanionReportComponent.generated.h"

class UCompanionAIComponent;
class UCompanionPerceptionComponent;
class UCompanionSpeechComponent;

// Perception 스냅샷 변화를 감시해 우선순위/스로틀을 적용한 뒤 Gemini를 거치지 않고
// SpeechComponent::Speak()을 직접 호출해 즉각적인 상황 보고를 한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionReportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionReportComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Report")
	float MinReportInterval = 20.0f;

	// 같은 적을 재감지해도 이 시간(초) 안에는 다시 보고하지 않는다(perception noise로 인한 스팸 방지).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Report")
	float EnemyForgetTime = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Report")
	FString EnemySpottedLine = TEXT("적 발견!");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Report")
	FString PlayerHealthLowLine = TEXT("체력이 낮아요, 조심하세요!");

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<UCompanionAIComponent> AIComponent;
	TWeakObjectPtr<UCompanionPerceptionComponent> PerceptionComponent;
	TWeakObjectPtr<UCompanionSpeechComponent> SpeechComponent;

	float LastReportTime = -999.0f;
	TMap<TWeakObjectPtr<AActor>, float> LastReportedEnemyTime;

	UFUNCTION()
	void HandleEnemySpotted(AActor* EnemyActor);

	UFUNCTION()
	void HandlePlayerHealthLow();

	bool TryReport(const FString& Text);
};
