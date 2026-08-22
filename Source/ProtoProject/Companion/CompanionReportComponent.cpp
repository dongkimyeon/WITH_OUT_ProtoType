// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionReportComponent.h"
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
