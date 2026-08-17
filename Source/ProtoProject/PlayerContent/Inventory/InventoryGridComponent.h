#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemDataBase.h"
#include "InventoryGridComponent.generated.h"

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
    
    // 고유 GUID
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FGuid InstanceId;

    FIntPoint GetEffectiveSize() const
    {
        if (!ItemData) 
        {
            return FIntPoint::ZeroValue;
        }

        return bIsRotated
            ? FIntPoint(ItemData->GridHeight, ItemData->GridWidth)
            : FIntPoint(ItemData->GridWidth, ItemData->GridHeight);
    }
    
    bool operator==(const FInventoryItemInstance& Other) const
    {
        return ItemData == Other.ItemData && GridPosition == Other.GridPosition;
    }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemRemoved, FGuid, RemovedInstanceId);

// Fired at the end of every successful mutation (add/move/rotate/remove/
// split/merge) so a single listener (see AProtoCharacter) can push the
// whole grid to the server without hooking every call site that touches
// Items -- widgets/other code call these methods directly, not through
// AProtoCharacter, so a per-call-site hook would be easy to miss one of.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "Inventory"))
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

    // 자리 있는지만 확인 (실제 추가 안 함)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool HasRoomFor(UItemDataBase* ItemData, int32 Count) const;

private:
    static bool DoesOverlap(const FIntPoint& OriginA, const FIntPoint& SizeA, const FInventoryItemInstance& B);
    
public:
   
    bool FindEmptySpace(const FIntPoint& ItemSize, FIntPoint& OutPosition) const;

    // 자동 배치로 추가
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(UItemDataBase* NewItem);

    // 정확한 위치에 추가
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItemAt(UItemDataBase* NewItem, const FIntPoint& Position, bool bRotate = false, int32 StackCount = 1);

    // 90도 회전
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RotateItem(int32 ItemIndex);

    // 위치 이동
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool MoveItem(int32 ItemIndex, const FIntPoint& NewPosition);

    // 위치+회전 동시 적용
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool PlaceItem(int32 ItemIndex, const FIntPoint& NewPosition, bool bNewRotated);

    // 제거 후 데이터 반환
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    UItemDataBase* RemoveItemAt(int32 ItemIndex);

    // 인스턴스ID로 인덱스 조회
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    int32 FindIndexById(const FGuid& InstanceId) const;

    // 인스턴스ID로 조회
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool FindInstanceById(const FGuid& InstanceId, FInventoryItemInstance& OutInstance) const;

    // 인스턴스ID로 제거
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    UItemDataBase* RemoveInstanceById(const FGuid& InstanceId);

    // 스택 일부 분리
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    UItemDataBase* SplitStack(const FGuid& InstanceId, int32 Count, int32& OutSplitCount);

    // 스택 병합
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool MergeStackFrom(UInventoryGridComponent* SourceInventory, const FGuid& SourceInstanceId, const FGuid& TargetInstanceId);

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryItemRemoved OnItemInstanceRemoved;

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;
};
