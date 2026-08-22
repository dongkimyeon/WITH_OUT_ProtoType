// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionAIComponent.h"
#include "CompanionCombatComponent.h"
#include "CompanionPerceptionComponent.h"
#include "AIController.h"
#include "GameFramework/Character.h"
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

float UCompanionAIComponent::GetEffectiveFollowDistance() const
{
	const float Override = CVarCompanionFollowDistance.GetValueOnGameThread();
	return Override >= 0.0f ? Override : FollowDistance;
}

void UCompanionAIComponent::CommandFollow()
{
	bFollowEnabled = true;
	bHasCommandedDestination = false;
	bCombatSuppressed = false;
	bExploring = false;
}

void UCompanionAIComponent::CommandStop()
{
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
	bCombatSuppressed = false;
	bCombatEngaged = true;
	bExploring = false;
}

void UCompanionAIComponent::CommandExplore()
{
	bExploring = true;
	bHasCommandedDestination = false;
	bCombatSuppressed = false;
	CurrentExploreTargetItem = nullptr;
	ExploreScanTimer = 0.0f;
	ExploreWanderTimer = 0.0f;

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

EBTNodeResult UCompanionAIComponent::DoAttack(float DeltaTime)
{
	if (AAIController* AIController = GetAIController())
	{
		AIController->StopMovement();
	}

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

	RequestMoveToActor(Enemy, MoveAcceptanceRadius);
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
			bExploring = false;
			return EBTNodeResult::Failed;
		}
	}

	// 습득 대상이 있으면 그쪽으로 이동하고, 도착하면 줍는다.
	AActor* TargetItem = CurrentExploreTargetItem.Get();
	if (IsValid(TargetItem))
	{
		if (FVector::DistSquared(Owner->GetActorLocation(), TargetItem->GetActorLocation()) <= FMath::Square(PickupRadius))
		{
			TryPickupItem(Cast<ADropItem>(TargetItem));
			CurrentExploreTargetItem = nullptr;
		}
		else
		{
			RequestMoveToActor(TargetItem, PickupRadius);
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
			CurrentExploreTargetItem = Found;
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
		return;
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

void UCompanionAIComponent::RequestMoveToActor(AActor* Target, float AcceptRadius)
{
	AAIController* AIController = GetAIController();
	if (!Target || !AIController)
	{
		return;
	}

	if (MoveRequestTimer > 0.0f)
	{
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	if (FVector::DistSquared(TargetLocation, LastMoveRequestLocation) < FMath::Square(AcceptRadius * 0.5f))
	{
		return;
	}

	MoveRequestTimer = MoveRequestInterval;
	LastMoveRequestLocation = TargetLocation;
	AIController->MoveToActor(Target, AcceptRadius, true, true, true, nullptr, true);
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

	if (FVector::DistSquared(Location, LastMoveRequestLocation) < FMath::Square(AcceptRadius * 0.5f))
	{
		return;
	}

	MoveRequestTimer = MoveRequestInterval;
	LastMoveRequestLocation = Location;
	AIController->MoveToLocation(Location, AcceptRadius);
}

void UCompanionAIComponent::BuildBehaviorTree()
{
	TSharedPtr<FCompanionSelectorNode> Root = MakeShared<FCompanionSelectorNode>();

	// 1. 전투: bCombatEngaged && 적 존재 -> [사거리+사선 확인되면 공격, 아니면 접근]
	TSharedPtr<FCompanionSequenceNode> CombatSequence = MakeShared<FCompanionSequenceNode>();
	CombatSequence->Children.Add(MakeShared<FCompanionConditionNode>([](UCompanionAIComponent* AI)
	{
		return IsValid(AI) && AI->bCombatEngaged && AI->HasEnemyTarget();
	}));

	TSharedPtr<FCompanionSelectorNode> CombatInner = MakeShared<FCompanionSelectorNode>();

	TSharedPtr<FCompanionSequenceNode> AttackSequence = MakeShared<FCompanionSequenceNode>();
	AttackSequence->Children.Add(MakeShared<FCompanionConditionNode>([](UCompanionAIComponent* AI)
	{
		return IsValid(AI) && AI->IsEnemyInAttackRangeWithLineOfSight()
			&& AI->CombatComponent.IsValid() && AI->CombatComponent->CanAttack();
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
