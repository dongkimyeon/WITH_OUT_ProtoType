#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemDataBase.h"
#include "ItemSpawnPoint.generated.h"

class UBillboardComponent;
class USphereComponent;

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

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UBillboardComponent* Billboard;

	// ScatterRadius를 에디터에서 와이어프레임 구로 보여주는 용도. 게임에선 숨김, 콜리전 없음.
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* RangeViz;
};
