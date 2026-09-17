// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionAIComponent.h"
#include "CompanionCombatComponent.h"
#include "CompanionPerceptionComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "../PlayerContent/Inventory/InventoryGridComponent.h"
#include "../PlayerContent/Item/DropItem.h"
#include "CompanionLog.h"

namespace
{
	// PIE 콘솔(~)에서 "companion.FollowDistance 400"처럼 입력하면 모든 Companion의 추적 거리를
	// 즉시 덮어쓴다. 음수(기본값)면 각 인스턴스의 FollowDistance 프로퍼티를 그대로 사용한다.
	TAutoConsoleVariable<float> CVarCompanionFollowDistance(
		TEXT("companion.FollowDistance"),
		-1.0f,
		TEXT("0 이상이면 모든 Companion의 FollowDistance를 이 값으로 덮어쓴다(디버그용, PIE 중 실시간 반영)."),
		ECVF_Cheat);

	// "companion.DebugDraw 1"로 켜면 FollowDistance/AttackRange/SightRadius를 구체로 화면에 표시해
	// 위 세 값을 조정하면서 눈으로 바로 확인할 수 있다.
	TAutoConsoleVariable<int32> CVarCompanionDebugDraw(
		TEXT("companion.DebugDraw"),
		0,
		TEXT("1이면 Companion의 FollowDistance(초록)/AttackRange(빨강)/SightRadius(하늘) 반경을 구체로 표시한다."),
		ECVF_Cheat);

	// 자유 포지셔닝 추종 내부 상수.
	constexpr float FollowMovingSpeedThreshold = 50.0f;   // 이 속도 초과면 플레이어가 이동 중
	constexpr float FollowValidateInterval = 0.3f;        // 현재 자리 유효성 재검사 주기
	constexpr float FollowRepickCooldown = 0.4f;          // 자리 재선택 최소 간격(핑퐁 방지)
	constexpr float FollowArrivalTolerance = 60.0f;       // AcceptRadius + 에이전트 반경 여유
	constexpr float FollowRepathDistance = 60.0f;         // 자리가 이만큼 움직이면 이동 재요청
	constexpr float FollowStuckMinDistance = 150.0f;      // 자리에서 이보다 멀 때만 정체 감지
	constexpr float FollowRejectDuration = 4.0f;
	constexpr float FollowRejectRadius = 150.0f;
	constexpr float FollowStuckFallbackDuration = 3.0f;   // 연속 정체 시 직접 추적으로 전환하는 시간
	constexpr float FollowNoSlotFallbackDuration = 1.5f;  // 유효 자리가 없을 때 직접 추적 시간
	constexpr float FollowCatchUpDeadZone = 100.0f;
	constexpr int32 FollowCandidateAngleCount = 12;
}

using FCompanionBTNode = TBTNode<UCompanionAIComponent>;
using FCompanionSelectorNode = TBTSelectorNode<UCompanionAIComponent>;
using FCompanionSequenceNode = TBTSequenceNode<UCompanionAIComponent>;
using FCompanionConditionNode = TBTConditionNode<UCompanionAIComponent>;
using FCompanionActionNode = TBTActionNode<UCompanionAIComponent>;
using ECompanionBTResult = EBTNodeResult;

UCompanionAIComponent::UCompanionAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCompanionAIComponent::SetFollowTarget(APawn* Player)
{
	CachedPlayerPawn = Player;
}

void UCompanionAIComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		CombatComponent = Owner->FindComponentByClass<UCompanionCombatComponent>();
		PerceptionComponent = Owner->FindComponentByClass<UCompanionPerceptionComponent>();
		InventoryComponent = Owner->FindComponentByClass<UInventoryGridComponent>();
	}

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
		{
			DefaultMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
		}
	}
	FollowNoiseSeed = FMath::FRandRange(0.0f, 1000.0f);

	if (PerceptionComponent.IsValid())
	{
		PerceptionComponent->OnEnemySpotted.AddDynamic(this, &UCompanionAIComponent::HandleEnemySpotted);
		PerceptionComponent->OnEnemyLost.AddDynamic(this, &UCompanionAIComponent::HandleEnemyLost);
	}

	CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	BuildBehaviorTree();
}

AAIController* UCompanionAIComponent::GetAIController()
{
	if (!CachedAIController.IsValid())
	{
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			CachedAIController = Cast<AAIController>(OwnerCharacter->GetController());
		}
	}

	return CachedAIController.Get();
}

void UCompanionAIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CombatComponent.IsValid() && CombatComponent->IsDead())
	{
		if (AAIController* AIController = GetAIController())
		{
			AIController->StopMovement();
		}
		return;
	}

	MoveRequestTimer -= DeltaTime;

	const bool bEnemyVisible = PerceptionComponent.IsValid() && PerceptionComponent->HasEnemyTarget();
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : LastEnemyVisibleTime;
	if (bEnemyVisible)
	{
		LastEnemyVisibleTime = Now;
	}

	// bHasEverEngaged: 플레이어가 CommandEngage()로 한 번이라도 "싸워"를 명령한 뒤 다른 명령으로
	// 전투 모드를 끄기 전까지는(각 Command*() 참고), 지금 교전 중인 적을 놓쳐도 새로 보이는 적에
	// 자동으로 재교전한다 - "그만 싸우라고 할 때까지 계속 전투 모드"라는 정책.
	if ((bAutoEngageEnabled || bHasEverEngaged) && !bCombatSuppressed && bEnemyVisible)
	{
		bCombatEngaged = true;
	}
	else if (bCombatEngaged && (Now - LastEnemyVisibleTime) > CombatDisengageGraceTime)
	{
		bCombatEngaged = false;
		bHasTacticalLocation = false; // 다음 교전의 히스테리시스가 지난 전투 위치를 참조하지 않게.
	}

	AActor* CombatEnemy = GetCombatTarget();
	SetCombatRotationEnabled(CombatEnemy != nullptr);
	if (CombatEnemy)
	{
		if (AAIController* AIController = GetAIController())
		{
			AIController->SetFocalPoint(CombatEnemy->GetActorLocation());
		}

		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			const FVector ToEnemy = CombatEnemy->GetActorLocation() - OwnerCharacter->GetActorLocation();
			if (!ToEnemy.IsNearlyZero())
			{
				const FRotator CurrentRot = OwnerCharacter->GetActorRotation();
				const FRotator TargetRot(CurrentRot.Pitch, ToEnemy.Rotation().Yaw, CurrentRot.Roll);
				OwnerCharacter->SetActorRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, CombatRotationInterpSpeed));
			}
		}
	}

	bFollowTickedThisFrame = false;
	if (BehaviorTreeRoot.IsValid())
	{
		BehaviorTreeRoot->Tick(this, DeltaTime);
	}
	if (!bFollowTickedThisFrame)
	{
		RestoreDefaultMoveSpeed();
	}

	if (CVarCompanionDebugDraw.GetValueOnGameThread() != 0)
	{
		if (AActor* Owner = GetOwner())
		{
			UWorld* World = GetWorld();
			const FVector Loc = Owner->GetActorLocation();
			DrawDebugSphere(World, Loc, GetEffectiveFollowDistance(), 24, FColor::Green, false, -1.0f, 0, 1.5f);
			if (CombatComponent.IsValid())
			{
				DrawDebugSphere(World, Loc, CombatComponent->GetEffectiveAttackRange(), 24, FColor::Red, false, -1.0f, 0, 1.5f);
			}
			if (PerceptionComponent.IsValid())
			{
				DrawDebugSphere(World, Loc, PerceptionComponent->GetEffectiveSightRadius(), 24, FColor::Cyan, false, -1.0f, 0, 1.5f);
			}
		}
	}
}

FText UCompanionAIComponent::GetStatusDisplayText() const
{
	if (CombatComponent.IsValid() && CombatComponent->IsDead())
	{
		return FText::FromString(TEXT("사망"));
	}
	if (bCombatEngaged && HasEnemyTarget())
	{
		return FText::FromString(TEXT("전투 중"));
	}
	if (bHasCommandedDestination)
	{
		return FText::FromString(TEXT("이동 중"));
	}
	if (bExploring)
	{
		return FText::FromString(TEXT("탐색 중"));
	}
	if (bFollowEnabled)
	{
		return FText::FromString(TEXT("따라가는 중"));
	}
	return FText::FromString(TEXT("정지"));
}

