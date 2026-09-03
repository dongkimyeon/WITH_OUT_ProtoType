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

    virtual void Tick(float DeltaTime) override;

    virtual bool CanCall() const override;

    // 주변 CallRadius 안의 타겟 없는 살아있는 좀비들을 자신과 같은 타겟으로 즉시 반응시킨다.
    virtual void DoCall() override;

protected:
    // CallRadius를 매 틱 디버그 스피어로 그린다(bEnableBehaviorDebug 켜져 있을 때만).
    // 쿨다운 준비되면 초록, 쿨다운 중이면 빨강.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Debug")
    bool bShowCallRadius = true;

    // 이 반경 안의 다른 살아있는 좀비를 즉시 같은 타겟으로 반응시킨다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    float CallRadius = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    float CallCooldown = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    TObjectPtr<UAnimMontage> CallMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Call")
    FName CallSectionName = TEXT("Call");

private:
    float LastCallTime = -999.0f;
};


