#pragma once

#include "CoreMinimal.h"
#include "ItemContainerBase.h"
#include "ItemDataBase.h"
#include "../../Network/ProtoNetClientSubsystem.h"
#include "LootContainer.generated.h"

class UItemDataBase;

// 스테이지에 배치되는 파밍용 루팅 박스 한 칸. 항목별로 독립적으로 확률 판정해 스폰 여부/수량을 정한다.
USTRUCT(BlueprintType)
struct FLootTableEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Loot")
	TObjectPtr<UItemDataBase> Item;

	// 이 아이템이 스폰될 확률(0~1). 항목마다 독립적으로 판정한다.
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinStackCount = 1;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxStackCount = 1;
};

UCLASS(meta = (PrioritizeCategories = "Loot"))
class PROTOPROJECT_API ALootContainer : public AItemContainerBase
{
	GENERATED_BODY()

public:
	ALootContainer();

	// LootTable이 비어 있을 때 사용. Tier <= RegionTier인 아이템이 티어별 가중치로 채워진다.
	UPROPERTY(EditAnywhere, Category = "Loot")
	EItemTier RegionTier = EItemTier::Tier1;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinItems = 1;

	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxItems = 3;

	// 수동 지정 루트 테이블. 비워두면 위 RegionTier 기준으로 자동 채운다.
	UPROPERTY(EditAnywhere, Category = "Loot")
	TArray<FLootTableEntry> LootTable;

protected:
	virtual void SeedContents() override;

private:
	// Fired once the server answers this container's loot roll. If another
	// client's roll got there first, replaces whatever this instance seeded
	// locally with the authoritative contents; if this instance's own roll
	// won, it's a same-as-already-shown no-op.
	UFUNCTION()
	void HandleContainerLootState(int32 ContainerId, const TArray<FProtoInventoryItemEntry>& Items);
};