bool UCompanionAIComponent::IsAimingRequested() const
{
	return bAimingRequested
		&& CombatComponent.IsValid()
		&& CombatComponent->GetEquippedWeapon() != nullptr;
}
void UCompanionAIComponent::ClearAimingRequest()
{
	bAimingRequested = false;
}

void UCompanionAIComponent::RequestAiming()
{
	if (bCombatSuppressed)
	{
		return;
	}

	bAimingRequested = CombatComponent.IsValid() && CombatComponent->GetEquippedWeapon() != nullptr;
}

bool UCompanionAIComponent::ShouldSprintWhileFollowing() const
{
	// 달리기 여부는 DoFollow(ApplyFollowSpeed)가 실제 이동 속도 기준으로 갱신한다.
	// 추종 속도 제어가 꺼져 있으면(추종 외 상태) 달리기 아님.
	return bFollowSpeedApplied && bFollowSprintActive;
}
float UCompanionAIComponent::GetEffectiveFollowDistance() const
{
	const float Override = CVarCompanionFollowDistance.GetValueOnGameThread();
	return Override >= 0.0f ? Override : FollowDistance;
}

void UCompanionAIComponent::CommandFollow()
{
	bAimingRequested = false;
	bFollowEnabled = true;
	bHasCommandedDestination = false;
	bCombatSuppressed = false;
	bExploring = false;

	// "따라와"는 전투 모드를 명시적으로 끈다 - CommandEngage() 이후 다른 명령이 올 때까지
	// 전투 모드를 유지하는 정책(TickComponent bHasEverEngaged 재교전)의 반대쪽 축.
	bCombatEngaged = false;
	bHasEverEngaged = false;
	bHasTacticalLocation = false;
}

void UCompanionAIComponent::CommandStop()
{
	bAimingRequested = false;
	bFollowEnabled = false;
	bHasCommandedDestination = false;
	bCombatEngaged = false;
	bCombatSuppressed = true;
	bExploring = false;
	bHasEverEngaged = false;
	bHasTacticalLocation = false;

	if (AAIController* AIController = GetAIController())
	{
		AIController->StopMovement();
	}
}

void UCompanionAIComponent::CommandMoveToLocation(const FVector& Location)
{
	bAimingRequested = false;
	FVector ProjectedLocation = Location;
	if (UWorld* World = GetWorld())
	{
		if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World))
		{
			FNavLocation NavLoc;
			if (NavSystem->ProjectPointToNavigation(Location, NavLoc))
			{
				ProjectedLocation = NavLoc.Location;
			}
		}
	}

	bHasCommandedDestination = true;
	bCommandedDestinationIsActor = false;
	CommandedLocation = ProjectedLocation;
	CommandedTargetActor = nullptr;
	bCombatSuppressed = false;
	bExploring = false;
	LastStuckTickTime = -1.0; // 다음 TickStuckDetection이 새 명령 기준으로 추적을 리셋하게 한다.
	CommandedMoveStuckRetries = 0;
	bCommandedMoveDetouring = false;

	// 이동 명령도 전투 모드를 끈다(CommandFollow와 동일한 정책).
	bCombatEngaged = false;
	bHasEverEngaged = false;
	bHasTacticalLocation = false;
}

void UCompanionAIComponent::CommandMoveToActor(AActor* TargetActor)
{
	bAimingRequested = false;
	if (!TargetActor)
	{
		return;
	}

	bHasCommandedDestination = true;
	bCommandedDestinationIsActor = true;
	CommandedTargetActor = TargetActor;
	bCombatSuppressed = false;
	bExploring = false;
	LastStuckTickTime = -1.0; // 다음 TickStuckDetection이 새 명령 기준으로 추적을 리셋하게 한다.
	CommandedMoveStuckRetries = 0;
	bCommandedMoveDetouring = false;

	// 이동 명령도 전투 모드를 끈다(CommandFollow와 동일한 정책).
	bCombatEngaged = false;
	bHasEverEngaged = false;
	bHasTacticalLocation = false;
}

void UCompanionAIComponent::CommandEngage()
{
	bAimingRequested = CombatComponent.IsValid() && CombatComponent->GetEquippedWeapon() != nullptr;
	bCombatSuppressed = false;
	bCombatEngaged = true;
	bExploring = false;
	bHasEverEngaged = true;
	LastEnemyVisibleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastEnemyVisibleTime;
}

void UCompanionAIComponent::HandleEnemySpotted(AActor* EnemyActor)
{
	LastEnemyVisibleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastEnemyVisibleTime;

	// 재장전 등으로 조준이 풀린 상태에서 적을 다시 포착하면 곧바로 재무장한다 - 사용자가 이미
	// CommandEngage()로 교전을 지시했을 때만(자동 교전 정책을 건드리지 않기 위해).
	if (bHasEverEngaged && !bCombatSuppressed)
	{
		RequestAiming();
	}
}

void UCompanionAIComponent::HandleEnemyLost(AActor* EnemyActor)
{
	// 실제 교전 해제는 TickComponent의 CombatDisengageGraceTime 유예 로직이 판단한다.
}

void UCompanionAIComponent::CommandExplore()
{
	bAimingRequested = false;
	bExploring = true;
	bHasCommandedDestination = false;
	bCombatSuppressed = false;

	// 탐색 명령도 전투 모드를 끈다(CommandFollow와 동일한 정책).
	bCombatEngaged = false;
	bHasEverEngaged = false;
	bHasTacticalLocation = false;

	CurrentExploreTargetItem = nullptr;
	ExploreScanTimer = 0.0f;
	ExploreWanderTimer = 0.0f;
	ExploreMoveFailCount = 0;
	UnreachableExploreItems.Reset();
	LastStuckTickTime = -1.0; // 다음 TickStuckDetection이 탐색 시작 기준으로 추적을 리셋하게 한다.

	if (AActor* Owner = GetOwner())
	{
		ExploreOriginLocation = Owner->GetActorLocation();
	}

	ExploreEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() + ExploreDuration : 0.0f;
}

bool UCompanionAIComponent::HasEnemyTarget() const
{
	return PerceptionComponent.IsValid() && PerceptionComponent->HasEnemyTarget();
}

AActor* UCompanionAIComponent::GetCurrentEnemyTarget() const
{
	return PerceptionComponent.IsValid() ? PerceptionComponent->GetCurrentEnemyTarget() : nullptr;
}

bool UCompanionAIComponent::IsEnemyInAttackRangeWithLineOfSight() const
{
	AActor* Enemy = GetCurrentEnemyTarget();
	AActor* Owner = GetOwner();
	if (!Enemy || !Owner || !CombatComponent.IsValid())
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector EnemyLocation = Enemy->GetActorLocation();
	if (FVector::DistSquared(OwnerLocation, EnemyLocation) > FMath::Square(CombatComponent->GetEffectiveAttackRange()))
	{
		return false;
	}

	// EvaluateTacticalPosition/HasLineOfSightFrom과 동일한 트레이스 기준(EyeProbeHeight 오프셋,
	// 적 상체 +50)을 써야 한다 - 서로 다른 높이로 판정하면 "전술 위치 선정 시엔 사격 가능"으로
	// 골라놓고 실제로 그 자리에 도착했을 때는 이 함수가 다른 기준으로 막혀버려(false) DoAttack이
	// 아예 호출되지 않는 불일치가 생긴다.
	return HasLineOfSightFrom(OwnerLocation + FVector(0.0f, 0.0f, EyeProbeHeight), Enemy);
}

