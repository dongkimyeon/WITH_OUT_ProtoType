// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionReportComponent.h"
#include "CompanionAIComponent.h"
#include "CompanionPerceptionComponent.h"
#include "CompanionSpeechComponent.h"

UCompanionReportComponent::UCompanionReportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCompanionReportComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		AIComponent = Owner->FindComponentByClass<UCompanionAIComponent>();
		PerceptionComponent = Owner->FindComponentByClass<UCompanionPerceptionComponent>();
		SpeechComponent = Owner->FindComponentByClass<UCompanionSpeechComponent>();
	}

	if (PerceptionComponent.IsValid())
	{
		PerceptionComponent->OnEnemySpotted.AddDynamic(this, &UCompanionReportComponent::HandleEnemySpotted);
		PerceptionComponent->OnPlayerHealthLow.AddDynamic(this, &UCompanionReportComponent::HandlePlayerHealthLow);
	}
}

bool UCompanionReportComponent::TryReport(const FString& Text)
{
	if (!SpeechComponent.IsValid())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastReportTime < MinReportInterval)
	{
		return false;
	}

	LastReportTime = Now;
	SpeechComponent->Speak(Text);
	return true;
}

void UCompanionReportComponent::HandleEnemySpotted(AActor* EnemyActor)
{
	// 이미 전투 중이면(교전 대상만 계속 바뀌는 상황 포함) 매번 "적 발견!"을 새로 외칠 필요가 없다 -
	// 감지 범위가 넓어진 뒤로 무리 지어 있는 적들 사이에서 타겟이 자주 바뀌며 스팸처럼 들리는 걸 막는다.
	if (AIComponent.IsValid() && AIComponent->IsCombatEngaged())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	if (const float* LastTime = LastReportedEnemyTime.Find(EnemyActor))
	{
		if (Now - *LastTime < EnemyForgetTime)
		{
			return;
		}
	}

	if (TryReport(EnemySpottedLine))
	{
		LastReportedEnemyTime.Add(EnemyActor, Now);
	}
}

void UCompanionReportComponent::HandlePlayerHealthLow()
{
	TryReport(PlayerHealthLowLine);
}
