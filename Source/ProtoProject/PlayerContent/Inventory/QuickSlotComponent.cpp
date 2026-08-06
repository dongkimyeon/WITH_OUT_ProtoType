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
	OnQuickSlotChanged.Broadcast(SlotIndex);
	return true;
}

bool UQuickSlotComponent::UnregisterToInventory(int32 SlotIndex, UInventoryGridComponent* TargetInventory)
{
	if (!Slots.IsValidIndex(SlotIndex) || !TargetInventory) return false;
	if (!Slots[SlotIndex].ItemData) return false;

	if (!TargetInventory->AddItem(Slots[SlotIndex].ItemData)) return false;

	Slots[SlotIndex] = FQuickSlotEntry();
	OnQuickSlotChanged.Broadcast(SlotIndex);
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

	OnQuickSlotChanged.Broadcast(SlotIndex);
	return true;
}

const FQuickSlotEntry& UQuickSlotComponent::GetQuickSlotEntry(int32 SlotIndex) const
{
	static const FQuickSlotEntry EmptyEntry;
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : EmptyEntry;
}