void UCompanionAIComponent::SetCombatRotationEnabled(bool bEnabled)
{
	if (bCombatRotationActive == bEnabled)
	{
		return;
	}
	bCombatRotationActive = bEnabled;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
	{
		MovementComponent->bOrientRotationToMovement = !bEnabled;
	}
	// 회전은 TickComponent에서 RInterpTo로 직접 보간한다(스냅 방지) - 컨트롤러 회전을 그대로
	// 따라가면 CharacterMovementComponent가 매 틱 즉시 스냅시키므로 항상 꺼둔다. Controller의
	// ControlRotation 자체(AimPitch 등에 쓰임)는 SetFocalPoint로 계속 갱신되므로 영향 없다.
	OwnerCharacter->bUseControllerRotationYaw = false;

	if (AAIController* AIController = GetAIController())
	{
		if (!bEnabled)
		{
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}

FVector UCompanionAIComponent::ComputeCombatMoveLocation(AActor* Enemy) const
{
	const AActor* Owner = GetOwner();
	const FVector EnemyLocation = Enemy->GetActorLocation();
	const FVector OwnerLocation = Owner->GetActorLocation();

	FVector AwayFromEnemy = OwnerLocation - EnemyLocation;
	AwayFromEnemy.Z = 0.0f;
	float CurrentDistance = AwayFromEnemy.Size();
	if (CurrentDistance < KINDA_SMALL_NUMBER)
	{
		AwayFromEnemy = FVector::ForwardVector;
		CurrentDistance = 0.0f;
	}
	else
	{
		AwayFromEnemy /= CurrentDistance;
	}

	const float EffectiveAttackRange = CombatComponent.IsValid() ? CombatComponent->GetEffectiveAttackRange() : MinAttackDistance;
	const float MaxOrbitDistance = FMath::Max(MinAttackDistance, EffectiveAttackRange * 0.85f);

	// 너무 가까우면 사거리를 벌리는 쪽으로, 아니면 지금 거리를 유지한 채(사거리 안에서) 좌우로만 움직인다.
	const float DesiredDistance = CurrentDistance < MinAttackDistance
		? MaxOrbitDistance
		: FMath::Clamp(CurrentDistance, MinAttackDistance, MaxOrbitDistance);

	const FVector RightVector = FVector::CrossProduct(FVector::UpVector, AwayFromEnemy).GetSafeNormal();
	FVector DesiredLocation = EnemyLocation + AwayFromEnemy * DesiredDistance + RightVector * StrafeDirection * StrafeDistance;

	if (CachedPlayerPawn.IsValid())
	{
		const FVector PlayerLocation = CachedPlayerPawn->GetActorLocation();
		const float DistanceFromPlayer = FVector::Dist(DesiredLocation, PlayerLocation);
		if (DistanceFromPlayer > MaxCombatDistanceFromPlayer)
		{
			const FVector TowardPlayer = (PlayerLocation - DesiredLocation).GetSafeNormal();
			DesiredLocation += TowardPlayer * (DistanceFromPlayer - MaxCombatDistanceFromPlayer);
		}
	}

	return DesiredLocation;
}

bool UCompanionAIComponent::HasLineOfSightFrom(const FVector& From, AActor* Enemy) const
{
	UWorld* World = GetWorld();
	if (!World || !Enemy)
	{
		return false;
	}

	FCollisionQueryParams Params(TEXT("CompanionTacticalLOS"), false, GetOwner());

	// 적 상체(발밑이 아니라)를 겨냥해 판정한다 - 낮은 엄폐물 뒤에서 상체 사격이 가능한 상황을
	// 발밑 트레이스가 "차단"으로 오판하지 않게.
	const FVector To = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);
	return !bBlocked || Hit.GetActor() == Enemy;
}

float UCompanionAIComponent::ScoreTacticalCandidate(const FVector& Candidate, AActor* Enemy, bool bCanFire) const
{
	float Score = 0.0f;

	// 부분 엄폐 보너스: 눈높이 사선은 뚫려 있는데(bCanFire) 허리 높이는 막혀 있다
	// = 낮은 엄폐물 뒤에서 상체만 내밀고 쏠 수 있는 위치.
	if (bCanFire && !HasLineOfSightFrom(Candidate + FVector(0.0f, 0.0f, CoverProbeHeightLow), Enemy))
	{
		Score += 30.0f;
	}

	// 플레이어 이탈 감점: 허용 반경 초과분에 비례. (최종 선택 위치는 ComputeCombatMoveLocation과
	// 동일한 당겨오기 클램프도 EvaluateTacticalPosition 끝에서 한 번 더 적용된다.)
	if (const APawn* Player = CachedPlayerPawn.Get())
	{
		const float DistFromPlayer = FVector::Dist2D(Candidate, Player->GetActorLocation());
		if (DistFromPlayer > MaxCombatDistanceFromPlayer)
		{
			Score -= (DistFromPlayer - MaxCombatDistanceFromPlayer) * 0.05f;
		}
	}

	// 이동 비용 감점: 가까운 재배치를 선호하고, 상한(StrafeDistance*2)을 넘는 대시는 강하게 감점.
	if (const AActor* Owner = GetOwner())
	{
		const float TravelDist = FVector::Dist2D(Candidate, Owner->GetActorLocation());
		Score -= TravelDist * 0.01f;
		if (TravelDist > StrafeDistance * 2.0f)
		{
			Score -= 25.0f;
		}
	}

	// 히스테리시스: 직전 선택 위치 근처 후보에 보너스 - 평가 주기마다 좌우로 핑퐁하는 것을 막는다.
	if (bHasTacticalLocation && FVector::Dist2D(Candidate, CachedTacticalLocation) <= StrafeDistance * 0.5f)
	{
		Score += 15.0f;
	}

	return Score;
}

bool UCompanionAIComponent::EvaluateTacticalPosition(AActor* Enemy, FVector& OutLocation) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!Owner || !Enemy || !NavSystem)
	{
		return false;
	}

	const FVector EnemyLocation = Enemy->GetActorLocation();
	const FVector OwnerLocation = Owner->GetActorLocation();

	// 궤도 반경은 ComputeCombatMoveLocation과 동일한 공식(MinAttackDistance ~ 사거리*0.85).
	FVector AwayFromEnemy = OwnerLocation - EnemyLocation;
	AwayFromEnemy.Z = 0.0f;
	float CurrentDistance = AwayFromEnemy.Size();
	if (CurrentDistance < KINDA_SMALL_NUMBER)
	{
		AwayFromEnemy = FVector::ForwardVector;
		CurrentDistance = 0.0f;
	}
	else
	{
		AwayFromEnemy /= CurrentDistance;
	}

	const float EffectiveAttackRange = CombatComponent.IsValid() ? CombatComponent->GetEffectiveAttackRange() : MinAttackDistance;
	const float MaxOrbitDistance = FMath::Max(MinAttackDistance, EffectiveAttackRange * 0.85f);
	const float DesiredDistance = CurrentDistance < MinAttackDistance
		? MaxOrbitDistance
		: FMath::Clamp(CurrentDistance, MinAttackDistance, MaxOrbitDistance);

	// 현재 방위각 기준 좌우 대칭 각도 오프셋 - 랜덤이 아니라 결정적으로 링을 샘플한다.
	static const float CandidateAngles[] = { 0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 110.0f, -110.0f };

	const bool bDebugDraw = CVarCompanionDebugDraw.GetValueOnGameThread() != 0;

	// 사선 확보 후보 중 최고점만 고른다. 링 각도 7개가 전부 사선 막힘이면(좁은 통로/엄폐물이
	// 많은 실내 등) 일부러 사선 없는 곳으로 이동시키지 않는다 - 그러면 도착 즉시
	// IsEnemyInAttackRangeWithLineOfSight()가 매 틱 실패해 DoAttack이 아예 호출되지 않게 되고,
	// 다음 재평가도 같은 기하 구조상 다시 실패하기 쉬워 사격이 영구 중단되는 결과를 낳는다.
	// 대신 실패(false) 반환으로 호출자가 기존 ComputeCombatMoveLocation(현재 위치 기준 소폭
	// 조정이라 사선을 깨뜨릴 가능성이 낮다)로 폴백하게 한다.
	float BestScore = -FLT_MAX;
	FVector BestLocation = FVector::ZeroVector;
	bool bFoundVisible = false;

	for (const float Angle : CandidateAngles)
	{
		const FVector Dir = AwayFromEnemy.RotateAngleAxis(Angle, FVector::UpVector);
		FNavLocation NavLoc;
		if (!NavSystem->ProjectPointToNavigation(EnemyLocation + Dir * DesiredDistance, NavLoc, FVector(300.0f, 300.0f, 500.0f)))
		{
			continue;
		}

		const FVector Candidate = NavLoc.Location;
		const bool bCanFire = HasLineOfSightFrom(Candidate + FVector(0.0f, 0.0f, EyeProbeHeight), Enemy);

		if (bDebugDraw)
		{
			DrawDebugSphere(World, Candidate, 30.0f, 8, bCanFire ? FColor::Green : FColor::Red, false, StrafeInterval, 0, 2.0f);
		}

		if (!bCanFire)
		{
			continue;
		}

		const float Score = ScoreTacticalCandidate(Candidate, Enemy, bCanFire);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestLocation = Candidate;
			bFoundVisible = true;
		}
	}

	if (!bFoundVisible)
	{
		return false;
	}

	FVector Picked = BestLocation;

	// ComputeCombatMoveLocation과 동일한 플레이어 이탈 하드 클램프.
	if (CachedPlayerPawn.IsValid())
	{
		const FVector PlayerLocation = CachedPlayerPawn->GetActorLocation();
		const float DistanceFromPlayer = FVector::Dist(Picked, PlayerLocation);
		if (DistanceFromPlayer > MaxCombatDistanceFromPlayer)
		{
			const FVector TowardPlayer = (PlayerLocation - Picked).GetSafeNormal();
			Picked += TowardPlayer * (DistanceFromPlayer - MaxCombatDistanceFromPlayer);
		}
	}

	if (bDebugDraw)
	{
		DrawDebugSphere(World, Picked, 45.0f, 8, FColor::Yellow, false, StrafeInterval, 0, 3.0f);
	}

	OutLocation = Picked;
	return true;
}

