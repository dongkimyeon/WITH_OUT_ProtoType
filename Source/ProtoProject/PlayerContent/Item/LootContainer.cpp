#include "LootContainer.h"
#include "ItemDataBase.h"
#include "../Inventory/InventoryGridComponent.h"
#include "LootTierRoll.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameInstance.h"

namespace
{

	UItemDataBase* ResolveItemDataByAssetName(const FString& AssetName)
	{
		static TMap<FString, TWeakObjectPtr<UItemDataBase>> Cache;

		if (const TWeakObjectPtr<UItemDataBase>* Cached = Cache.Find(AssetName))
		{
			if (Cached->IsValid())
			{
				return Cached->Get();
			}
			Cache.Remove(AssetName);
		}

		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// See AProtoCharacter::ResolveItemDataByName's comment: force the
		// background discovery scan to finish if it hasn't yet, instead of
		// risking a silent "couldn't resolve" on every item because
		// GetAssetsByClass below only saw a partial scan.
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch=*/true);
		}

		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssetsByClass(UItemDataBase::StaticClass()->GetClassPathName(), AssetDataList, /*bSearchSubClasses=*/true);

		for (const FAssetData& AssetData : AssetDataList)
		{
			if (AssetData.AssetName.ToString() != AssetName)
			{
				continue;
			}

			if (UItemDataBase* Resolved = Cast<UItemDataBase>(AssetData.GetAsset()))
			{
				Cache.Add(AssetName, Resolved);
				return Resolved;
			}
		}

		return nullptr;
	}
}

ALootContainer::ALootContainer()
{
	ContainerName = FText::FromString(TEXT("루팅 상자"));
}

void ALootContainer::SeedContents()
{
	// LootTable을 비워두면 RegionTier 기준으로 자동 채운다.
	if (LootTable.Num() == 0)
	{
		const int32 Count = FMath::RandRange(FMath::Min(MinItems, MaxItems), FMath::Max(MinItems, MaxItems));
		for (int32 i = 0; i < Count; ++i)
		{
			if (UItemDataBase* Chosen = LootTier::RollItem(RegionTier))
			{
				ContainerInventory->AddItem(Chosen);
			}
		}
	}

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

	// Report this local roll to the server so every client ends up agreeing
	// on the same contents (see this class's header comment). No-op if not
	// connected -- the container just keeps whatever it rolled locally,
	// same as before this feature existed.
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
	if (!NetClient || !NetClient->IsConnected())
	{
		return;
	}

	TArray<FProtoInventoryItemEntry> LocalRoll;
	LocalRoll.Reserve(ContainerInventory->Items.Num());
	for (const FInventoryItemInstance& Item : ContainerInventory->Items)
	{
		if (!Item.ItemData) continue;

		FProtoInventoryItemEntry Entry;
		Entry.ItemId = FName(*Item.ItemData->GetName());
		Entry.GridX = Item.GridPosition.X;
		Entry.GridY = Item.GridPosition.Y;
		Entry.bRotated = Item.bIsRotated;
		Entry.StackCount = Item.StackCount;
		LocalRoll.Add(Entry);
	}

	NetClient->OnContainerLootState.AddDynamic(this, &ALootContainer::HandleContainerLootState);
	NetClient->SendContainerLootRoll(GetContainerId(), LocalRoll);
}

void ALootContainer::HandleContainerLootState(int32 ContainerId, const TArray<FProtoInventoryItemEntry>& Items)
{
	if (ContainerId != GetContainerId())
	{
		// Not our reply -- every container in the level shares this one
		// broadcast delegate, so most calls are for someone else's box.
		return;
	}

	// Got our answer; no need to keep listening for other containers' replies.
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->OnContainerLootState.RemoveDynamic(this, &ALootContainer::HandleContainerLootState);
		}
	}

	ContainerInventory->Items.Empty();
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FProtoInventoryItemEntry& Entry = Items[Index];
		UItemDataBase* ItemData = ResolveItemDataByAssetName(Entry.ItemId.ToString());
		if (!ItemData)
		{
			UE_LOG(LogTemp, Warning, TEXT("ALootContainer::HandleContainerLootState: couldn't resolve item asset '%s', skipping"), *Entry.ItemId.ToString());
			continue;
		}

		if (ContainerInventory->AddItemAt(ItemData, FIntPoint(Entry.GridX, Entry.GridY), Entry.bRotated, Entry.StackCount))
		{
			// Same (container name + index-in-authoritative-list) formula
			// ADropItem/AItemSpawnPoint use -- Index here is this item's
			// position in the AUTHORITATIVE Items list (identical on every
			// client), so this is what lets every client's copy of "the
			// same" box item agree on a NetSlotId for pickup arbitration
			// (see FInventoryItemInstance::NetSlotId's comment). AddItemAt
			// always appends, so the instance we just added is Items.Last().
			ContainerInventory->Items.Last().NetSlotId = GetTypeHash(GetName() + FString::FromInt(Index));
		}
	}
}
