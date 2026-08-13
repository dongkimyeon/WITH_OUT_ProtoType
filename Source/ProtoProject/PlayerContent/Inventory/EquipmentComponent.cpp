
#include "EquipmentComponent.h"
#include "InventoryGridComponent.h"
#include "ArmorItemData.h"
#include "WeaponItemData.h"

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    EquippedSlots.SetNum(4);
}

bool UEquipmentComponent::CanEquipToSlot(UItemDataBase* ItemData, EEquipmentSlot Slot) const
{
    if (!ItemData) return false;

    switch (Slot)
    {
    case EEquipmentSlot::Helmet:
    {
        const UArmorItemData* Armor = Cast<UArmorItemData>(ItemData);
        return Armor && Armor->ArmorSlot == EArmorSlot::Helmet;
    }
    case EEquipmentSlot::Vest:
    {
        const UArmorItemData* Armor = Cast<UArmorItemData>(ItemData);
        return Armor && Armor->ArmorSlot == EArmorSlot::Vest;
    }
    case EEquipmentSlot::Weapon1:
    case EEquipmentSlot::Weapon2:
        return ItemData->IsA<UWeaponItemData>();
    default:
        return false;
    }
}

bool UEquipmentComponent::ResolveTargetSlot(UItemDataBase* ItemData, EEquipmentSlot& OutSlot) const
{
    if (!ItemData) return false;

    if (const UArmorItemData* Armor = Cast<UArmorItemData>(ItemData))
    {
        OutSlot = (Armor->ArmorSlot == EArmorSlot::Helmet) ? EEquipmentSlot::Helmet : EEquipmentSlot::Vest;
        return true;
    }

    if (ItemData->IsA<UWeaponItemData>())
    {
        const int32 Weapon1Index = static_cast<int32>(EEquipmentSlot::Weapon1);
        OutSlot = EquippedSlots[Weapon1Index].ItemData ? EEquipmentSlot::Weapon2 : EEquipmentSlot::Weapon1;
        return true;
    }

    return false;
}

bool UEquipmentComponent::EquipFromInventory(UInventoryGridComponent* SourceInventory, const FGuid& InstanceId, EEquipmentSlot Slot)
{
    if (!SourceInventory) return false;

    FInventoryItemInstance SourceInstance;
    if (!SourceInventory->FindInstanceById(InstanceId, SourceInstance)) return false;

    UItemDataBase* NewItemData = SourceInstance.ItemData;
    if (!CanEquipToSlot(NewItemData, Slot)) return false;

    const int32 SlotIndex = static_cast<int32>(Slot);
    FEquippedItem PrevEquipped = EquippedSlots[SlotIndex];

    SourceInventory->RemoveInstanceById(InstanceId);

    if (PrevEquipped.ItemData)
    {
        if (!SourceInventory->AddItem(PrevEquipped.ItemData))
        {
            // 롤백
            SourceInventory->AddItemAt(NewItemData, SourceInstance.GridPosition, SourceInstance.bIsRotated, SourceInstance.StackCount);
            return false;
        }
    }

    EquippedSlots[SlotIndex].ItemData = NewItemData;
    EquippedSlots[SlotIndex].SourceInstanceId = InstanceId;
    OnEquipmentChanged.Broadcast(Slot);
    return true;
}

bool UEquipmentComponent::UnequipToInventory(UInventoryGridComponent* TargetInventory, EEquipmentSlot Slot)
{
    if (!TargetInventory) return false;

    const int32 SlotIndex = static_cast<int32>(Slot);
    if (!EquippedSlots[SlotIndex].ItemData) return false;

    if (!TargetInventory->AddItem(EquippedSlots[SlotIndex].ItemData)) return false;

    EquippedSlots[SlotIndex] = FEquippedItem();
    OnEquipmentChanged.Broadcast(Slot);
    return true;
}

bool UEquipmentComponent::UnequipToInventoryAt(UInventoryGridComponent* TargetInventory, EEquipmentSlot Slot, const FIntPoint& Position, bool bRotated)
{
    if (!TargetInventory) return false;

    const int32 SlotIndex = static_cast<int32>(Slot);
    if (!EquippedSlots[SlotIndex].ItemData) return false;

    if (!TargetInventory->AddItemAt(EquippedSlots[SlotIndex].ItemData, Position, bRotated)) return false;

    EquippedSlots[SlotIndex] = FEquippedItem();
    OnEquipmentChanged.Broadcast(Slot);
    return true;
}

bool UEquipmentComponent::SwapOrMoveSlots(EEquipmentSlot SourceSlot, EEquipmentSlot TargetSlot)
{
    if (SourceSlot == TargetSlot) return false;

    FEquippedItem& Source = EquippedSlots[static_cast<int32>(SourceSlot)];
    FEquippedItem& Target = EquippedSlots[static_cast<int32>(TargetSlot)];

    if (!Source.ItemData) return false;
    if (!CanEquipToSlot(Source.ItemData, TargetSlot)) return false;
    if (Target.ItemData && !CanEquipToSlot(Target.ItemData, SourceSlot)) return false;

    Swap(Source, Target);

    OnEquipmentChanged.Broadcast(SourceSlot);
    OnEquipmentChanged.Broadcast(TargetSlot);
    return true;
}

const FEquippedItem& UEquipmentComponent::GetEquippedItem(EEquipmentSlot Slot) const
{
    return EquippedSlots[static_cast<int32>(Slot)];
}