EBTNodeResult UCompanionAIComponent::DoAttack(float DeltaTime)
{
	AActor* Enemy = GetCurrentEnemyTarget();
	if (!Enemy)
	{
		return EBTNodeResult::Running;
	}

	StrafeTimer -= DeltaTime;
	// 적이 마지막 평가 시점보다 크게 이동했으면 캐시된 위치가 낡았으므로 타이머와 무관하게 즉시 재평가.
	const bool bEnemyMovedFar = bHasTacticalLocation &&
		FVector::DistSquared2D(Enemy->GetActorLocation(), TacticalEvalEnemyLocation) >= FMath::Square(StrafeDistance);
	// 캐시된 위치 자체가 더 이상 사선을 확보하지 못하면(적이 그 사이 살짝만 움직였거나, 도착 지점이
	// 검증 지점과 살짝 어긋난 경우 등) 타이머/이동거리 트리거를 기다리지 않고 즉시 무효화 + 재평가한다.
	// 그러지 않으면 ScoreTacticalCandidate의 히스테리시스 보너스(+15) 때문에 다음 재평가에서도 같은
	// 막힌 자리로 다시 끌려가 영구히 사격이 멈추는 상태에 빠질 수 있다.
	const bool bCachedLocationStale = bHasTacticalLocation &&
		!HasLineOfSightFrom(CachedTacticalLocation + FVector(0.0f, 0.0f, EyeProbeHeight), Enemy);
	if (StrafeTimer <= 0.0f || bEnemyMovedFar || bCachedLocationStale)
	{
		StrafeTimer = StrafeInterval;
		StrafeDirection = FMath::RandBool() ? 1.0f : -1.0f;

		if (bCachedLocationStale)
		{
			// 히스테리시스 보너스가 이미 막힌 걸로 확인된 자리를 재선택하지 않도록 먼저 지운다.
			bHasTacticalLocation = false;
		}

		if (bTacticalPositioningEnabled)
		{
			// 평가 전에 캐시를 지우지 않는다 - ScoreTacticalCandidate의 히스테리시스 보너스가
			// 직전 선택 위치(CachedTacticalLocation)를 참조하기 때문.
			FVector Picked;
			if (EvaluateTacticalPosition(Enemy, Picked))
			{
				CachedTacticalLocation = Picked;
				bHasTacticalLocation = true;
				TacticalEvalEnemyLocation = Enemy->GetActorLocation();
			}
			else
			{
				bHasTacticalLocation = false;
			}
		}
		else
		{
			bHasTacticalLocation = false;
		}
	}

	// 스트레이프 목표 지점이 벽 안/낭떠러지 등 내비메시 밖이면 MoveToLocation이 실패해 전투 중
	// 이동이 끊긴다 - 내비메시로 투영해 가장 가까운 유효 지점으로 보낸다.
	// Utility 평가가 실패했거나 기능이 꺼져 있으면 기존 랜덤 스트레이프 계산으로 폴백.
	FVector CombatMoveLocation = bHasTacticalLocation ? CachedTacticalLocation : ComputeCombatMoveLocation(Enemy);
	if (UNavigationSystemV1* NavSystem = GetWorld() ? UNavigationSystemV1::GetCurrent(GetWorld()) : nullptr)
	{
		FNavLocation ProjectedMove;
		if (NavSystem->ProjectPointToNavigation(CombatMoveLocation, ProjectedMove, FVector(300.0f, 300.0f, 500.0f)))
		{
			CombatMoveLocation = ProjectedMove.Location;
		}
	}
	RequestMoveToLocation(CombatMoveLocation, MoveAcceptanceRadius);

	if (CombatComponent.IsValid())
	{
		CombatComponent->FireWeapon();
	}

	return EBTNodeResult::Running;
}

EBTNodeResult UCompanionAIComponent::DoMoveToEnemy(float DeltaTime)
{
	AActor* Enemy = GetCurrentEnemyTarget();
	if (!Enemy)
	{
		return EBTNodeResult::Running;
	}

	// 총기를 쓰므로 근접까지 붙지 않는다 - 사거리 안쪽까지만 다가가고, 그 안에서 사선이 확보되는 즉시
	// (매 틱 IsEnemyInAttackRangeWithLineOfSight로 체크) DoAttack으로 전환돼 그 자리에서 사격한다.
	const float AcceptRadius = CombatComponent.IsValid()
		? FMath::Max(MoveAcceptanceRadius, CombatComponent->GetEffectiveAttackRange() * 0.85f)
		: MoveAcceptanceRadius;

	RequestMoveToActor(Enemy, AcceptRadius);
	return EBTNodeResult::Running;
}

void UCompanionAIComponent::AbandonCommandedMove()
{
	bHasCommandedDestination = false;
	bCommandedMoveDetouring = false;
	CommandedMoveStuckRetries = 0;
	if (AAIController* AIController = GetAIController())
	{
		AIController->StopMovement();
	}
	OnMoveCommandBlocked.Broadcast();
}

