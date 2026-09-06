#include "StorageContainer.h"
#include "ItemDataBase.h"
#include "../Inventory/InventoryGridComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameInstance.h"

namespace
{
	// 애셋 이름 -> UItemDataBase* 조회. ALootContainer/AItemSpawnPoint/
	// AProtoCharacter가 각자 들고 있는 것과 같은 로직의 파일 로컬 복사본(서로
	// 관계 없는 클래스들이라 공유하지 않는다는 그쪽 주석과 동일한 이유).
	UItemDataBase* ResolveItemDataByAssetNameForStash(const FString& AssetName)
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
		// background discovery scan to finish if it hasn't yet.
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

AStorageContainer::AStorageContainer()
{
	ContainerName = FText::FromString(TEXT("창고"));
}

void AStorageContainer::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
	if (!NetClient || !NetClient->IsConnected())
	{
		// 오프라인/미접속: 서버에 저장할 방법이 없으니 예전처럼 빈 채로 시작해서
		// 로컬 전용으로 동작한다(이번 세션 동안만 유지, 재접속 시 사라짐).
		return;
	}

	NetClient->OnStashState.AddDynamic(this, &AStorageContainer::HandleStashState);
	NetClient->SendRequestStash();

	// 위 SendRequestStash 응답(HandleStashState)이 오기 전까지, 그리고 그 이후로도
	// 계속 -- 넣기/빼기/이동이 생길 때마다 즉시 저장한다.
	ContainerInventory->OnInventoryChanged.AddDynamic(this, &AStorageContainer::HandleStashChanged);
}

void AStorageContainer::HandleStashState(const TArray<FProtoInventoryItemEntry>& Items)
{
	// 계정당 유니캐스트라 우리 응답이 맞다 -- 한 번만 받으면 되니 더 들을 필요 없음.
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->OnStashState.RemoveDynamic(this, &AStorageContainer::HandleStashState);
		}
	}

	bIsRestoringStash = true;

	ContainerInventory->Items.Empty();
	for (const FProtoInventoryItemEntry& Entry : Items)
	{
		UItemDataBase* ItemData = ResolveItemDataByAssetNameForStash(Entry.ItemId.ToString());
		if (!ItemData)
		{
			UE_LOG(LogTemp, Warning, TEXT("AStorageContainer::HandleStashState: couldn't resolve item asset '%s', skipping"), *Entry.ItemId.ToString());
			continue;
		}

		ContainerInventory->AddItemAt(ItemData, FIntPoint(Entry.GridX, Entry.GridY), Entry.bRotated, Entry.StackCount);
	}

	bIsRestoringStash = false;
}

void AStorageContainer::HandleStashChanged()
{
	if (bIsRestoringStash)
	{
		// HandleStashState가 채우는 중 -- 아직 정착되지 않은 중간 상태라 저장할
		// 가치가 없다. 어차피 서버에서 막 받아온 것과 같은 내용이라 재저장은
		// 낭비이기도 하다.
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
	if (!NetClient)
	{
		return;
	}

	TArray<FProtoInventoryItemEntry> Snapshot;
	Snapshot.Reserve(ContainerInventory->Items.Num());
	for (const FInventoryItemInstance& Item : ContainerInventory->Items)
	{
		if (!Item.ItemData)
		{
			continue;
		}

		FProtoInventoryItemEntry Entry;
		Entry.ItemId = FName(*Item.ItemData->GetName());
		Entry.GridX = Item.GridPosition.X;
		Entry.GridY = Item.GridPosition.Y;
		Entry.bRotated = Item.bIsRotated;
		Entry.StackCount = Item.StackCount;
		Snapshot.Add(Entry);
	}

	NetClient->SendSaveStash(Snapshot);
}
