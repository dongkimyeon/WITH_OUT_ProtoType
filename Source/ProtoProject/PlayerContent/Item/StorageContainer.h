#pragma once

#include "CoreMinimal.h"
#include "ItemContainerBase.h"
#include "StorageContainer.generated.h"

// 플레이어가 아이템을 직접 넣고 빼는 개인 보관함(안전지대 창고). 초기값 없이 항상 빈 채로 시작한다.
UCLASS()
class PROTOPROJECT_API AStorageContainer : public AItemContainerBase
{
	GENERATED_BODY()

public:
	AStorageContainer();
};
