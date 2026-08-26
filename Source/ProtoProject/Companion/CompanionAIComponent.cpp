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
#include "../Network/ProtoNetClientSubsystem.h"
#include "Engine/GameInstance.h"
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
	if (bAutoEngageEnabled && !bCombatSuppressed && bEnemyVisible)
	{
		bCombatEngaged = true;
	}
	else if (!bEnemyVisible)
	{
		bCombatEngaged = false;
	}

	AActor* CombatEnemy = bCombatEngaged ? GetCurrentEnemyTarget() : nullptr;
	SetCombatRotationEnabled(CombatEnemy != nullptr);
	if (CombatEnemy)
	{
		if (AAIController* AIController = GetAIController())
		{
			AIController->SetFocalPoint(CombatEnemy->GetActorLocation());
		}
	}

	if (BehaviorTreeRoot.IsValid())
	{
		BehaviorTreeRoot->Tick(this, DeltaTime);
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

bool UCompanionAIComponent::IsAimingRequested() const
{
	return bAimingRequested
		&& CombatComponent.IsValid()
		&& CombatComponent->GetEquippedWeapon() != nullptr;
}
bool UCompanionAIComponent::ShouldSprintWhileFollowing() const
{
	const AActor* Owner = GetOwner();
	const APawn* Player = CachedPlayerPawn.Get();
	if (!bFollowEnabled || bHasCommandedDestination || bCombatEngaged || bExploring || !Owner || !Player)
	{
		bFollowSprintActive = false;
		return false;
	}

	const float DistanceSquared = FVector::DistSquared(Owner->GetActorLocation(), Player->GetActorLocation());
	const float StartDistance = FMath::Max(FollowSprintDistance, FollowSprintStopDistance);
	const float StopDistance = FMath::Min(FollowSprintDistance, FollowSprintStopDistance);

	if (DistanceSquared >= FMath::Square(StartDistance))
	{
		bFollowSprintActive = true;
	}
	else if (DistanceSquared <= FMath::Square(StopDistance))
	{
		bFollowSprintActive = false;
	}

	return bFollowSprintActive;
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
}

void UCompanionAIComponent::CommandStop()
{
	bAimingRequested = false;
	bFollowEnabled = false;
	bHasCommandedDestination = false;
	bCombatEngaged = false;
	bCombatSuppressed = true;
	bExploring = false;

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
}

void UCompanionAIComponent::CommandEngage()
{
	bAimingRequested = CombatComponent.IsValid() && CombatComponent->GetEquippedWeapon() != nullptr;
	bCombatSuppressed = false;
	bCombatEngaged = true;
	bExploring = false;
}

void UCompanionAIComponent::CommandExplore()
{
	bAimingRequested = false;
	bExploring = true;
	bHasCommandedDestination = false;
	bCombatSuppressed = false;
	CurrentExploreTargetItem = nullptr;
	ExploreScanTimer = 0.0f;
	ExploreWanderTimer = 0.0f;
	ExploreMoveFailCount = 0;
	UnreachableExploreItems.Reset();

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

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(TEXT("CompanionAttackLOS"), false, Owner);
	Params.AddIgnoredActor(Owner);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, OwnerLocation, EnemyLocation, ECC_Visibility, Params);
	if (bBlocked && Hit.GetActor() != Enemy)
	{
		// 사선이 다른 것(엄폐물, 아군 등)에 막혀 있음 - 헛사격 방지.
		return false;
	}

	return true;
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
	OwnerCharacter->bUseControllerRotationYaw = bEnabled;

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

EBTNodeResult UCompanionAIComponent::DoAttack(float DeltaTime)
{
	AActor* Enemy = GetCurrentEnemyTarget();
	if (!Enemy)
	{
		return EBTNodeResult::Running;
	}

	StrafeTimer -= DeltaTime;
	if (StrafeTimer <= 0.0f)
	{
		StrafeTimer = StrafeInterval;
		StrafeDirection = FMath::RandBool() ? 1.0f : -1.0f;
	}

	RequestMoveToLocation(ComputeCombatMoveLocation(Enemy), MoveAcceptanceRadius);

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

EBTNodeResult UCompanionAIComponent::DoMoveToCommanded(float DeltaTime)
{
	FVector TargetLocation;

	if (bCommandedDestinationIsActor)
	{
		AActor* Target = CommandedTargetActor.Get();
		if (!IsValid(Target))
		{
			bHasCommandedDestination = false;
			return EBTNodeResult::Failed;
		}

		TargetLocation = Target->GetActorLocation();
		RequestMoveToActor(Target, MoveAcceptanceRadius);
	}
	else
	{
		TargetLocation = CommandedLocation;
		RequestMoveToLocation(CommandedLocation, MoveAcceptanceRadius);
	}

	AActor* Owner = GetOwner();
	if (Owner && FVector::DistSquared(Owner->GetActorLocation(), TargetLocation) <= FMath::Square(MoveAcceptanceRadius))
	{
		bHasCommandedDestination = false;
		return EBTNodeResult::Succeeded;
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
				if (ExploreMoveFailCount >= 3)
				{
					UE_LOG(LogCompanionAI, Warning, TEXT("[AI] %s 도달 불가로 판단해 포기(경로 탐색 %d회 실패)"),
						*TargetItem->GetName(), ExploreMoveFailCount);
					UnreachableExploreItems.Add(TargetItem);
					CurrentExploreTargetItem = nullptr;
					ExploreMoveFailCount = 0;
				}
			}
			else
			{
				ExploreMoveFailCount = 0;
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

	// 플레이어 픽업(ADropItem::OnInteract_Implementation)과 동일하게, 습득 전에 서버로 루팅을
	// 브로드캐스트한다 - 안 하면 다른 클라이언트 화면에는 아이템이 그대로 남아있게 된다.
	if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->SendInteractLoot(static_cast<int32>(Item->GetUniqueID()));
		}
	}

	const int32 CountToAdd = FMath::Max(1, Item->StackCount);
	int32 AddedCount = 0;
	for (; AddedCount < CountToAdd; ++AddedCount)
	{
		if (!InventoryComponent->AddItem(Item->ItemData))
		{
			break;
		}
	}

	if (AddedCount == 0)
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[AI] 습득 실패: %s - 인벤토리에 자리 없음(Grid %dx%d, 아이템 크기 %dx%d)"),
			*Item->ItemData->DisplayName.ToString(), InventoryComponent->GridColumns, InventoryComponent->GridRows,
			Item->ItemData->GridWidth, Item->ItemData->GridHeight);
	}

	if (AddedCount >= CountToAdd)
	{
		Item->Destroy();
	}
	else if (AddedCount > 0)
	{
		// 인벤토리 공간이 모자라 일부만 주웠으면 남은 수량은 드롭 아이템으로 그대로 둔다.
		Item->StackCount = CountToAdd - AddedCount;
	}

	if (AddedCount > 0)
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[AI] 아이템 습득: %s x%d"), *Item->ItemData->DisplayName.ToString(), AddedCount);
	}
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
	if (!Player || !Owner)
	{
		return EBTNodeResult::Running;
	}

	const float EffectiveFollowDistance = GetEffectiveFollowDistance();
	const float DistSq = FVector::DistSquared(Owner->GetActorLocation(), Player->GetActorLocation());
	if (DistSq > FMath::Square(EffectiveFollowDistance))
	{
		RequestMoveToActor(Player, EffectiveFollowDistance);
	}
	else if (DistSq < FMath::Square(MinFollowDistance))
	{
		if (AAIController* AIController = GetAIController())
		{
			AIController->StopMovement();
		}
	}

	return EBTNodeResult::Running;
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

