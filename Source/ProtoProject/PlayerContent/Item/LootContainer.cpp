#include "LootContainer.h"
#include "ItemDataBase.h"
#include "../Inventory/InventoryGridComponent.h"

ALootContainer::ALootContainer()
{
	ContainerName = FText::FromString(TEXT("루팅 상자"));
}

void ALootContainer::SeedContents()
{
	for (const FLootTableEntry& Entry : LootTable)
	{
		if (!Entry.Item) continue;
		if (FMath::FRand() > Entry.DropChance) continue;

		const int32 Count = FMath::RandRange(Entry.MinStackCount, Entry.MaxStackCount);
		for (int32 i = 0; i < Count; ++i)
		{
			ContainerInventory->AddItem(Entry.Item);
		}
	}
}
