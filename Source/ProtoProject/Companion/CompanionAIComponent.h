// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../AI/SimpleBehaviorTree.h"
#include "CompanionAIComponent.generated.h"

class AAIController;
class UCompanionCombatComponent;
class UCompanionPerceptionComponent;
class UInventoryGridComponent;
class ADropItem;

// 명령 이동 지점에 도달하지 못하고 정체(스턱)로 명령을 포기했을 때 브로드캐스트된다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCompanionMoveBlockedSignature);

// EnemyBase와 같은 방식(TBTNode<UCompanionAIComponent> 커스텀 트리)으로 Idle/Follow/
// MoveToLocation/MoveToTarget(Combat)/Attack 상태를 매 틱 평가하고 AIController 이동을 호출한다.
// 명령(CommandRouter)과 지각(Perception)이 세팅하는 bool 플래그로 상태 전이를 표현한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionAIComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float FollowDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float MinFollowDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float FollowSprintDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float FollowSprintStopDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float MoveAcceptanceRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float MoveRequestInterval = 0.25f;

	// 이동 명령/탐색 수행 중 이 시간(초) 동안 StuckDistanceThreshold보다 적게 움직이면 경로가
	// 막힌 것으로 본다(플레이어가 좁은 통로를 막고 서 있는 경우 등). UE 기본값에서 플레이어 캐릭터는
	// 내비메시를 깎지 않아 경로는 항상 플레이어를 통과하도록 계산되므로, 물리적으로 막히면
	// PathFollowing이 스스로 실패를 보고하지 않는다 - 여기서 직접 감지해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float StuckTimeThreshold = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float StuckDistanceThreshold = 50.0f;

	// 명령 이동이 정체될 때마다 좌/우 우회 지점을 경유해 본 목적지로 재접근을 시도하는 최대 횟수.
	// 이 횟수를 넘겨도 계속 막히면 명령을 포기하고 OnMoveCommandBlocked를 브로드캐스트한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	int32 MaxStuckRetries = 3;

	// 정체 시 잡는 우회 지점을 현재 위치에서 좌/우(재시도마다 번갈아)로 얼마나 벌릴지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float DetourDistance = 500.0f;

	// 우회 지점으로 이동을 시도하는 최대 시간(초). 이 안에 도달 못 하면 본 목적지 재시도로 넘어간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	float DetourTimeout = 5.0f;

	// 명령 이동이 MaxStuckRetries회 우회 재시도 후에도 막혀 포기했을 때 브로드캐스트.
	// Router가 받아 확인 대사를 들려준다.
	UPROPERTY(BlueprintAssignable, Category = "Companion|AI")
	FCompanionMoveBlockedSignature OnMoveCommandBlocked;

	// 적을 시야에 두면(Perception) 자동으로 교전 상태로 전환할지 여부. 기본값 false: 플레이어가
	// "싸워"/CommandEngage()로 명령하기 전까지는 적을 발견해도 공격하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|AI")
	bool bAutoEngageEnabled = false;

	// 이 동료가 따라다닐 플레이어를 명시적으로 지정한다. 스폰한 쪽(AProtoCharacter)이 스폰 직후
	// 호출하며, 호출되지 않으면 BeginPlay가 기존처럼 UGameplayStatics::GetPlayerPawn(0)으로 폴백한다.
	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void SetFollowTarget(APawn* Player);

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandFollow();

	// 최우선으로 관철되는 정지 명령: 이동/교전을 모두 즉시 중단하고 재교전을 억제한다.
	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandStop();

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandMoveToLocation(const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandMoveToActor(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandEngage();

	// 명령 지점 주변을 배회하며 근처 ADropItem을 찾아 인벤토리에 습득한다. ExploreDuration이 지나거나
	// 다른 명령이 들어오면 종료된다.
	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void CommandExplore();

	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	bool IsCombatEngaged() const { return bCombatEngaged; }

	// 머리 위 상태 라벨(UI) 등에 쓸 한국어 상태 텍스트. BuildBehaviorTree() 루트 우선순위와 동일한
	// 순서(사망 > 전투 > 이동명령 > 탐색 > 따라가기 > 정지)로 판단한다.
	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	FText GetStatusDisplayText() const;

	// 현재 조준/교전 중인 적(교전 중이 아니면 nullptr). 무기가 화면/카메라 기반 조준 대신 이 액터를
	// 직접 겨냥하는 데 쓴다 - AIController의 ControlRotation은 컨트롤러 자신의 틱에서 갱신되므로
	// 매 프레임 즉시 반영되지 않을 수 있어, 그 값을 거치면 교전 진입 첫 프레임에 방향이 어긋날 수 있다.
	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	AActor* GetCombatTarget() const { return bCombatEngaged ? GetCurrentEnemyTarget() : nullptr; }

	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	bool IsAimingRequested() const;

	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void ClearAimingRequest();

	// 재장전 완료 등으로 꺼졌던 조준 요청을 되살린다. 교전이 억제된 상태(CommandStop 등)에서는
	// 무시되며, 무기가 장착되어 있을 때만 의미가 있다.
	UFUNCTION(BlueprintCallable, Category = "Companion|AI")
	void RequestAiming();

	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	bool ShouldSprintWhileFollowing() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Explore")
	float ExploreDuration = 30.0f;

	// 탐색 시작 지점 기준 배회 반경.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Explore")
	float ExploreRadius = 2000.0f;

	// 이 범위 안의 ADropItem을 자동으로 주우러 간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Explore")
	float ExploreSearchRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Explore")
	float PickupRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Explore")
	float ExploreScanInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Explore")
	float ExploreWanderInterval = 4.0f;

	// 플레이어와 이 거리 이상 벌어지면 탐색을 중단하고 Follow로 복귀한다(길을 잃지 않게).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Explore")
	float MaxExploreDistanceFromPlayer = 4000.0f;

	// companion.FollowDistance 콘솔 변수로 덮어써진 경우 그 값을, 아니면 FollowDistance를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Companion|AI")
	float GetEffectiveFollowDistance() const;

	// 전투 중 제자리에 서 있지 않고 이 시간마다 좌우 스트레이프 방향을 새로 고른다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float StrafeInterval = 1.5f;

	// 스트레이프 시 좌우로 벌리는 거리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float StrafeDistance = 400.0f;

	// 적과 이 거리보다 가까워지면 스트레이프 대신 뒤로 물러나 사거리를 벌린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float MinAttackDistance = 600.0f;

	// 전투 중이어도 플레이어에게서 이 거리 이상은 벗어나지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float MaxCombatDistanceFromPlayer = 1500.0f;

	// 적을 놓친 뒤 실제로 교전을 해제하기까지의 유예시간. 스트레이프/회전 중 시야각 경계에서
	// 순간적으로 적을 놓쳐도(한두 틱) 이 시간 안에 다시 보이면 교전을 계속 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float CombatDisengageGraceTime = 1.5f;

	// 전투 중 회전을 스냅이 아니라 이 속도로 적 방향으로 보간한다(FMath::RInterpTo 계수).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float CombatRotationInterpSpeed = 8.0f;

	// true면 전투 중 위치를 랜덤 스트레이프 대신 Utility 점수(사선/엄폐/플레이어 근접/이동 비용)로
	// 고른다. false면 기존 ComputeCombatMoveLocation 랜덤 좌우 스트레이프로 폴백(비교/롤백용).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	bool bTacticalPositioningEnabled = true;

	// 부분 엄폐 판정용 허리 높이 프로브. 이 높이의 사선이 막히고 눈높이는 뚫리면 "낮은 엄폐물 뒤"로 본다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float CoverProbeHeightLow = 40.0f;

	// 사격 가능 판정용 눈높이 프로브.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float EyeProbeHeight = 60.0f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TSharedPtr<TBTNode<UCompanionAIComponent>> BehaviorTreeRoot;

	TWeakObjectPtr<AAIController> CachedAIController;
	TWeakObjectPtr<APawn> CachedPlayerPawn;
	TWeakObjectPtr<UCompanionCombatComponent> CombatComponent;
	TWeakObjectPtr<UCompanionPerceptionComponent> PerceptionComponent;
	TWeakObjectPtr<UInventoryGridComponent> InventoryComponent;

	bool bFollowEnabled = true;
	bool bHasCommandedDestination = false;
	bool bCommandedDestinationIsActor = false;
	bool bCombatEngaged = false;
	bool bCombatSuppressed = false;
	bool bAimingRequested = false;
	bool bExploring = false;
	mutable bool bFollowSprintActive = false;

	// 플레이어가 이 동료에게 CommandEngage()로 전투를 지시한 적이 있는지. 재장전 중 적을
	// 재감지했을 때 자동으로 조준을 재무장할지 판단하는 데 쓰인다(bAutoEngageEnabled=false
	// 정책을 위반하지 않도록, 한 번도 교전 지시를 받지 않은 동료는 자동 재무장하지 않는다).
	bool bHasEverEngaged = false;

	float LastEnemyVisibleTime = -FLT_MAX;

	FVector CommandedLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> CommandedTargetActor;

	FVector ExploreOriginLocation = FVector::ZeroVector;
	float ExploreEndTime = 0.0f;
	float ExploreScanTimer = 0.0f;
	float ExploreWanderTimer = 0.0f;
	TWeakObjectPtr<AActor> CurrentExploreTargetItem;
	int32 ExploreMoveFailCount = 0;
	// 경로 탐색이 반복 실패해 포기한 아이템들 - 이번 탐색 세션 동안은 재선택하지 않는다.
	TArray<TWeakObjectPtr<AActor>> UnreachableExploreItems;

	float MoveRequestTimer = 0.0f;

	// 이동 정체(스턱) 감지 상태.
	FVector StuckAnchorLocation = FVector::ZeroVector;
	float StuckElapsed = 0.0f;
	double LastStuckTickTime = -1.0;

	// 명령 이동 정체 시 우회 재시도 상태.
	int32 CommandedMoveStuckRetries = 0;
	bool bCommandedMoveDetouring = false;
	FVector CommandedDetourLocation = FVector::ZeroVector;
	float CommandedDetourEndTime = 0.0f;

	float StrafeTimer = 0.0f;
	float StrafeDirection = 1.0f;
	bool bCombatRotationActive = false;

	// Utility 포지셔닝 캐시: StrafeInterval마다(또는 적이 크게 움직였을 때) 재평가해 채운다.
	FVector CachedTacticalLocation = FVector::ZeroVector;
	bool bHasTacticalLocation = false;
	// 마지막 평가 시점의 적 위치 - 적이 StrafeDistance 이상 이동하면 즉시 재평가한다.
	FVector TacticalEvalEnemyLocation = FVector::ZeroVector;

	void BuildBehaviorTree();
	AAIController* GetAIController();

	UFUNCTION()
	void HandleEnemySpotted(AActor* EnemyActor);

	UFUNCTION()
	void HandleEnemyLost(AActor* EnemyActor);

	bool HasEnemyTarget() const;
	AActor* GetCurrentEnemyTarget() const;
	bool IsEnemyInAttackRangeWithLineOfSight() const;

	// 전투 중엔 이동 방향이 아니라 적을 향하도록 회전 방식을 바꾼다(스트레이프 중에도 조준 유지).
	// 전투가 끝나면 원래(이동 방향으로 회전)로 되돌린다.
	void SetCombatRotationEnabled(bool bEnabled);

	// 적 기준 현재 거리를 사거리 안쪽으로 유지하면서 좌우로 벌린 목표 지점을 계산한다.
	// 그 지점이 플레이어에게서 MaxCombatDistanceFromPlayer보다 멀면 플레이어 쪽으로 당겨온다.
	// (bTacticalPositioningEnabled=false일 때, 그리고 Utility 평가가 실패했을 때의 폴백.)
	FVector ComputeCombatMoveLocation(AActor* Enemy) const;

	// 적 중심 궤도 링 위의 각도 오프셋 후보들을 내비메시에 투영하고 ScoreTacticalCandidate로
	// 점수화해 최고점 위치를 고른다. 유효 후보가 하나도 없으면 false(호출자가 폴백).
	bool EvaluateTacticalPosition(AActor* Enemy, FVector& OutLocation) const;

	// 후보 위치 하나의 Utility 점수: 부분 엄폐 보너스 + 플레이어 이탈/이동 거리 감점
	// + 직전 선택 위치 근접 보너스(히스테리시스). 사선 확보 여부(bCanFire)는 호출자가 판정해
	// 넘긴다 - 사선 확보 후보가 하나도 없을 때의 폴백 선택에도 같은 점수를 재사용하기 위해.
	float ScoreTacticalCandidate(const FVector& Candidate, AActor* Enemy, bool bCanFire) const;

	// From 지점(높이 오프셋 포함 좌표 그대로)에서 적 상체로의 사선이 뚫려 있으면 true.
	bool HasLineOfSightFrom(const FVector& From, AActor* Enemy) const;

	EBTNodeResult DoAttack(float DeltaTime);
	EBTNodeResult DoMoveToEnemy(float DeltaTime);
	EBTNodeResult DoMoveToCommanded(float DeltaTime);
	EBTNodeResult DoExplore(float DeltaTime);
	EBTNodeResult DoFollow(float DeltaTime);
	EBTNodeResult DoIdle(float DeltaTime);

	AActor* FindNearestDropItem(float SearchRadius) const;
	void TryPickupItem(ADropItem* Item);

	// 경로 탐색이 명시적으로 실패(EPathFollowingRequestResult::Failed)하면 true를 반환한다.
	// (쓰로틀로 요청 자체를 건너뛴 경우는 실패로 치지 않는다.)
	bool RequestMoveToActor(AActor* Target, float AcceptRadius);
	void RequestMoveToLocation(const FVector& Location, float AcceptRadius);

	// 이동 명령/탐색 Action에서 매 틱 호출한다. 다른 상태에 있다가 돌아온 경우(연속 호출이
	// 0.5초 이상 끊긴 경우) 추적을 자동으로 리셋하고, 정체가 StuckTimeThreshold를 넘으면 true.
	bool TickStuckDetection(float DeltaTime);
	void ResetStuckDetection();

	// 정체 시 호출. 현재 위치에서 목적지 방향의 좌/우(재시도마다 번갈아)로 DetourDistance만큼
	// 벌린 지점을 내비메시에 투영해 우회 목표로 잡는다. 실패하면 주변 랜덤 도달 가능 지점으로
	// 폴백하고, 그것도 없으면 false.
	bool ComputeDetourLocation(const FVector& TowardTarget, FVector& OutDetour) const;

	// 명령 이동을 포기한다: 상태 초기화 + StopMovement + OnMoveCommandBlocked 브로드캐스트.
	void AbandonCommandedMove();
};


