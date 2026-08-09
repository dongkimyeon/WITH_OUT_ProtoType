#include "QuickSlotComponent.h"
#include "InventoryGridComponent.h"
#include "ItemDataBase.h"
#include "ConsumableItemData.h"
#include "../ProtoCharacter.h"

UQuickSlotComponent::UQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Slots.SetNum(NumSlots);
}

void UQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Slots.Num() != NumSlots)
	{
		Slots.SetNum(NumSlots);
	}
}

bool UQuickSlotComponent::CanRegisterToQuickSlot(UItemDataBase* ItemData) const
{
	return ItemData && ItemData->GetContextAction() == EItemContextAction::Use;
}

bool UQuickSlotComponent::RegisterFromInventory(int32 SlotIndex, UInventoryGridComponent* SourceInventory, const FGuid& InstanceId)
{
	if (!Slots.IsValidIndex(SlotIndex) || !SourceInventory) return false;

	FInventoryItemInstance SourceInstance;
	if (!SourceInventory->FindInstanceById(InstanceId, SourceInstance)) return false;

	UItemDataBase* NewItemData = SourceInstance.ItemData;
	if (!CanRegisterToQuickSlot(NewItemData)) return false;

	FQuickSlotEntry& Entry = Slots[SlotIndex];

	// 같은 아이템이면 교체 대신 수량 병합
	if (Entry.ItemData == NewItemData && NewItemData->bIsStackable)
	{
		const int32 Room = NewItemData->MaxStackCount - Entry.StackCount;
		if (Room <= 0) return false;

		const int32 MergeCount = FMath::Min(Room, SourceInstance.StackCount);
		int32 ActuallySplit = 0;
		SourceInventory->SplitStack(InstanceId, MergeCount, ActuallySplit);
		if (ActuallySplit <= 0) return false;

		Entry.StackCount += ActuallySplit;
		EnsureValidLastUsedSlot();
		OnQuickSlotChanged.Broadcast(SlotIndex);
		return true;
	}

	FQuickSlotEntry PrevEntry = Entry;

	// 반환 자리 사전 확인 (수량 소실 방지)
	if (PrevEntry.ItemData && !SourceInventory->HasRoomFor(PrevEntry.ItemData, PrevEntry.StackCount))
	{
		return false;
	}

	SourceInventory->RemoveInstanceById(InstanceId);

	if (PrevEntry.ItemData)
	{
		// 기존 아이템 반환
		const int32 CountToReturn = FMath::Max(1, PrevEntry.StackCount);
		for (int32 i = 0; i < CountToReturn; ++i)
		{
			const bool bReturned = SourceInventory->AddItem(PrevEntry.ItemData);
			ensureMsgf(bReturned, TEXT("QuickSlotComponent: HasRoomFor said there was room but AddItem failed - PrevEntry.StackCount may exceed MaxStackCount"));
		}
	}

	Slots[SlotIndex].ItemData = NewItemData;
	Slots[SlotIndex].StackCount = SourceInstance.StackCount;
	EnsureValidLastUsedSlot();
	OnQuickSlotChanged.Broadcast(SlotIndex);
	return true;
}

bool UQuickSlotComponent::UnregisterToInventory(int32 SlotIndex, UInventoryGridComponent* TargetInventory)
{
	if (!Slots.IsValidIndex(SlotIndex) || !TargetInventory) return false;

	FQuickSlotEntry& Entry = Slots[SlotIndex];
	if (!Entry.ItemData) return false;

	// 수량만큼 반복 반환
	const int32 CountToReturn = FMath::Max(1, Entry.StackCount);
	UItemDataBase* ItemData = Entry.ItemData;
	int32 Returned = 0;
	for (; Returned < CountToReturn; ++Returned)
	{
		if (!TargetInventory->AddItem(ItemData))
		{
			break;
		}
	}

	if (Returned <= 0) return false;

	if (Returned >= CountToReturn)
	{
		Entry = FQuickSlotEntry();
	}
	else
	{
		// 일부만 반환된 경우 나머지 유지
		Entry.StackCount = CountToReturn - Returned;
	}

	EnsureValidLastUsedSlot();
	OnQuickSlotChanged.Broadcast(SlotIndex);
	return true;
}

bool UQuickSlotComponent::UnregisterToInventoryAt(int32 SlotIndex, UInventoryGridComponent* TargetInventory, const FIntPoint& Position, bool bRotated)
{
	if (!Slots.IsValidIndex(SlotIndex) || !TargetInventory) return false;

	FQuickSlotEntry& Entry = Slots[SlotIndex];
	if (!Entry.ItemData) return false;

	if (!TargetInventory->AddItemAt(Entry.ItemData, Position, bRotated, Entry.StackCount)) return false;

	Entry = FQuickSlotEntry();
	EnsureValidLastUsedSlot();
	OnQuickSlotChanged.Broadcast(SlotIndex);
	return true;
}

bool UQuickSlotComponent::SwapOrMoveSlots(int32 SourceIndex, int32 TargetIndex)
{
	if (!Slots.IsValidIndex(SourceIndex) || !Slots.IsValidIndex(TargetIndex) || SourceIndex == TargetIndex) return false;
	if (!Slots[SourceIndex].ItemData) return false;

	Slots.Swap(SourceIndex, TargetIndex);

	EnsureValidLastUsedSlot();
	OnQuickSlotChanged.Broadcast(SourceIndex);
	OnQuickSlotChanged.Broadcast(TargetIndex);
	return true;
}

bool UQuickSlotComponent::UseQuickSlot(int32 SlotIndex, AProtoCharacter* OwningCharacter)
{
	if (!Slots.IsValidIndex(SlotIndex) || !OwningCharacter) return false;

	FQuickSlotEntry& Entry = Slots[SlotIndex];
	if (!Entry.ItemData) return false;

	UConsumableItemData* ConsumableData = Cast<UConsumableItemData>(Entry.ItemData);
	if (!ConsumableData) return false;

	if (!OwningCharacter->UseConsumable(ConsumableData))
	{
		return false;
	}

	Entry.StackCount -= 1;
	if (Entry.StackCount <= 0)
	{
		Entry = FQuickSlotEntry();
	}

	EnsureValidLastUsedSlot();
	OnQuickSlotChanged.Broadcast(SlotIndex);
	return true;
}

const FQuickSlotEntry& UQuickSlotComponent::GetQuickSlotEntry(int32 SlotIndex) const
{
	static const FQuickSlotEntry EmptyEntry;
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : EmptyEntry;
}

void UQuickSlotComponent::EnsureValidLastUsedSlot()
{
	if (Slots.IsValidIndex(LastUsedSlotIndex) && Slots[LastUsedSlotIndex].ItemData)
	{
		return;
	}

	LastUsedSlotIndex = INDEX_NONE;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].ItemData)
		{
			LastUsedSlotIndex = i;
			break;
		}
	}
}
