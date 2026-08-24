#pragma once

#include "CoreMinimal.h"
#include "ItemContainerBase.h"
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

// 랜덤 확률로 내용물이 채워지는 스테이지 파밍용 루팅 박스. 레벨을 새로 로드할 때마다 다시 리롤된다.
// (다른 플레이어와 결과를 공유하려면 서버 권위 상태가 필요 - 지금은 클라이언트 로컬 전용.)
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
};
