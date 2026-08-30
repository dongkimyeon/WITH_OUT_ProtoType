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

UENUM(BlueprintType)
enum class EItemContextAction : uint8
{
    None,   // 우클릭 동작 없음
    Equip,  // 장착 슬롯으로 이동
    Use     // 즉시 소모/사용
};

// 아이템 등급. 스폰 지점/컨테이너의 지역 티어가 N이면 Tier <= N인 아이템만 후보가 된다.
UENUM(BlueprintType)
enum class EItemTier : uint8
{
    Tier1 = 0,  // 흔함
    Tier2,
    Tier3,
    Tier4,
    Tier5       // 희귀
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

    // 아이템 아이콘 
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

    // 아이템 등급 (스폰 티어 필터에 사용)
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    EItemTier Tier = EItemTier::Tier1;

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    TSoftObjectPtr<UStaticMesh> ItemMesh;

    // 스폰(스폰 포인트/적 루트)으로 월드에 놓일 때 드롭 메시에 적용할 스케일.
    // 기본값(1,1,1)이면 적용하지 않음 - BP_DropItem_* 를 직접 배치한 경우의 스케일을 유지.
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    FVector WorldMeshScale = FVector(1.f);

    // 우클릭 시 수행할 동작 오버라이드
    virtual EItemContextAction GetContextAction() const { return EItemContextAction::None; }

    virtual FText GetContextActionText() const;

    virtual bool IsUsable() const { return GetContextAction() == EItemContextAction::Use; }
};