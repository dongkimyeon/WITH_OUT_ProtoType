// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryGridComponent.h"


// Sets default values for this component's properties
UInventoryGridComponent::UInventoryGridComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

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
    // Y(세로)를 먼저, 그 다음 X(가로)를 훑습니다. (왼쪽 위에서부터 오른쪽으로 읽어나가는 방식)
    for (int32 y = 0; y <= GridRows - ItemSize.Y; ++y)
    {
        for (int32 x = 0; x <= GridColumns - ItemSize.X; ++x)
        {
            FIntPoint TestPosition(x, y);
            
            // 우리가 아까 만든 그 철통 보안 알고리즘을 여기서 써먹습니다!
            if (CanPlaceAt(TestPosition, ItemSize))
            {
                OutPosition = TestPosition;
                return true; // 자리 찾음!
            }
        }
    }
    return false; // 가방에 여유 공간이 없음
}

bool UInventoryGridComponent::AddItemAt(UItemDataBase* NewItem, const FIntPoint& Position, bool bRotate)
{
    if (!NewItem) return false;

    // 회전 상태를 고려한 진짜 크기를 계산
    FIntPoint ItemSize = bRotate ? FIntPoint(NewItem->GridHeight, NewItem->GridWidth) : FIntPoint(NewItem->GridWidth, NewItem->GridHeight);

    // 마지막으로 한 번 더 겹치는지 검사 (이중 보안)
    if (CanPlaceAt(Position, ItemSize))
    {
        // 겹치지 않는다면 가방(배열)에 진짜로 추가!
        FInventoryItemInstance NewInstance;
        NewInstance.ItemData = NewItem;
        NewInstance.GridPosition = Position;
        NewInstance.bIsRotated = bRotate;
        NewInstance.StackCount = 1;

        Items.Add(NewInstance);
        return true;
    }
    return false; // 누군가 마우스로 억지로 겹치는 곳에 놓으려 함 -> 거절!
}

bool UInventoryGridComponent::AddItem(UItemDataBase* NewItem)
{
    if (!NewItem) return false;

    FIntPoint FoundPosition;
    
    // 1차 시도: 회전하지 않은 원래 크기 그대로 빈 공간이 있는지 찾습니다.
    FIntPoint NormalSize(NewItem->GridWidth, NewItem->GridHeight);
    if (FindEmptySpace(NormalSize, FoundPosition))
    {
        return AddItemAt(NewItem, FoundPosition, false); // 안 돌리고 넣음
    }

    // 2차 시도 (타르코프식 꿀잼 디테일!): 
    // 그냥 넣을 자리가 없다면? 시스템이 알아서 90도 돌려보고 남는 자리가 있는지 한 번 더 찾습니다.
    FIntPoint RotatedSize(NewItem->GridHeight, NewItem->GridWidth);
    if (FindEmptySpace(RotatedSize, FoundPosition))
    {
        return AddItemAt(NewItem, FoundPosition, true); // 돌려서 넣음
    }

    // 두 번 다 실패했다면 진짜로 가방이 꽉 찬 것입니다.
    // TODO: "가방이 가득 찼습니다" UI 메시지 띄우기
    return false; 
}
