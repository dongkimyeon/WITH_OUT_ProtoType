#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDataBase.generated.h"

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    Consumable,     // 소모품 (체력, 감염도 회복 등)
    Material,       // 제작 재료
    Equipment,      // 장비 (무기, 방어구)
    ComputingPart,  // 컴퓨팅 부품 (CPU, GPU 등)
    Bag             // 가방 (인벤토리 확장용)
};

UCLASS(Abstract, BlueprintType)
class PROTOPROJECT_API UItemDataBase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 고유 ID
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    FName ItemId;

    // 인게임에서 보여질 이름
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    FText DisplayName;

    // 아이템 설명
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    FText Description;

    // 아이템 아이콘 (TSoftObjectPtr를 써서 필요할 때만 메모리에 로드하도록 최적화)
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    TSoftObjectPtr<class UTexture2D> Icon;

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    int32 GridWidth = 1;

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    int32 GridHeight = 1;

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    bool bIsStackable = false;

    UPROPERTY(EditDefaultsOnly, Category = "Item", meta = (EditCondition = "bIsStackable"))
    int32 MaxStackCount = 1;

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    EItemCategory Category = EItemCategory::Material;

    virtual bool IsUsable() const { return false; }
};