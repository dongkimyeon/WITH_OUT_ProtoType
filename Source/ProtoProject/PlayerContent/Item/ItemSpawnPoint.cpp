#include "ItemSpawnPoint.h"
#include "DropItem.h"
#include "LootTierRoll.h"
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameInstance.h"

namespace
{
	// Same lookup ALootContainer::ResolveItemDataByAssetName does (asset
	// name -> UItemDataBase*), duplicated here with its own cache rather
	// than shared: same reasoning as that copy's own comment -- this class
	// has no relationship to it either, and the lookup itself has no
	// per-instance state.
	// Named distinctly from LootContainer.cpp's copy (not just "anonymous
	// namespace") because Unity Build can merge both .cpp files into one
	// translation unit, where an unnamed namespace no longer gives each
	// file its own scope -- two identically-named functions there collide
	// as a redefinition (C2084) instead of quietly staying file-local.
	UItemDataBase* ResolveItemDataByAssetNameForSpawnPoint(const FString& AssetName)
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

AItemSpawnPoint::AItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;

	RangeViz = CreateDefaultSubobject<USphereComponent>(TEXT("RangeViz"));
	RangeViz->SetupAttachment(RootComponent);
	RangeViz->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RangeViz->SetCanEverAffectNavigation(false);
	RangeViz->SetHiddenInGame(true);
	RangeViz->ShapeColor = FColor(255, 200, 0);
	RangeViz->SetSphereRadius(ScatterRadius);
}

void AItemSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 ScatterRadius를 바꾸면 시각화 구도 즉시 따라가게 한다.
	RangeViz->SetSphereRadius(FMath::Max(ScatterRadius, 1.f));
}

void AItemSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnLoot();
	}
}

void AItemSpawnPoint::SpawnLoot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 Count = FMath::RandRange(FMath::Min(MinItems, MaxItems), FMath::Max(MinItems, MaxItems));
	for (int32 i = 0; i < Count; ++i)
	{
		UItemDataBase* Chosen = LootTier::RollItem(RegionTier);
		if (!Chosen)
		{
			continue;
		}

		FVector Offset = FVector(FMath::VRand().GetSafeNormal2D() * FMath::FRand() * ScatterRadius);
		Offset.Z = 30.f;

		// 스폰 포인트 액터의 스케일이 아이템에 새어들지 않도록 위치/회전만 쓰고 스케일은 1로 고정.
		const FTransform SpawnTransform(GetActorQuat(), GetActorLocation() + Offset, FVector(1.f));

		// ItemData는 OnConstruction이 메시를 붙이는 데 쓰이므로 Deferred 스폰으로 먼저 세팅한다.
		// (AEnemyBase::SpawnLoot와 동일한 패턴)
		ADropItem* Drop = World->SpawnActorDeferred<ADropItem>(ADropItem::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Drop)
		{
			continue;
		}

		Drop->ItemData = Chosen;
		Drop->StackCount = 1;
		Drop->NetSlotId = GetTypeHash(GetName() + FString::FromInt(i));
		Drop->FinishSpawning(SpawnTransform);
		SpawnedDrops.Add(Drop);
	}

	// Report this local roll to the server so every client ends up seeing
	// the same scattered drops (see this class's header comment / the
	// identical ALootContainer::SeedContents pattern). No-op if not
	// connected -- these stay whatever they rolled locally, same as before
	// this feature existed.
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
	if (!NetClient || !NetClient->IsConnected())
	{
		return;
	}

	TArray<FProtoWorldItemEntry> LocalRoll;
	LocalRoll.Reserve(SpawnedDrops.Num());
	for (const ADropItem* Drop : SpawnedDrops)
	{
		if (!Drop || !Drop->ItemData) continue;

		FProtoWorldItemEntry Entry;
		Entry.ItemId = FName(*Drop->ItemData->GetName());
		Entry.Position = Drop->GetActorLocation();
		Entry.StackCount = Drop->StackCount;
		LocalRoll.Add(Entry);
	}

	NetClient->OnItemSpawnState.AddDynamic(this, &AItemSpawnPoint::HandleItemSpawnState);
	NetClient->SendItemSpawnRoll(GetSpawnPointId(), LocalRoll);
}

int32 AItemSpawnPoint::GetSpawnPointId() const
{
	return static_cast<int32>(GetTypeHash(GetName()));
}

void AItemSpawnPoint::HandleItemSpawnState(int32 SpawnPointId, const TArray<FProtoWorldItemEntry>& Items)
{
	if (SpawnPointId != GetSpawnPointId())
	{
		// Not our reply -- every spawn point in the level shares this one
		// broadcast delegate, so most calls are for somewhere else.
		return;
	}

	// Got our answer; no need to keep listening for other spawn points' replies.
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->OnItemSpawnState.RemoveDynamic(this, &AItemSpawnPoint::HandleItemSpawnState);
		}
	}

	// Replace whatever was spawned locally with the authoritative set --
	// either this instance's own roll (if it was first) or an earlier
	// client's.
	for (ADropItem* Drop : SpawnedDrops)
	{
		if (IsValid(Drop))
		{
			Drop->Destroy();
		}
	}
	SpawnedDrops.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FProtoWorldItemEntry& Entry = Items[Index];
		UItemDataBase* ItemData = ResolveItemDataByAssetNameForSpawnPoint(Entry.ItemId.ToString());
		if (!ItemData)
		{
			UE_LOG(LogTemp, Warning, TEXT("AItemSpawnPoint::HandleItemSpawnState: couldn't resolve item asset '%s', skipping"), *Entry.ItemId.ToString());
			continue;
		}

		const FTransform SpawnTransform(GetActorQuat(), Entry.Position, FVector(1.f));
		ADropItem* Drop = World->SpawnActorDeferred<ADropItem>(ADropItem::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Drop)
		{
			continue;
		}

		Drop->ItemData = ItemData;
		Drop->StackCount = Entry.StackCount;
		// Same (spawn point name + index) formula SpawnLoot() uses for its
		// own local pre-authoritative roll -- Index here is this item's
		// position in the AUTHORITATIVE Items list (identical on every
		// client, since it's the one thing the server just arbitrated),
		// so this is what actually gives every client's copy of "the same"
		// item a matching NetSlotId. See RequestPickup's comment for why
		// that has to be true for pickup sync to work at all.
		Drop->NetSlotId = GetTypeHash(GetName() + FString::FromInt(Index));
		Drop->FinishSpawning(SpawnTransform);
		SpawnedDrops.Add(Drop);
	}
}
