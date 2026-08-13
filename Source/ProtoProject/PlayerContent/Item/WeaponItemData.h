#pragma once

#include "CoreMinimal.h"
#include "EquipmentItemData.h" // 부모 클래스 헤더
#include "WeaponItemData.generated.h"

class AWeaponBase;

UCLASS(BlueprintType, meta = (PrioritizeCategories = "Item Equipment Weapon"))
class PROTOPROJECT_API UWeaponItemData : public UEquipmentItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float BaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float StaminaCostPerSwing = 5.f;   // 근접 공격 시 스태미나 소모량

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	bool bIsHitscan = false;           // 즉시 적중(히트스캔) 여부

	// 인벤토리에서 이 아이템을 장착할 때 캐릭터에 스폰할 무기 액터 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> WeaponActorClass;
};