#include "LootTierRoll.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// 티어별 희귀도 가중치 (인덱스 = EItemTier 값). 지역 티어가 높을수록 상위 티어가
	// "후보에 포함"되지만, 가중치 자체는 하위 티어가 커서 흔한 아이템이 더 자주 나온다.
	// 밸런싱은 이 배열만 수정하면 된다.
	const float GTierWeights[] = { 100.f, 45.f, 18.f, 6.f, 2.f };

	// 프로젝트 내 모든 UItemDataBase 애셋. 한 번만 스캔해 캐시하고, 애셋을 살려두기 위해
	// 강한 참조로 붙든다(데이터 애셋은 가벼워서 상시 로드해도 부담 없음).
	const TArray<TStrongObjectPtr<UItemDataBase>>& GetAllItemData()
	{
		static TArray<TStrongObjectPtr<UItemDataBase>> Cache;
		static bool bBuilt = false;

		if (bBuilt && Cache.Num() > 0)
		{
			return Cache;
		}
		bBuilt = true;
		Cache.Reset();

		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssetsByClass(
			UItemDataBase::StaticClass()->GetClassPathName(), AssetDataList, /*bSearchSubClasses=*/true);

		for (const FAssetData& AssetData : AssetDataList)
		{
			if (UItemDataBase* Item = Cast<UItemDataBase>(AssetData.GetAsset()))
			{
				Cache.Emplace(Item);
			}
		}

		return Cache;
	}
}

UItemDataBase* LootTier::RollItem(EItemTier MaxTier)
{
	const TArray<TStrongObjectPtr<UItemDataBase>>& All = GetAllItemData();

	TArray<UItemDataBase*> Candidates;
	TArray<float> Weights;
	float TotalWeight = 0.f;

	for (const TStrongObjectPtr<UItemDataBase>& Ptr : All)
	{
		UItemDataBase* Item = Ptr.Get();
		if (!Item || Item->Tier > MaxTier)
		{
			continue;
		}

		const uint8 TierIdx = static_cast<uint8>(Item->Tier);
		const float Weight = (TierIdx < UE_ARRAY_COUNT(GTierWeights)) ? GTierWeights[TierIdx] : 1.f;

		Candidates.Add(Item);
		Weights.Add(Weight);
		TotalWeight += Weight;
	}

	if (Candidates.Num() == 0 || TotalWeight <= 0.f)
	{
		return nullptr;
	}

	float Pick = FMath::FRand() * TotalWeight;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Pick -= Weights[i];
		if (Pick <= 0.f)
		{
			return Candidates[i];
		}
	}
	return Candidates.Last();
}
