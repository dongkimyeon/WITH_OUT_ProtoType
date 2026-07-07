#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDataBase.generated.h"

// 1. 아이템 카테고리를 분류하는 열거형 (Enum)
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    Consumable,     // 소모품 (체력, 감염도 회복 등)
    Material,       // 제작 재료
    Equipment,      // 장비 (무기, 방어구)
    ComputingPart,  // 컴퓨팅 부품 (CPU, GPU 등)
    Bag             // 가방 (인벤토리 확장용)
};

// 2. Abstract를 붙여서 이 베이스 클래스 자체로는 데이터 에셋을 못 만들게 막습니다.
// (반드시 소모품, 무기 등의 자식 클래스를 통해서만 만들도록 강제)
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

    // [그리드 인벤토리 핵심] 가로 x 세로 칸 수 (회전 전 기준)
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    int32 GridWidth = 1;

    UPROPERTY(EditDefaultsOnly, Category = "Item")
    int32 GridHeight = 1;

    // 탄약이나 재료처럼 한 칸에 여러 개가 겹쳐지는 아이템인가?
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    bool bIsStackable = false;

    // 겹쳐진다면 최대 몇 개까지 겹칠 수 있는가?
    // (meta = (EditCondition) 덕분에 bIsStackable이 true일 때만 에디터에서 수정 가능)
    UPROPERTY(EditDefaultsOnly, Category = "Item", meta = (EditCondition = "bIsStackable"))
    int32 MaxStackCount = 1;

    // 아이템 카테고리 (기본값 설정)
    UPROPERTY(EditDefaultsOnly, Category = "Item")
    EItemCategory Category = EItemCategory::Material;

    // 사용 가능한 아이템인지 확인하는 가상 함수 (자식 클래스에서 재정의할 예정)
    virtual bool IsUsable() const { return false; }
};