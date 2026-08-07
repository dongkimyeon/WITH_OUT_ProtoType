#pragma once

#include "CoreMinimal.h"
#include "ItemDataBase.h"
#include "EquipmentItemData.generated.h"

UCLASS(Abstract, BlueprintType, meta = (PrioritizeCategories = "Item Equipment"))
class PROTOPROJECT_API UEquipmentItemData : public UItemDataBase
{
	GENERATED_BODY()

public:
	UEquipmentItemData() 
	{ 
		Category = EItemCategory::Equipment; 
	}

	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	int32 Durability = 100;

	virtual EItemContextAction GetContextAction() const override { return EItemContextAction::Equip; }
};