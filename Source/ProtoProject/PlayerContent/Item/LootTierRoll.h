#pragma once

#include "CoreMinimal.h"
#include "ItemDataBase.h"

// 지역 티어 기반으로 월드에 뿌릴 아이템을 뽑는 헬퍼.
// 프로젝트의 모든 UItemDataBase 애셋을 자동으로 후보 풀로 삼는다(수동 목록 불필요).
namespace LootTier
{
	// Tier <= MaxTier인 UItemDataBase 애셋 중 티어별 희귀도 가중치로 하나를 뽑는다.
	// 후보가 없으면 nullptr.
	PROTOPROJECT_API UItemDataBase* RollItem(EItemTier MaxTier);
}
