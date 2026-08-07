#pragma once

#include "CoreMinimal.h"
#include "EquipmentItemData.h" // 부모인 장비 클래스 헤더
#include "ArmorItemData.generated.h"

// 방어구가 장착 가능한 슬롯 종류
UENUM(BlueprintType)
enum class EArmorSlot : uint8
{
	Helmet,
	Vest
};

UCLASS(BlueprintType, meta = (PrioritizeCategories = "Item Equipment Armor"))
class PROTOPROJECT_API UArmorItemData : public UEquipmentItemData
{
	GENERATED_BODY()

public:
	// 방어구만의 고유 속성: 피해 감소량
	UPROPERTY(EditDefaultsOnly, Category = "Armor")
	float DamageReduction = 0.f;

	// 이 방어구가 장착될 슬롯 (헬멧/조끼)
	UPROPERTY(EditDefaultsOnly, Category = "Armor")
	EArmorSlot ArmorSlot = EArmorSlot::Helmet;
};