EBTNodeResult UCompanionAIComponent::DoMoveToCommanded(float DeltaTime)
{
	AActor* Owner = GetOwner();

	// 최종 목적지 좌표 파악(액터면 현재 위치).
	FVector TargetLocation;
	AActor* TargetActor = nullptr;
	if (bCommandedDestinationIsActor)
	{
		TargetActor = CommandedTargetActor.Get();
		if (!IsValid(TargetActor))
		{
			bHasCommandedDestination = false;
			bCommandedMoveDetouring = false;
			CommandedMoveStuckRetries = 0;
			return EBTNodeResult::Failed;
		}
		TargetLocation = TargetActor->GetActorLocation();
	}
	else
	{
		TargetLocation = CommandedLocation;
	}

	// 최종 목적지 도착 판정(우회 중이어도 우연히 도착했으면 성공).
	if (Owner && FVector::DistSquared(Owner->GetActorLocation(), TargetLocation) <= FMath::Square(MoveAcceptanceRadius))
	{
		bHasCommandedDestination = false;
		bCommandedMoveDetouring = false;
		CommandedMoveStuckRetries = 0;
		return EBTNodeResult::Succeeded;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// ── 우회 지점으로 이동 중 ──
	if (bCommandedMoveDetouring)
	{
		const bool bReachedDetour = Owner &&
			FVector::DistSquared(Owner->GetActorLocation(), CommandedDetourLocation) <= FMath::Square(MoveAcceptanceRadius);
		const bool bDetourTimedOut = Now >= CommandedDetourEndTime;
		const bool bDetourStuck = TickStuckDetection(DeltaTime);

		if (bReachedDetour || bDetourTimedOut || bDetourStuck)
		{
			UE_LOG(LogCompanionAI, Log, TEXT("[AI] 우회 지점 처리 완료(도달=%d 시간초과=%d 정체=%d) - 본 목적지 재접근"),
				bReachedDetour, bDetourTimedOut, bDetourStuck);
			bCommandedMoveDetouring = false;
			ResetStuckDetection();
		}
		else
		{
			RequestMoveToLocation(CommandedDetourLocation, MoveAcceptanceRadius);
		}
		return EBTNodeResult::Running;
	}

	// ── 본 목적지로 이동 ──
	if (TargetActor)
	{
		RequestMoveToActor(TargetActor, MoveAcceptanceRadius);
	}
	else
	{
		RequestMoveToLocation(TargetLocation, MoveAcceptanceRadius);
	}

	// 도착도 실패도 아니면서 제자리에 멈춰 있으면(플레이어가 길을 막고 서 있는 등) 우회를 시도한다.
	if (TickStuckDetection(DeltaTime))
	{
		++CommandedMoveStuckRetries;

		if (CommandedMoveStuckRetries > MaxStuckRetries)
		{
			UE_LOG(LogCompanionAI, Warning, TEXT("[AI] 명령 지점으로 이동 불가(%d회 우회 재시도 실패) - 명령 포기"), MaxStuckRetries);
			AbandonCommandedMove();
			return EBTNodeResult::Failed;
		}

		FVector Detour;
		if (ComputeDetourLocation(TargetLocation, Detour))
		{
			UE_LOG(LogCompanionAI, Log, TEXT("[AI] 이동 정체 감지 - 우회 시도 %d/%d"), CommandedMoveStuckRetries, MaxStuckRetries);
			bCommandedMoveDetouring = true;
			CommandedDetourLocation = Detour;
			CommandedDetourEndTime = Now + DetourTimeout;
			ResetStuckDetection();
		}
		else
		{
			UE_LOG(LogCompanionAI, Warning, TEXT("[AI] 우회 지점을 찾지 못함 - 명령 포기"));
			AbandonCommandedMove();
			return EBTNodeResult::Failed;
		}
	}

	return EBTNodeResult::Running;
}

EBTNodeResult UCompanionAIComponent::DoExplore(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return EBTNodeResult::Running;
	}

	if (GetWorld()->GetTimeSeconds() >= ExploreEndTime)
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[AI] 탐색 종료: 시간 만료"));
		bExploring = false;
		return EBTNodeResult::Failed;
	}

	// 플레이어와 너무 멀어지면 길을 잃지 않게 탐색을 접고 Follow로 복귀한다.
	if (!CachedPlayerPawn.IsValid())
	{
		CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	}
	if (APawn* Player = CachedPlayerPawn.Get())
	{
		if (FVector::DistSquared(Owner->GetActorLocation(), Player->GetActorLocation()) > FMath::Square(MaxExploreDistanceFromPlayer))
		{
			UE_LOG(LogCompanionAI, Log, TEXT("[AI] 탐색 종료: 플레이어와 거리 초과(MaxExploreDistanceFromPlayer=%.0f)"), MaxExploreDistanceFromPlayer);
			bExploring = false;
			return EBTNodeResult::Failed;
		}
	}

	// 습득 대상이 있으면 그쪽으로 이동하고, 도착하면 줍는다.
	AActor* TargetItem = CurrentExploreTargetItem.Get();
	if (IsValid(TargetItem))
	{
		// 순수 거리만으로 판정하면 MoveToActor가 실제로 멈추는 지점(엔진이 AcceptanceRadius에
		// 에이전트 반경을 더해 도착 판정하는 지점)보다 항상 엄격해서 절대 만족되지 않는다.
		// AIController가 쓰는 것과 동일한 수평 거리 공식을 재사용하되, 높이(Z)는 무시한다 - 진열대처럼
		// 공중에 띄워둔 아이템은 캐릭터 캡슐 half-height를 넘는 높이 차 때문에 절대 도착 판정이 안 나서다.
		AAIController* AIController = GetAIController();
		UPathFollowingComponent* PathFollowing = AIController ? AIController->GetPathFollowingComponent() : nullptr;
		FVector ReachTestPoint = TargetItem->GetActorLocation();
		ReachTestPoint.Z = Owner->GetActorLocation().Z;
		const bool bReachedItem = PathFollowing
			&& PathFollowing->HasReached(ReachTestPoint, EPathFollowingReachMode::OverlapAgent, PickupRadius);

		if (bReachedItem)
		{
			TryPickupItem(Cast<ADropItem>(TargetItem));
			CurrentExploreTargetItem = nullptr;
		}
		else
		{
			const float DistToItem = FVector::Dist(Owner->GetActorLocation(), TargetItem->GetActorLocation());
			UE_LOG(LogCompanionAI, Log, TEXT("[AI] 아이템으로 이동 중: %s (거리 %.0f)"), *TargetItem->GetName(), DistToItem);

			if (RequestMoveToActor(TargetItem, PickupRadius))
			{
				++ExploreMoveFailCount;
			}
			else
			{
				ExploreMoveFailCount = 0;
			}

			// 경로 요청이 반복 실패했거나, 요청은 받아들여졌지만 실제로는 제자리라면(플레이어가
			// 막고 서 있는 경우 등) 이 아이템은 도달 불가로 보고 포기한 뒤 다른 대상을 찾는다.
			if (ExploreMoveFailCount >= 3 || TickStuckDetection(DeltaTime))
			{
				UE_LOG(LogCompanionAI, Warning, TEXT("[AI] %s 도달 불가로 판단해 포기(요청 실패 %d회 또는 정체 감지)"),
					*TargetItem->GetName(), ExploreMoveFailCount);
				UnreachableExploreItems.Add(TargetItem);
				CurrentExploreTargetItem = nullptr;
				ExploreMoveFailCount = 0;
				ResetStuckDetection();
			}
		}
		return EBTNodeResult::Running;
	}

	// 주기적으로 근처 아이템을 스캔한다.
	ExploreScanTimer -= DeltaTime;
	if (ExploreScanTimer <= 0.0f)
	{
		ExploreScanTimer = ExploreScanInterval;
		if (AActor* Found = FindNearestDropItem(ExploreSearchRadius))
		{
			UE_LOG(LogCompanionAI, Log, TEXT("[AI] 탐색 대상 아이템 발견: %s"), *Found->GetName());
			CurrentExploreTargetItem = Found;
			ExploreMoveFailCount = 0;
			return EBTNodeResult::Running;
		}
	}

	// 근처에 아이템이 없으면 시작 지점 주변을 배회한다.
	ExploreWanderTimer -= DeltaTime;
	if (ExploreWanderTimer <= 0.0f)
	{
		ExploreWanderTimer = ExploreWanderInterval;

		if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
		{
			FNavLocation RandomPoint;
			if (NavSystem->GetRandomReachablePointInRadius(ExploreOriginLocation, ExploreRadius, RandomPoint))
			{
				RequestMoveToLocation(RandomPoint.Location, MoveAcceptanceRadius);
			}
		}
	}

	return EBTNodeResult::Running;
}

AActor* UCompanionAIComponent::FindNearestDropItem(float SearchRadius) const
{
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(SearchRadius);

	for (TActorIterator<ADropItem> It(GetWorld()); It; ++It)
	{
		ADropItem* Item = *It;
		if (!IsValid(Item) || !Item->ItemData)
		{
			continue;
		}

		if (UnreachableExploreItems.ContainsByPredicate([Item](const TWeakObjectPtr<AActor>& Unreachable)
		{
			return Unreachable.Get() == Item;
		}))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Owner->GetActorLocation(), Item->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Item;
		}
	}

	return Best;
}

void UCompanionAIComponent::TryPickupItem(ADropItem* Item)
{
	if (!Item || !Item->ItemData || !InventoryComponent.IsValid())
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[AI] 습득 실패: Item=%s ItemData=%s InventoryComponent=%s"),
			Item ? *Item->GetName() : TEXT("null"),
			(Item && Item->ItemData) ? TEXT("valid") : TEXT("null"),
			InventoryComponent.IsValid() ? TEXT("valid") : TEXT("invalid"));
		return;
	}

	// 플레이어 픽업(ADropItem::OnInteract_Implementation)과 동일한 진입점을 탄다 - 서버에
	// 먼저 요청해서 허가받은 뒤에만 실제로 담고 삭제한다(중복 습득 방지). 자세한 흐름은
	// ADropItem::RequestPickup 주석 참고.
	UE_LOG(LogCompanionAI, Log, TEXT("[AI] 아이템 습득 요청: %s"), *Item->ItemData->DisplayName.ToString());
	Item->RequestPickup(InventoryComponent.Get());
}

