#pragma once

#include "CoreMinimal.h"
#include "ItemContainerBase.h"
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

// 랜덤 확률로 내용물이 채워지는 스테이지 파밍용 루팅 박스. 매번 각자 로컬에서 리롤하지만,
// 서버에 처음 보고된 결과가 권위 있는 정답이 되어 모든 클라이언트에게 재방송되므로
// (SendContainerLootRoll/OnContainerLootState 참고) 다른 플레이어와 내용물이 항상 일치한다.
UCLASS()
class PROTOPROJECT_API ALootContainer : public AItemContainerBase
{
	GENERATED_BODY()

public:
	ALootContainer();

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
