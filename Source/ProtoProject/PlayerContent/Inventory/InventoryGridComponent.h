#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemDataBase.h" // 우리가 만든 아이템 데이터 원본
#include "InventoryGridComponent.generated.h"

// 1. 가방 속에 존재하는 '아이템 1개'의 인스턴스 정보
USTRUCT(BlueprintType)
struct FInventoryItemInstance
{
    GENERATED_BODY()

    // 원본 데이터 에셋을 가리키는 포인터 (메모리 최적화)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UItemDataBase* ItemData = nullptr;

    // 현재 가방 안에서의 좌상단(Top-Left) 좌표 (X: 가로, Y: 세로)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FIntPoint GridPosition = FIntPoint(0, 0);

    // 90도 회전 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bIsRotated = false;

    // 겹쳐진 수량 (총알 등)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 StackCount = 1;

    // 회전 상태를 고려해서, 현재 그리드에서 차지하는 '진짜 가로/세로 길이'를 반환하는 함수
    FIntPoint GetEffectiveSize() const
    {
        if (!ItemData) 
        {
            return FIntPoint::ZeroValue;
        }

        // 회전되어 있으면 X와 Y를 뒤집어서 반환
        return bIsRotated
            ? FIntPoint(ItemData->GridHeight, ItemData->GridWidth)
            : FIntPoint(ItemData->GridWidth, ItemData->GridHeight);
    }
    
    bool operator==(const FInventoryItemInstance& Other) const
    {
        return ItemData == Other.ItemData && GridPosition == Other.GridPosition;
    }
};

// 2. 테트리스 가방 시스템 본체
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOPROJECT_API UInventoryGridComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryGridComponent();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

public:
    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 인벤토리의 가로 총 칸 수
    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    int32 GridColumns = 10;

    // 인벤토리의 세로 총 칸 수
    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    int32 GridRows = 6;

    // 현재 가방 안에 들어있는 아이템들의 목록
    UPROPERTY(VisibleAnywhere, Category = "Inventory")
    TArray<FInventoryItemInstance> Items;

    bool CanPlaceAt(const FIntPoint& Origin, const FIntPoint& Size, int32 IgnoreIndex = INDEX_NONE) const;

private:
    static bool DoesOverlap(const FIntPoint& OriginA, const FIntPoint& SizeA, const FInventoryItemInstance& B);
    
public:
   
    bool FindEmptySpace(const FIntPoint& ItemSize, FIntPoint& OutPosition) const;

    // F키로 주웠을 때 자동으로 넣기 
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(UItemDataBase* NewItem);
    
    // 수동으로 드래그해서 넣기
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItemAt(UItemDataBase* NewItem, const FIntPoint& Position, bool bRotate = false);
};