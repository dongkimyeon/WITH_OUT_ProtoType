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

bool UInventoryGridComponent::AddItemAt(UItemDataBase* NewItem, const FIntPoint& Position, bool bRotate)
{
    if (!NewItem) return false;

    FIntPoint ItemSize = bRotate ? FIntPoint(NewItem->GridHeight, NewItem->GridWidth) : FIntPoint(NewItem->GridWidth, NewItem->GridHeight);

    if (CanPlaceAt(Position, ItemSize))
    {
        FInventoryItemInstance NewInstance;
        NewInstance.ItemData = NewItem;
        NewInstance.GridPosition = Position;
        NewInstance.bIsRotated = bRotate;
        NewInstance.StackCount = 1;

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
    Items.RemoveAt(ItemIndex);
    return Data;
}

bool UInventoryGridComponent::AddItem(UItemDataBase* NewItem)
{
    if (!NewItem) return false;

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

    // 두 번 다 실패했다면 진짜로 가방이 꽉 찬 것입니다.
    // TODO: "가방이 가득 찼습니다" UI 메시지 띄우기
    return false; 
}