EBTNodeResult UCompanionAIComponent::DoFollow(float DeltaTime)
{
	if (!bFollowEnabled)
	{
		return EBTNodeResult::Failed;
	}

	if (!CachedPlayerPawn.IsValid())
	{
		CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	APawn* Player = CachedPlayerPawn.Get();
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Player || !Owner || !World)
	{
		return EBTNodeResult::Running;
	}

	bFollowTickedThisFrame = true;
	const float Now = World->GetTimeSeconds();

	// 다른 상태(전투/명령/탐색)에 있다가 막 돌아왔으면 자리 선택을 처음부터 다시 한다.
	if (LastFollowTickTime < 0.0 || (Now - LastFollowTickTime) > 0.5)
	{
		bHasFollowSlot = false;
		bFollowMoveIssued = false;
		FollowStuckCount = 0;
		FollowFallbackEndTime = 0.0f;
		PlayerStillElapsed = 0.0f;
		bIdleLookActive = false;
		FollowHeadingYaw = static_cast<float>(Player->GetActorRotation().Yaw);
	}
	LastFollowTickTime = Now;

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector PlayerLocation = Player->GetActorLocation();
	FVector PlayerVelocity = Player->GetVelocity();
	PlayerVelocity.Z = 0.0f;
	const float PlayerSpeed = static_cast<float>(PlayerVelocity.Size());
	const bool bPlayerMoving = PlayerSpeed > FollowMovingSpeedThreshold;

	PlayerStillElapsed = bPlayerMoving ? 0.0f : PlayerStillElapsed + DeltaTime;
	const bool bIdleMode = PlayerStillElapsed >= IdleSettleDelay;

	// 플레이어 진행 방향(자리 각도의 기준축). 멈춰 있으면 마지막 방향을 유지한다.
	if (bPlayerMoving)
	{
		const FRotator CurrentHeading(0.0f, FollowHeadingYaw, 0.0f);
		const FRotator TargetHeading(0.0f, PlayerVelocity.Rotation().Yaw, 0.0f);
		FollowHeadingYaw = static_cast<float>(FMath::RInterpTo(CurrentHeading, TargetHeading, DeltaTime, FollowHeadingInterpSpeed).Yaw);
	}

	// 도넛 중심 = 플레이어 예측 위치(내비메시 투영). 실패하면 현재 위치로.
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);
	const FVector ProjectExtent(100.0f, 100.0f, 300.0f);
	FNavLocation CenterNav;
	const bool bHasCenter = NavSystem
		&& (NavSystem->ProjectPointToNavigation(PlayerLocation + PlayerVelocity * FollowPredictionTime, CenterNav, ProjectExtent)
			|| NavSystem->ProjectPointToNavigation(PlayerLocation, CenterNav, ProjectExtent));

	// 폴백: 자리를 잡을 수 없거나 연속 정체 직후엔 예전 방식(플레이어에게 직접 접근).
	if (!bHasCenter || Now < FollowFallbackEndTime)
	{
		const float EffectiveFollowDistance = GetEffectiveFollowDistance();
		const float DistToPlayer = static_cast<float>(FVector::Dist2D(OwnerLocation, PlayerLocation));
		if (DistToPlayer > EffectiveFollowDistance)
		{
			RequestMoveToActor(Player, EffectiveFollowDistance);
		}
		bFollowMoveIssued = false;
		ApplyFollowSpeed(DeltaTime, FMath::Max(0.0f, DistToPlayer - EffectiveFollowDistance), PlayerSpeed, bPlayerMoving);
		return EBTNodeResult::Running;
	}

	// ── 자리 유지/재선택 판단 ──
	bool bNeedPick = !bHasFollowSlot || Now >= FollowSlotExpireTime || bIdleMode != bFollowSlotIsIdle;

	FollowValidateTimer -= DeltaTime;
	if (!bNeedPick && FollowValidateTimer <= 0.0f)
	{
		FollowValidateTimer = FollowValidateInterval;

		// 자리는 플레이어 기준 (각도, 반경)이라 플레이어가 움직이면 월드 좌표도 같이 옮겨진다.
		FVector Spot;
		if (ComputeFollowSpot(CenterNav.Location, FollowSlotAngle, FollowSlotRadius, bPlayerMoving, Spot))
		{
			FollowSpotLocation = Spot;
		}
		else
		{
			bNeedPick = true; // 벽에 막혔거나 플레이어 진행 경로에 걸림 -> 비켜선다.
		}

		// 플레이어가 이쪽으로 다가오며 너무 가까워지면 비켜선다.
		const FVector PlayerToOwner = OwnerLocation - PlayerLocation;
		if (bPlayerMoving
			&& PlayerToOwner.SizeSquared2D() < FMath::Square(MinFollowDistance)
			&& FVector::DotProduct(PlayerVelocity, PlayerToOwner) > 0.0f)
		{
			bNeedPick = true;
		}
	}

	if (bNeedPick && (Now - FollowLastPickTime) >= FollowRepickCooldown)
	{
		FollowLastPickTime = Now;
		FollowValidateTimer = FollowValidateInterval;
		if (!PickFollowSlot(CenterNav.Location, bPlayerMoving, bIdleMode))
		{
			UE_LOG(LogCompanionAI, Verbose, TEXT("[AI] 추종 자리 후보 없음 - 잠시 직접 추적"));
			bHasFollowSlot = false;
			FollowFallbackEndTime = Now + FollowNoSlotFallbackDuration;
			return EBTNodeResult::Running;
		}
	}

	if (!bHasFollowSlot)
	{
		return EBTNodeResult::Running;
	}

	// ── 자리로 이동 ──
	const float DistToSpot = static_cast<float>(FVector::Dist2D(OwnerLocation, FollowSpotLocation));
	const bool bArrived = DistToSpot <= FollowSpotAcceptRadius + FollowArrivalTolerance;

	AAIController* AIController = GetAIController();
	const bool bPathIdle = AIController && AIController->GetMoveStatus() == EPathFollowingStatus::Idle;
	// 플레이어가 이동 중이면 자리도 계속 움직이므로 도착 여부와 무관하게 따라간다(멈칫거림 방지).
	if ((!bArrived || bPlayerMoving) && MoveRequestTimer <= 0.0f
		&& (!bFollowMoveIssued
			|| FVector::Dist2D(FollowSpotLocation, LastRequestedFollowSpot) > FollowRepathDistance
			|| bPathIdle))
	{
		RequestMoveToLocation(FollowSpotLocation, FollowSpotAcceptRadius);
		LastRequestedFollowSpot = FollowSpotLocation;
		bFollowMoveIssued = true;
	}

	// 좁은 곳에서 막히면: 그 자리를 잠시 포기하고 다른 자리를 고른다. 두 번 연속이면 직접 추적으로.
	if (!bArrived && DistToSpot > FollowStuckMinDistance)
	{
		if (TickStuckDetection(DeltaTime))
		{
			ResetStuckDetection();
			FollowRejectedSpots.Emplace(FollowSpotLocation, Now + FollowRejectDuration);
			bHasFollowSlot = false;
			bFollowMoveIssued = false;
			FollowLastPickTime = -FLT_MAX;

			if (++FollowStuckCount >= 2)
			{
				UE_LOG(LogCompanionAI, Log, TEXT("[AI] 추종 중 연속 정체 - %.1f초간 직접 추적"), FollowStuckFallbackDuration);
				FollowStuckCount = 0;
				FollowFallbackEndTime = Now + FollowStuckFallbackDuration;
			}
			else
			{
				UE_LOG(LogCompanionAI, Log, TEXT("[AI] 추종 중 정체 - 다른 자리 선택"));
			}
			return EBTNodeResult::Running;
		}
	}
	else if (bArrived)
	{
		FollowStuckCount = 0;
	}

	ApplyFollowSpeed(DeltaTime, DistToSpot, PlayerSpeed, bPlayerMoving);

	if (bIdleMode && bArrived)
	{
		TickIdleLook(DeltaTime, Player);
	}
	else
	{
		bIdleLookActive = false;
	}

	if (CVarCompanionDebugDraw.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(World, FollowSpotLocation, 35.0f, 8, bIdleMode ? FColor::Blue : FColor::Yellow, false, -1.0f, 0, 2.0f);
		DrawDebugLine(World, OwnerLocation, FollowSpotLocation, FColor::Yellow, false, -1.0f, 0, 1.0f);
	}

	return EBTNodeResult::Running;
}

bool UCompanionAIComponent::ComputeFollowSpot(const FVector& Center, float Angle, float Radius, bool bPlayerMoving, FVector& OutSpot) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!NavSystem)
	{
		return false;
	}

	const FVector Direction = FRotator(0.0f, FollowHeadingYaw + Angle, 0.0f).Vector();
	FNavLocation NavLoc;
	if (!NavSystem->ProjectPointToNavigation(Center + Direction * Radius, NavLoc, FVector(80.0f, 80.0f, 250.0f)))
	{
		return false;
	}
	const FVector Spot = NavLoc.Location;

	// 투영이 플레이어 쪽으로 크게 끌려왔으면 버린다.
	if (FVector::Dist2D(Spot, Center) < FMath::Min(FollowRingMinRadius, FollowRingMaxRadius) * 0.6f)
	{
		return false;
	}

	// 플레이어 위치에서 그 자리까지 내비메시 위로 곧게 이어져야 한다. 좁은 통로에서는 옆/앞 자리가
	// 여기서 걸러져 자연스럽게 뒤쪽 자리만 남는다(벽 너머/다른 층 자리도 제외).
	FVector HitLocation;
	if (UNavigationSystemV1::NavigationRaycast(World, Center, Spot, HitLocation))
	{
		return false;
	}

	// 이동 중인 플레이어의 진행 경로 위에는 서지 않는다.
	if (bPlayerMoving)
	{
		const FVector Local = FRotator(0.0f, FollowHeadingYaw, 0.0f).UnrotateVector(Spot - Center);
		if (Local.X > -50.0f && Local.X < FollowPathBlockLength && FMath::Abs(Local.Y) < FollowPathBlockHalfWidth)
		{
			return false;
		}
	}

	const float Now = World->GetTimeSeconds();
	for (const TPair<FVector, float>& Rejected : FollowRejectedSpots)
	{
		if (Rejected.Value > Now && FVector::DistSquared2D(Spot, Rejected.Key) < FMath::Square(FollowRejectRadius))
		{
			return false;
		}
	}

	OutSpot = Spot;
	return true;
}

