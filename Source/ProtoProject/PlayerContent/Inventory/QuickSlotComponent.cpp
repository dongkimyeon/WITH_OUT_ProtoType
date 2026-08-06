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

	FQuickSlotEntry PrevEntry = Slots[SlotIndex];

	SourceInventory->RemoveInstanceById(InstanceId);

	if (PrevEntry.ItemData)
	{
		if (!SourceInventory->AddItem(PrevEntry.ItemData))
		{
			// 기존 등록 아이템을 되돌릴 공간이 없으면 등록 실패 - 새 아이템을 원래 위치로 롤백
			SourceInventory->AddItemAt(NewItemData, SourceInstance.GridPosition, SourceInstance.bIsRotated);
			return false;
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

	// AddItem은 한 번에 1개씩만 추가하므로(스택은 내부적으로 병합), 등록된 수량만큼 반복 호출해야 수량이 보존된다.
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
		// 인벤토리 공간이 모자라 일부만 들어간 경우 - 못 옮긴 나머지는 슬롯에 그대로 유지한다.
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

	OwningCharacter->UseConsumable(ConsumableData);

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
