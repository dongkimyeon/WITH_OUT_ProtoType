#pragma once

#include "CoreMinimal.h"
#include "ItemDataBase.h" // 부모 클래스 헤더 반드시 포함
#include "ConsumableItemData.generated.h"

// 1. 무엇을 회복시킬 것인가?
UENUM(BlueprintType)
enum class EConsumableTargetStat : uint8
{
    Health,
    Hunger,
    Thirst,
    Infection
};

// 2. 어떻게 회복시킬 것인가? (즉시, 서서히, 둘 다)
UENUM(BlueprintType)
enum class EEffectApplication : uint8
{
    Instant,        
    OverTime,        
    InstantThenOverTime 
};

// 3. 소모품 사용 시 발생할 수 있는 부작용 (예: 더러운 물을 마시면 갈증은 차지만 감염도 증가)
USTRUCT(BlueprintType)
struct FConsumableSideEffect
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Side Effect")
    bool bAppliesDebuff = false;

    UPROPERTY(EditDefaultsOnly, Category = "Side Effect", meta = (EditCondition = "bAppliesDebuff"))
    float InfectionIncrease = 0.f;   

    UPROPERTY(EditDefaultsOnly, Category = "Side Effect", meta = (EditCondition = "bAppliesDebuff"))
    float DebuffDurationSeconds = 0.f;
};


// 4. 소모품 데이터 클래스 본체
UCLASS(BlueprintType, meta = (PrioritizeCategories = "Item Consumable"))
class PROTOPROJECT_API UConsumableItemData : public UItemDataBase
{
    GENERATED_BODY()

public:
    UConsumableItemData() 
    { 
        Category = EItemCategory::Consumable; 
    }

    UPROPERTY(EditDefaultsOnly, Category = "Consumable")
    EConsumableTargetStat TargetStat = EConsumableTargetStat::Health;

    UPROPERTY(EditDefaultsOnly, Category = "Consumable")
    EEffectApplication Application = EEffectApplication::Instant;

    UPROPERTY(EditDefaultsOnly, Category = "Consumable")
    float InstantAmount = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "Consumable")
    float OverTimeAmount = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "Consumable")
    float OverTimeDurationSeconds = 0.f;

    // 자가제세동기 같은 특수 부활 아이템인지 체크
    UPROPERTY(EditDefaultsOnly, Category = "Consumable")
    bool bIsDefibrillator = false;   

    UPROPERTY(EditDefaultsOnly, Category = "Consumable")
    FConsumableSideEffect SideEffect;

    virtual EItemContextAction GetContextAction() const override { return EItemContextAction::Use; }
};