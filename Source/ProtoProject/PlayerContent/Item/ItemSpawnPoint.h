#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemDataBase.h"
#include "../../Network/ProtoNetClientSubsystem.h"
#include "ItemSpawnPoint.generated.h"

class UBillboardComponent;
class USphereComponent;
class ADropItem;

// 레벨에 배치해두면 BeginPlay에 RegionTier 기준으로 ADropItem을 월드에 스폰하는 지점.
// Tier <= RegionTier인 아이템만 후보가 되고, 티어별 희귀도 가중치로 뽑힌다(LootTierRoll).
UCLASS(meta = (PrioritizeCategories = "Loot"))
class PROTOPROJECT_API AItemSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AItemSpawnPoint();

	// 이 지점(지역)의 티어. 높을수록 상위 티어 아이템이 후보에 들어온다.
	UPROPERTY(EditAnywhere, Category = "Loot")
	EItemTier RegionTier = EItemTier::Tier1;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinItems = 1;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxItems = 3;

	UPROPERTY(EditAnywhere, Category = "Loot")
	bool bSpawnOnBeginPlay = true;

	// 여러 개가 나올 때 서로 겹치지 않도록 XY로 랜덤 산포시키는 반경(uu).
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0.0"))
	float ScatterRadius = 40.f;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void SpawnLoot();

	// Stable id shared across every client's copy of this level, derived
	// from the placed actor's own in-level name -- same idea as
	// AItemContainerBase::GetContainerId. Used to key the server-mediated
	// first-roll-wins arbitration (see SendItemSpawnRoll).
	int32 GetSpawnPointId() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBillboardComponent* Billboard;

	// ScatterRadius를 에디터에서 와이어프레임 구로 보여주는 용도. 게임에선 숨김, 콜리전 없음.
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* RangeViz;

	// Bound (if connected) to UProtoNetClientSubsystem::OnItemSpawnState:
	// replaces whatever SpawnLoot() spawned locally with the authoritative
	// set -- either this roll (if it was first) or an earlier client's.
	UFUNCTION()
	void HandleItemSpawnState(int32 SpawnPointId, const TArray<FProtoWorldItemEntry>& Items);

	// What SpawnLoot()/HandleItemSpawnState() currently has spawned, so a
	// later authoritative answer can destroy-and-replace them cleanly.
	UPROPERTY()
	TArray<TObjectPtr<ADropItem>> SpawnedDrops;
};
