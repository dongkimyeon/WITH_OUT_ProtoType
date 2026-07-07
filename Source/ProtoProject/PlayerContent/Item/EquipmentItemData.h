#pragma once

#include "CoreMinimal.h"
#include "ItemDataBase.h"
#include "EquipmentItemData.generated.h"

UCLASS(Abstract, BlueprintType)
class PROTOPROJECT_API UEquipmentItemData : public UItemDataBase
{
	GENERATED_BODY()

public:
	UEquipmentItemData() 
	{ 
		Category = EItemCategory::Equipment; 
	}

	// 장비의 공통 속성: 내구도
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	int32 Durability = 100;
};