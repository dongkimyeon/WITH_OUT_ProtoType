// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionPerceptionComponent.h"
#include "../PlayerContent/PlayerStatusComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Kismet/GameplayStatics.h"
#include "../Enemy/EnemyBase.h"

namespace
{
	// PIE 중 콘솔(~)에서 "companion.SightRadius 3000"처럼 입력하면 모든 Companion의 시야 반경을
	// 즉시 덮어쓴다. 음수(기본값)면 각 인스턴스의 SightRadius 프로퍼티를 그대로 사용한다.
	TAutoConsoleVariable<float> CVarCompanionSightRadius(
		TEXT("companion.SightRadius"),
		-1.0f,
		TEXT("0 이상이면 모든 Companion의 SightRadius를 이 값으로 덮어쓴다(디버그용, PIE 중 실시간 반영)."),
		ECVF_Cheat);
}

UCompanionPerceptionComponent::UCompanionPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SightSenseConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightSenseConfig->SightRadius = SightRadius;
	SightSenseConfig->LoseSightRadius = LoseSightRadius;
	SightSenseConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightSenseConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightSenseConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightSenseConfig->DetectionByAffiliation.bDetectFriendlies = false;

	ConfigureSense(*SightSenseConfig);
	SetDominantSense(SightSenseConfig->GetSenseImplementation());
}

void UCompanionPerceptionComponent::BeginPlay()
{
	Super::BeginPlay();

	OnTargetPerceptionUpdated.AddDynamic(this, &UCompanionPerceptionComponent::HandlePerceptionUpdated);

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UPlayerStatusComponent* Status = PlayerPawn->FindComponentByClass<UPlayerStatusComponent>())
		{
			PlayerStatus = Status;
			Status->OnHealthChanged.AddDynamic(this, &UCompanionPerceptionComponent::HandlePlayerHealthChanged);
		}
	}
}

void UCompanionPerceptionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// SightRadius(EditAnywhere)나 companion.SightRadius CVar를 PIE 중에 바꿔도 즉시 반영되도록
	// 실제 감지에 쓰이는 SenseConfig에 매 틱 재적용한다(값 몇 개 복사하는 정도라 비용은 무시할 수준).
	if (SightSenseConfig)
	{
		const float EffectiveSightRadius = GetEffectiveSightRadius();
		SightSenseConfig->SightRadius = EffectiveSightRadius;
		SightSenseConfig->LoseSightRadius = FMath::Max(EffectiveSightRadius, LoseSightRadius);
		SightSenseConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	}

	// 시야각 경계에서 한두 틱만 스치듯 벗어나는 노이즈를 흡수: 유예시간을 넘겨야 실제로 소실 처리한다.
	if (CurrentEnemyTarget.IsValid())
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : LastSensedTime;
		if (Now - LastSensedTime > EnemyLostGraceTime)
		{
			AActor* Lost = CurrentEnemyTarget.Get();
			CurrentEnemyTarget = nullptr;
			OnEnemyLost.Broadcast(Lost);
		}
	}
}

float UCompanionPerceptionComponent::GetEffectiveSightRadius() const
{
	const float Override = CVarCompanionSightRadius.GetValueOnGameThread();
	return Override >= 0.0f ? Override : SightRadius;
}

void UCompanionPerceptionComponent::HandlePerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus)
{
	// 시체/비적대 액터 오탐 방지: Enemy 타입이면서 아직 살아있는 경우만 타겟으로 인정한다.
	AEnemyBase* Enemy = Cast<AEnemyBase>(UpdatedActor);
	if (!Enemy || Enemy->IsDead())
	{
		if (CurrentEnemyTarget.Get() == UpdatedActor)
		{
			CurrentEnemyTarget = nullptr;
			OnEnemyLost.Broadcast(UpdatedActor);
		}
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		const bool bIsNewSighting = !CurrentEnemyTarget.IsValid();
		CurrentEnemyTarget = UpdatedActor;
		LastSensedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastSensedTime;
		if (bIsNewSighting)
		{
			OnEnemySpotted.Broadcast(UpdatedActor);
		}
	}
	// 감지 실패는 여기서 즉시 처리하지 않는다. TickComponent의 EnemyLostGraceTime 유예 타이머가
	// 실제 소실 여부(순간적 시야각 이탈 vs 지속적 소실)를 판단해 OnEnemyLost를 브로드캐스트한다.
}

void UCompanionPerceptionComponent::HandlePlayerHealthChanged(float NewValue, float MaxValue)
{
	if (MaxValue <= 0.0f)
	{
		return;
	}

	const float HealthPct = NewValue / MaxValue;
	if (HealthPct <= LowHealthThresholdPct)
	{
		if (!bLowHealthReported)
		{
			bLowHealthReported = true;
			OnPlayerHealthLow.Broadcast();
		}
	}
	else if (bLowHealthReported)
	{
		bLowHealthReported = false;
		OnPlayerHealthRecovered.Broadcast();
	}
}

FString UCompanionPerceptionComponent::BuildSituationSummary() const
{
	TArray<FString> Lines;

	if (PlayerStatus.IsValid())
	{
		const float HealthPct = PlayerStatus->GetMaxHealth() > 0.0f
			? (PlayerStatus->GetHealth() / PlayerStatus->GetMaxHealth()) * 100.0f
			: 0.0f;
		Lines.Add(FString::Printf(TEXT("플레이어 체력 %.0f%%"), HealthPct));
	}

	if (CurrentEnemyTarget.IsValid())
	{
		Lines.Add(TEXT("근처에 적이 있음"));
	}
	else
	{
		Lines.Add(TEXT("근처에 적 없음"));
	}

	return FString::Join(Lines, TEXT(", "));
}
