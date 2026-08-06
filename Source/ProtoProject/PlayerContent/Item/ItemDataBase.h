#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
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

// 인벤토리에서 아이템을 우클릭했을 때 수행할 동작. 카테고리가 늘어나도 이 enum의 케이스만으로 UI가 분기하도록 한다.
UENUM(BlueprintType)
enum class EItemContextAction : uint8
{
    None,   // 우클릭 동작 없음 (Material, ComputingPart 등)
    Equip,  // 장착 슬롯으로 이동
    Use     // 즉시 소모/사용
};

UCLASS(Abstract, BlueprintType, meta = (PrioritizeCategories = "Item"))
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

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    TSoftObjectPtr<UStaticMesh> ItemMesh;

    // 우클릭 시 수행할 동작. 서브클래스는 이 함수만 override하면 우클릭 UI/디스패치에 자동으로 반영된다.
    virtual EItemContextAction GetContextAction() const { return EItemContextAction::None; }

    // GetContextAction()에 대응하는 UI 표시 텍스트("장착"/"사용" 등). 구현은 .cpp 참고.
    virtual FText GetContextActionText() const;

    virtual bool IsUsable() const { return GetContextAction() == EItemContextAction::Use; }
};