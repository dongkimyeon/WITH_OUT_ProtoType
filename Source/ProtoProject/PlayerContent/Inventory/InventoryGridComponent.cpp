// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryGridComponent.h"


// Sets default values for this component's properties
UInventoryGridComponent::UInventoryGridComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UInventoryGridComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UInventoryGridComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

bool UInventoryGridComponent::CanPlaceAt(const FIntPoint& Origin, const FIntPoint& Size, int32 IgnoreIndex) const
{
	if (Origin.X < 0 || Origin.Y < 0 || 
		Origin.X + Size.X > GridColumns || 
		Origin.Y + Size.Y > GridRows)
	{
		return false;
	}

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (i == IgnoreIndex) 
		{
			continue; 
		}

		if (DoesOverlap(Origin, Size, Items[i]))
		{
			return false;
		}
	}

	return true; 
}

bool UInventoryGridComponent::DoesOverlap(const FIntPoint& OriginA, const FIntPoint& SizeA, const FInventoryItemInstance& B)
{
	FIntPoint OriginB = B.GridPosition;
	FIntPoint SizeB = B.GetEffectiveSize(); 

	bool bOverlapX = (OriginA.X < OriginB.X + SizeB.X) && (OriginA.X + SizeA.X > OriginB.X);
    
	bool bOverlapY = (OriginA.Y < OriginB.Y + SizeB.Y) && (OriginA.Y + SizeA.Y > OriginB.Y);

	return bOverlapX && bOverlapY;
}


bool UInventoryGridComponent::FindEmptySpace(const FIntPoint& ItemSize, FIntPoint& OutPosition) const
{
    for (int32 y = 0; y <= GridRows - ItemSize.Y; ++y)
    {
        for (int32 x = 0; x <= GridColumns - ItemSize.X; ++x)
        {
            FIntPoint TestPosition(x, y);
            
            if (CanPlaceAt(TestPosition, ItemSize))
            {
                OutPosition = TestPosition;
                return true; 
            }
        }
    }
    return false; 
}

bool UInventoryGridComponent::AddItemAt(UItemDataBase* NewItem, const FIntPoint& Position, bool bRotate, int32 StackCount)
{
    if (!NewItem) return false;

    FIntPoint ItemSize = bRotate ? FIntPoint(NewItem->GridHeight, NewItem->GridWidth) : FIntPoint(NewItem->GridWidth, NewItem->GridHeight);

    if (CanPlaceAt(Position, ItemSize))
    {
        FInventoryItemInstance NewInstance;
        NewInstance.ItemData = NewItem;
        NewInstance.GridPosition = Position;
        NewInstance.bIsRotated = bRotate;
        NewInstance.StackCount = FMath::Max(1, StackCount);
        NewInstance.InstanceId = FGuid::NewGuid();

        Items.Add(NewInstance);
        return true;
    }
    return false; 
}

bool UInventoryGridComponent::MoveItem(int32 ItemIndex, const FIntPoint& NewPosition)
{
    if (!Items.IsValidIndex(ItemIndex)) return false;

    FInventoryItemInstance& Item = Items[ItemIndex];
    FIntPoint ItemSize = Item.GetEffectiveSize();

    if (CanPlaceAt(NewPosition, ItemSize, ItemIndex))
    {
        Item.GridPosition = NewPosition;
        return true;
    }
    return false;
}

bool UInventoryGridComponent::RotateItem(int32 ItemIndex)
{
    if (!Items.IsValidIndex(ItemIndex)) return false;

    FInventoryItemInstance& Item = Items[ItemIndex];
    if (!Item.ItemData) return false;

    bool bNewRotated = !Item.bIsRotated;
    FIntPoint NewSize = bNewRotated
        ? FIntPoint(Item.ItemData->GridHeight, Item.ItemData->GridWidth)
        : FIntPoint(Item.ItemData->GridWidth, Item.ItemData->GridHeight);

    if (CanPlaceAt(Item.GridPosition, NewSize, ItemIndex))
    {
        Item.bIsRotated = bNewRotated;
        return true;
    }
    return false;
}

bool UInventoryGridComponent::PlaceItem(int32 ItemIndex, const FIntPoint& NewPosition, bool bNewRotated)
{
    if (!Items.IsValidIndex(ItemIndex)) return false;

    FInventoryItemInstance& Item = Items[ItemIndex];
    if (!Item.ItemData) return false;

    FIntPoint NewSize = bNewRotated
        ? FIntPoint(Item.ItemData->GridHeight, Item.ItemData->GridWidth)
        : FIntPoint(Item.ItemData->GridWidth,  Item.ItemData->GridHeight);

    if (CanPlaceAt(NewPosition, NewSize, ItemIndex))
    {
        Item.GridPosition = NewPosition;
        Item.bIsRotated = bNewRotated;
        return true;
    }
    return false;
}

UItemDataBase* UInventoryGridComponent::RemoveItemAt(int32 ItemIndex)
{
    if (!Items.IsValidIndex(ItemIndex)) return nullptr;
    UItemDataBase* Data = Items[ItemIndex].ItemData;
    FGuid RemovedId = Items[ItemIndex].InstanceId;
    Items.RemoveAt(ItemIndex);
    OnItemInstanceRemoved.Broadcast(RemovedId);
    return Data;
}

