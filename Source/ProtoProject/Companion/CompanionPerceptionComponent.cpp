// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionPerceptionComponent.h"
#include "../PlayerContent/PlayerStatusComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "CollisionShape.h"
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
	// 적 개별로 유예시간을 판정하므로(여러 적이 섞여 있어도 각자 따로), 죽었거나 유예시간을 넘긴
	// 항목만 감지 목록에서 정리한다.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (auto It = SensedEnemyLastSeenTime.CreateIterator(); It; ++It)
	{
		const AEnemyBase* Enemy = Cast<AEnemyBase>(It.Key().Get());
		if (!Enemy || Enemy->IsDead() || (Now - It.Value()) > EnemyLostGraceTime)
		{
			It.RemoveCurrent();
		}
	}

	// 플레이어 조준 대상은 AIPerception 스티뮬러스 갱신 없이도(카메라만 돌려도) 매 프레임 바뀔 수
	// 있으므로, 퍼셉션 이벤트를 기다리지 않고 매 틱 우선순위를 다시 계산한다.
	AActor* const PreviousTarget = CurrentEnemyTarget.Get();
	CurrentEnemyTarget = ResolveBestTarget();
	AActor* const NewTarget = CurrentEnemyTarget.Get();

	if (NewTarget && !PreviousTarget)
	{
		OnEnemySpotted.Broadcast(NewTarget);
	}
	else if (!NewTarget && PreviousTarget)
	{
		OnEnemyLost.Broadcast(PreviousTarget);
	}
}

float UCompanionPerceptionComponent::GetEffectiveSightRadius() const
{
	const float Override = CVarCompanionSightRadius.GetValueOnGameThread();
	return Override >= 0.0f ? Override : SightRadius;
}

void UCompanionPerceptionComponent::HandlePerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus)
{
	// 시체/비적대 액터 오탐 방지: Enemy 타입이면서 아직 살아있는 경우만 감지 목록에 남긴다.
	AEnemyBase* Enemy = Cast<AEnemyBase>(UpdatedActor);
	if (!Enemy || Enemy->IsDead())
	{
		SensedEnemyLastSeenTime.Remove(UpdatedActor);
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		SensedEnemyLastSeenTime.Add(UpdatedActor, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	}
	// 감지 실패는 여기서 즉시 제거하지 않는다. TickComponent가 매 틱 EnemyLostGraceTime 유예를
	// 적용해(개별 적 단위로) 실제 소실 여부(순간적 시야각 이탈 vs 지속적 소실)를 판단하고, 그 결과에
	// 따라 우선순위 재계산(ResolveBestTarget)과 OnEnemySpotted/OnEnemyLost 브로드캐스트를 처리한다.
}

AActor* UCompanionPerceptionComponent::FindPlayerAimedEnemy() const
{
	const UWorld* World = GetWorld();
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	APlayerController* PlayerController = PlayerPawn ? Cast<APlayerController>(PlayerPawn->GetController()) : nullptr;
	if (!World || !PlayerController)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * PlayerAimTraceRange;

	FCollisionQueryParams Params(TEXT("CompanionPlayerAim"), false, PlayerPawn);
	Params.AddIgnoredActor(PlayerPawn);

	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(
		Hit,
		ViewLocation,
		TraceEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(PlayerAimTraceRadius),
		Params);

	if (!bHit)
	{
		return nullptr;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(Hit.GetActor());
	if (!Enemy && Hit.GetComponent())
	{
		Enemy = Cast<AEnemyBase>(Hit.GetComponent()->GetOwner());
	}

	return (Enemy && !Enemy->IsDead()) ? Enemy : nullptr;
}

AActor* UCompanionPerceptionComponent::ResolveBestTarget() const
{
	if (SensedEnemyLastSeenTime.Num() == 0)
	{
		return nullptr;
	}

	// 1순위: 플레이어가 조준 중인 적이 현재 감지 목록에 있으면 그 적을 우선한다.
	if (AActor* PlayerAimedEnemy = FindPlayerAimedEnemy())
	{
		if (SensedEnemyLastSeenTime.Contains(PlayerAimedEnemy))
		{
			return PlayerAimedEnemy;
		}
	}

	// 2순위: 기존 타겟이 여전히 감지되고 있으면 유지한다(우선순위가 매 틱 흔들려 타겟이
	// 계속 바뀌는 것을 방지).
	if (AActor* Current = CurrentEnemyTarget.Get())
	{
		if (SensedEnemyLastSeenTime.Contains(Current))
		{
			return Current;
		}
	}

	// 3순위: 감지된 것 중 가장 가까운 적.
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	AActor* Nearest = nullptr;
	float NearestDistSq = FLT_MAX;
	const FVector OwnerLocation = Owner->GetActorLocation();
	for (const auto& SensedPair : SensedEnemyLastSeenTime)
	{
		AActor* SensedEnemy = SensedPair.Key.Get();
		if (!SensedEnemy)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(OwnerLocation, SensedEnemy->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = SensedEnemy;
		}
	}

	return Nearest;
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