bool UCompanionAIComponent::PickFollowSlot(const FVector& Center, bool bPlayerMoving, bool bIdleMode)
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	FollowRejectedSpots.RemoveAll([Now](const TPair<FVector, float>& Rejected)
	{
		return Rejected.Value <= Now;
	});

	const float MinRadius = FMath::Min(FollowRingMinRadius, FollowRingMaxRadius);
	const float MaxRadius = FMath::Max(FollowRingMinRadius, FollowRingMaxRadius);
	const float MidRadius = (MinRadius + MaxRadius) * 0.5f;
	const float RadiusRange = FMath::Max(MaxRadius - MinRadius, 1.0f);
	const float CandidateRadii[] = { FMath::Lerp(MinRadius, MaxRadius, 0.25f), FMath::Lerp(MinRadius, MaxRadius, 0.75f) };

	// 시간에 따라 천천히 흔들리는 선호 방향(진행 방향 기준, 0=앞). 같은 길을 걸어도 매번 위치가 달라진다.
	const float Noise = FMath::Clamp(FMath::PerlinNoise1D(Now * FollowPreferenceDriftSpeed + FollowNoiseSeed) * 1.8f, -1.0f, 1.0f);
	const float PreferredAngle = Noise * 180.0f;

	const FVector OwnerLocation = Owner->GetActorLocation();
	const bool bDebugDraw = CVarCompanionDebugDraw.GetValueOnGameThread() != 0;

	FCollisionQueryParams WallParams(TEXT("CompanionIdleWall"), false, Owner);
	if (const APawn* Player = CachedPlayerPawn.Get())
	{
		WallParams.AddIgnoredActor(Player);
	}

	float BestScore = -FLT_MAX;
	float BestAngle = 0.0f;
	float BestRadius = 0.0f;
	FVector BestSpot = FVector::ZeroVector;
	bool bFound = false;

	for (int32 Index = 0; Index < FollowCandidateAngleCount; ++Index)
	{
		const float Angle = -180.0f + Index * (360.0f / FollowCandidateAngleCount);
		for (const float Radius : CandidateRadii)
		{
			FVector Spot;
			const bool bValid = ComputeFollowSpot(Center, Angle, Radius, bPlayerMoving, Spot);
			if (bDebugDraw)
			{
				const FVector DrawLocation = bValid ? Spot : Center + FRotator(0.0f, FollowHeadingYaw + Angle, 0.0f).Vector() * Radius;
				DrawDebugSphere(World, DrawLocation, 15.0f, 6, bValid ? FColor::Green : FColor::Red, false, 0.5f, 0, 1.0f);
			}
			if (!bValid)
			{
				continue;
			}

			float Score = 0.0f;
			// 지금 위치에서 가까운 자리 선호 - 괜히 플레이어를 가로질러 반대편으로 가지 않게.
			Score -= static_cast<float>(FVector::Dist2D(Spot, OwnerLocation)) * 0.003f;
			// 선호 방향과 가까울수록 가점. 대기 중엔 약하게(벽 등지기가 우선).
			Score -= FMath::Abs(FMath::FindDeltaAngleDegrees(Angle, PreferredAngle)) / 180.0f * (bIdleMode ? 0.5f : 2.0f);
			// 도넛 중간 거리 선호.
			Score -= FMath::Abs(Radius - MidRadius) / RadiusRange * 0.5f;

			if (bIdleMode)
			{
				// 자리 바깥쪽(플레이어 반대편)에 벽이 있으면 가점 - 벽을 등지고 선다.
				const FVector Outward = (Spot - Center).GetSafeNormal2D();
				const FVector TraceStart = Spot + FVector(0.0f, 0.0f, 50.0f);
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceStart + Outward * IdleWallProbeDistance, ECC_Visibility, WallParams))
				{
					Score += 1.5f;
				}
			}

			Score += FMath::FRandRange(0.0f, 0.3f);

			if (Score > BestScore)
			{
				BestScore = Score;
				BestAngle = Angle;
				BestRadius = Radius;
				BestSpot = Spot;
				bFound = true;
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	FollowSlotAngle = BestAngle;
	FollowSlotRadius = BestRadius;
	FollowSpotLocation = BestSpot;
	bHasFollowSlot = true;
	bFollowSlotIsIdle = bIdleMode;
	bFollowMoveIssued = false;
	FollowSlotExpireTime = Now + (bIdleMode
		? FMath::FRandRange(IdleSlotHoldTimeMin, IdleSlotHoldTimeMax)
		: FMath::FRandRange(FollowSlotHoldTimeMin, FollowSlotHoldTimeMax));

	UE_LOG(LogCompanionAI, Verbose, TEXT("[AI] 추종 자리 선택: 각도 %.0f 반경 %.0f (선호 %.0f, 대기=%d)"),
		BestAngle, BestRadius, PreferredAngle, bIdleMode);
	return true;
}

void UCompanionAIComponent::ApplyFollowSpeed(float DeltaTime, float DistToSpot, float PlayerSpeed, bool bPlayerMoving)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (!MovementComponent)
	{
		return;
	}

	if (!bFollowSpeedApplied)
	{
		bFollowSpeedApplied = true;
		CurrentFollowSpeed = FMath::Max(static_cast<float>(MovementComponent->Velocity.Size2D()), FollowWalkSpeed * 0.4f);
	}

	float TargetSpeed;
	if (bPlayerMoving)
	{
		// 플레이어 속도에 맞추고, 자리에서 뒤처진 만큼 더 빠르게.
		TargetSpeed = PlayerSpeed + FMath::Max(0.0f, DistToSpot - FollowCatchUpDeadZone) * FollowCatchUpGain;
	}
	else
	{
		// 플레이어가 서 있으면 걷기 속도로 가다가 도착 직전에 감속.
		const float ArrivalScale = FMath::Lerp(0.4f, 1.0f,
			FMath::Clamp(DistToSpot / FMath::Max(FollowArrivalSlowRadius, 1.0f), 0.0f, 1.0f));
		TargetSpeed = FollowWalkSpeed * ArrivalScale
			+ FMath::Max(0.0f, DistToSpot - FollowRingMaxRadius) * FollowCatchUpGain;
	}
	TargetSpeed = FMath::Clamp(TargetSpeed, FollowWalkSpeed * 0.4f, FollowSprintSpeed);

	CurrentFollowSpeed = FMath::FInterpTo(CurrentFollowSpeed, TargetSpeed, DeltaTime, FollowSpeedInterpSpeed);
	MovementComponent->MaxWalkSpeed = CurrentFollowSpeed;

	if (CurrentFollowSpeed >= FollowSprintAnimSpeed)
	{
		bFollowSprintActive = true;
	}
	else if (CurrentFollowSpeed <= FollowSprintAnimSpeed * 0.85f)
	{
		bFollowSprintActive = false;
	}
}

void UCompanionAIComponent::RestoreDefaultMoveSpeed()
{
	if (!bFollowSpeedApplied)
	{
		return;
	}
	bFollowSpeedApplied = false;
	bFollowSprintActive = false;

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
		{
			MovementComponent->MaxWalkSpeed = DefaultMaxWalkSpeed;
		}
	}
}

