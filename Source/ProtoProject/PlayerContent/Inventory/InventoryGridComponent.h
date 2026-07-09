#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemDataBase.h" // 우리가 만든 아이템 데이터 원본
#include "InventoryGridComponent.generated.h"

// 아이템 인스턴스 정보 
USTRUCT(BlueprintType)
struct FInventoryItemInstance
{
    GENERATED_BODY()

  
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UItemDataBase* ItemData = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FIntPoint GridPosition = FIntPoint(0, 0);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool bIsRotated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 StackCount = 1;

    FIntPoint GetEffectiveSize() const
    {
        if (!ItemData) 
        {
            return FIntPoint::ZeroValue;
        }

        return bIsRotated
            ? FIntPoint(ItemData->GridHeight, ItemData-q>GridWidth)
            : FIntPoint(ItemData->GridWidth, ItemData->GridHeight);
    }
    
    bool operator==(const FInventoryItemInstance& Other) const
    {
        return ItemData == Other.ItemData && GridPosition == Other.GridPosition;
    }
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROTOPROJECT_API UInventoryGridComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryGridComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    int32 GridColumns = 10;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    int32 GridRows = 6;

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

    // 현재 위치에서 90도 회전 가능한지 검사 후 회전
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RotateItem(int32 ItemIndex);

    // 아이템을 새 위치로 이동 (겹침 검사 포함)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool MoveItem(int32 ItemIndex, const FIntPoint& NewPosition);

    // 위치와 회전을 동시에 적용 (드래그 드롭 완료 시 사용)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool PlaceItem(int32 ItemIndex, const FIntPoint& NewPosition, bool bNewRotated);
};