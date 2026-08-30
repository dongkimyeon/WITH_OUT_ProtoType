#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "EnemyCaller.generated.h"

class UAnimMontage;

UCLASS()
class PROTOPROJECT_API AEnemyCaller : public AEnemyBase
{
    GENERATED_BODY()

public:
    AEnemyCaller();

    virtual bool CanCall() const override;

    // 주변 CallRadius 안의 타겟 없는 살아있는 좀비들을 자신과 같은 타겟으로 즉시 반응시킨다.
    virtual void DoCall() override;

protected:
    // 이 반경 안의 다른 살아있는 좀비를 즉시 같은 타겟으로 반응시킨다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    float CallRadius = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    float CallCooldown = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    TObjectPtr<UAnimMontage> CallMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    FName CallSectionName = TEXT("Call");

private:
    float LastCallTime = -999.0f;
};