int32 UInventoryGridComponent::FindIndexById(const FGuid& InstanceId) const
{
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].InstanceId == InstanceId)
        {
            return i;
        }
    }
    return INDEX_NONE;
}

bool UInventoryGridComponent::FindInstanceById(const FGuid& InstanceId, FInventoryItemInstance& OutInstance) const
{
    const int32 Index = FindIndexById(InstanceId);
    if (Index == INDEX_NONE) return false;
    OutInstance = Items[Index];
    return true;
}

UItemDataBase* UInventoryGridComponent::RemoveInstanceById(const FGuid& InstanceId)
{
    const int32 Index = FindIndexById(InstanceId);
    if (Index == INDEX_NONE) return nullptr;
    return RemoveItemAt(Index);
}

UItemDataBase* UInventoryGridComponent::SplitStack(const FGuid& InstanceId, int32 Count, int32& OutSplitCount)
{
    OutSplitCount = 0;

    const int32 Index = FindIndexById(InstanceId);
    if (Index == INDEX_NONE || Count <= 0) return nullptr;

    UItemDataBase* ItemData = Items[Index].ItemData;

    if (Count >= Items[Index].StackCount)
    {
        OutSplitCount = Items[Index].StackCount;
        RemoveItemAt(Index);
    }
    else
    {
        OutSplitCount = Count;
        Items[Index].StackCount -= Count;
    }

    return ItemData;
}

bool UInventoryGridComponent::MergeStackFrom(UInventoryGridComponent* SourceInventory, const FGuid& SourceInstanceId, const FGuid& TargetInstanceId)
{
    if (!SourceInventory) return false;

    FInventoryItemInstance TargetSnapshot;
    if (!FindInstanceById(TargetInstanceId, TargetSnapshot)) return false;
    if (!TargetSnapshot.ItemData || !TargetSnapshot.ItemData->bIsStackable) return false;

    FInventoryItemInstance SourceSnapshot;
    if (!SourceInventory->FindInstanceById(SourceInstanceId, SourceSnapshot)) return false;
    if (SourceSnapshot.ItemData != TargetSnapshot.ItemData) return false;

    const int32 Room = TargetSnapshot.ItemData->MaxStackCount - TargetSnapshot.StackCount;
    if (Room <= 0) return false;

    const int32 RequestCount = FMath::Min(Room, SourceSnapshot.StackCount);
    if (RequestCount <= 0) return false;

    int32 ActuallySplit = 0;
    SourceInventory->SplitStack(SourceInstanceId, RequestCount, ActuallySplit);
    if (ActuallySplit <= 0) return false;
    
    const int32 TargetIndex = FindIndexById(TargetInstanceId);
    if (TargetIndex == INDEX_NONE) return false;

    Items[TargetIndex].StackCount += ActuallySplit;
    return true;
}

bool UInventoryGridComponent::HasRoomFor(UItemDataBase* ItemData, int32 Count) const
{
    if (!ItemData) return false;
    if (Count <= 0) return true;

    int32 Remaining = Count;

    if (ItemData->bIsStackable)
    {
        for (const FInventoryItemInstance& Existing : Items)
        {
            if (Existing.ItemData == ItemData)
            {
                Remaining -= FMath::Max(0, ItemData->MaxStackCount - Existing.StackCount);
                if (Remaining <= 0) return true;
            }
        }
    }

    // 기존 스택으로 다 못 받으면, 남은 수량(최대 MaxStackCount)이 들어갈 새 칸이 최소 하나 있어야 한다.
    FIntPoint FoundPosition;
    const FIntPoint NormalSize(ItemData->GridWidth, ItemData->GridHeight);
    const FIntPoint RotatedSize(ItemData->GridHeight, ItemData->GridWidth);
    return FindEmptySpace(NormalSize, FoundPosition) || FindEmptySpace(RotatedSize, FoundPosition);
}

bool UInventoryGridComponent::AddItem(UItemDataBase* NewItem)
{
    if (!NewItem) return false;

    // 스택 가능한 아이템이면 기존 스택에 먼저 합쳐본다 (빈 공간을 새로 찾기 전에).
    if (NewItem->bIsStackable)
    {
        for (FInventoryItemInstance& Existing : Items)
        {
            if (Existing.ItemData == NewItem && Existing.StackCount < NewItem->MaxStackCount)
            {
                ++Existing.StackCount;
                return true;
            }
        }
    }

    FIntPoint FoundPosition;
    
    FIntPoint NormalSize(NewItem->GridWidth, NewItem->GridHeight);
    if (FindEmptySpace(NormalSize, FoundPosition))
    {
        return AddItemAt(NewItem, FoundPosition, false); 
    }

    FIntPoint RotatedSize(NewItem->GridHeight, NewItem->GridWidth);
    if (FindEmptySpace(RotatedSize, FoundPosition))
    {
        return AddItemAt(NewItem, FoundPosition, true); 
    }

    return false; 
}