void UCompanionAIComponent::TickIdleLook(float DeltaTime, const APawn* Player)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !Player)
	{
		return;
	}

	IdleLookTimer -= DeltaTime;
	if (!bIdleLookActive || IdleLookTimer <= 0.0f)
	{
		bIdleLookActive = true;
		IdleLookTimer = FMath::FRandRange(IdleLookIntervalMin, IdleLookIntervalMax);

		const FVector ToPlayer = Player->GetActorLocation() - OwnerCharacter->GetActorLocation();
		if (FMath::FRand() < IdleLookAtPlayerChance)
		{
			IdleLookYaw = static_cast<float>(ToPlayer.Rotation().Yaw);
		}
		else
		{
			// 플레이어 반대편(바깥쪽) 기준 좌우로 둘러본다 - 경계하는 느낌.
			IdleLookYaw = static_cast<float>((-ToPlayer).Rotation().Yaw) + FMath::FRandRange(-100.0f, 100.0f);
		}
	}

	// 서 있을 때는 bOrientRotationToMovement가 회전을 덮어쓰지 않으므로 직접 보간한다.
	const FRotator CurrentRotation = OwnerCharacter->GetActorRotation();
	const FRotator TargetRotation(CurrentRotation.Pitch, IdleLookYaw, CurrentRotation.Roll);
	OwnerCharacter->SetActorRotation(FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, IdleLookInterpSpeed));
}

EBTNodeResult UCompanionAIComponent::DoIdle(float DeltaTime)
{
	if (AAIController* AIController = GetAIController())
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Running;
}

bool UCompanionAIComponent::RequestMoveToActor(AActor* Target, float AcceptRadius)
{
	AAIController* AIController = GetAIController();
	if (!Target || !AIController)
	{
		return false;
	}

	if (MoveRequestTimer > 0.0f)
	{
		return false;
	}

	MoveRequestTimer = MoveRequestInterval;
	const EPathFollowingRequestResult::Type Result =
		AIController->MoveToActor(Target, AcceptRadius, true, true, true, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[AI] %s(으)로 이동 요청 실패(경로 탐색 실패)"), *Target->GetName());
		return true;
	}

	return false;
}

void UCompanionAIComponent::RequestMoveToLocation(const FVector& Location, float AcceptRadius)
{
	AAIController* AIController = GetAIController();
	if (!AIController)
	{
		return;
	}

	if (MoveRequestTimer > 0.0f)
	{
		return;
	}

	MoveRequestTimer = MoveRequestInterval;
	const EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(Location, AcceptRadius);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[AI] %s(으)로 이동 요청 실패(경로 탐색 실패)"), *Location.ToString());
	}
}

void UCompanionAIComponent::ResetStuckDetection()
{
	StuckElapsed = 0.0f;
	StuckAnchorLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

bool UCompanionAIComponent::TickStuckDetection(float DeltaTime)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// 이 함수가 매 틱 연속 호출되지 않았다면(전투/추적 등 다른 상태에 있었다면) 추적을 리셋한다.
	if (LastStuckTickTime < 0.0 || (Now - LastStuckTickTime) > 0.5)
	{
		ResetStuckDetection();
	}
	LastStuckTickTime = Now;

	if (FVector::DistSquared(Owner->GetActorLocation(), StuckAnchorLocation) >= FMath::Square(StuckDistanceThreshold))
	{
		// 기준점에서 충분히 멀어졌다 = 전진 중. 기준점을 당겨오고 타이머를 리셋한다.
		StuckAnchorLocation = Owner->GetActorLocation();
		StuckElapsed = 0.0f;
		return false;
	}

	StuckElapsed += DeltaTime;
	return StuckElapsed >= StuckTimeThreshold;
}

bool UCompanionAIComponent::ComputeDetourLocation(const FVector& TowardTarget, FVector& OutDetour) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!Owner || !NavSystem)
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	FVector ToTarget = TowardTarget - OwnerLocation;
	ToTarget.Z = 0.0f;
	if (!ToTarget.Normalize())
	{
		ToTarget = Owner->GetActorForwardVector();
	}

	// 재시도마다 좌/우를 번갈아 시도한다(1회=우, 2회=좌, 3회=우 ...).
	const float SideSign = (CommandedMoveStuckRetries % 2 == 1) ? 1.0f : -1.0f;
	const FVector SideDir = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal();

	// 옆쪽으로 벌리고 살짝 앞쪽으로 당긴 지점을 우선 시도.
	const FVector Candidate = OwnerLocation + SideDir * SideSign * DetourDistance + ToTarget * (DetourDistance * 0.5f);

	FNavLocation NavLoc;
	if (NavSystem->ProjectPointToNavigation(Candidate, NavLoc, FVector(DetourDistance, DetourDistance, 500.0f)))
	{
		OutDetour = NavLoc.Location;
		return true;
	}

	// 폴백: 주변 아무 도달 가능 지점.
	if (NavSystem->GetRandomReachablePointInRadius(OwnerLocation, DetourDistance * 1.5f, NavLoc))
	{
		OutDetour = NavLoc.Location;
		return true;
	}

	return false;
}

void UCompanionAIComponent::BuildBehaviorTree()
{
	TSharedPtr<FCompanionSelectorNode> Root = MakeShared<FCompanionSelectorNode>();

	// 1. 전투: bCombatEngaged && 적 존재 -> [사거리+사선 확인되면 공격, 아니면 접근]
	TSharedPtr<FCompanionSequenceNode> CombatSequence = MakeShared<FCompanionSequenceNode>();
	CombatSequence->Children.Add(MakeShared<FCompanionConditionNode>([](UCompanionAIComponent* AI)
	{
		const bool bInCombat = IsValid(AI) && AI->bCombatEngaged && AI->HasEnemyTarget();
		if (bInCombat && AI->bExploring)
		{
			UE_LOG(LogCompanionAI, Verbose, TEXT("[AI] 탐색 중 전투 우선순위에 밀림(적 감지됨)"));
		}
		return bInCombat;
	}));

	TSharedPtr<FCompanionSelectorNode> CombatInner = MakeShared<FCompanionSelectorNode>();

	TSharedPtr<FCompanionSequenceNode> AttackSequence = MakeShared<FCompanionSequenceNode>();
	AttackSequence->Children.Add(MakeShared<FCompanionConditionNode>([](UCompanionAIComponent* AI)
	{
		// CanAttack()(쿨다운) 여부는 DoAttack 안에서 발사만 걸러낸다 - 쿨다운 중에도 사거리+사선만
		// 확보돼 있으면 계속 이 노드에 머물러 스트레이프 움직임이 끊기지 않게 한다.
		return IsValid(AI) && AI->IsEnemyInAttackRangeWithLineOfSight();
	}));
	AttackSequence->Children.Add(MakeShared<FCompanionActionNode>([](UCompanionAIComponent* AI, float DeltaTime)
	{
		return AI->DoAttack(DeltaTime);
	}));

	CombatInner->Children.Add(AttackSequence);
	CombatInner->Children.Add(MakeShared<FCompanionActionNode>([](UCompanionAIComponent* AI, float DeltaTime)
	{
		return AI->DoMoveToEnemy(DeltaTime);
	}));

	CombatSequence->Children.Add(CombatInner);
	Root->Children.Add(CombatSequence);

	// 2. 명시적 이동 명령
	TSharedPtr<FCompanionSequenceNode> MoveSequence = MakeShared<FCompanionSequenceNode>();
	MoveSequence->Children.Add(MakeShared<FCompanionConditionNode>([](UCompanionAIComponent* AI)
	{
		return IsValid(AI) && AI->bHasCommandedDestination;
	}));
	MoveSequence->Children.Add(MakeShared<FCompanionActionNode>([](UCompanionAIComponent* AI, float DeltaTime)
	{
		return AI->DoMoveToCommanded(DeltaTime);
	}));
	Root->Children.Add(MoveSequence);

	// 3. 탐색(주변 배회 + 아이템 습득)
	TSharedPtr<FCompanionSequenceNode> ExploreSequence = MakeShared<FCompanionSequenceNode>();
	ExploreSequence->Children.Add(MakeShared<FCompanionConditionNode>([](UCompanionAIComponent* AI)
	{
		return IsValid(AI) && AI->bExploring;
	}));
	ExploreSequence->Children.Add(MakeShared<FCompanionActionNode>([](UCompanionAIComponent* AI, float DeltaTime)
	{
		return AI->DoExplore(DeltaTime);
	}));
	Root->Children.Add(ExploreSequence);

	// 4. 플레이어 추적
	Root->Children.Add(MakeShared<FCompanionActionNode>([](UCompanionAIComponent* AI, float DeltaTime)
	{
		return AI->DoFollow(DeltaTime);
	}));

	// 5. Idle
	Root->Children.Add(MakeShared<FCompanionActionNode>([](UCompanionAIComponent* AI, float DeltaTime)
	{
		return AI->DoIdle(DeltaTime);
	}));

	BehaviorTreeRoot = Root;
}


