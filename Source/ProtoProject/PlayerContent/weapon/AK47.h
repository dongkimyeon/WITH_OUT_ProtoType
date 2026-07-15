#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "AK47.generated.h"

class UArrowComponent;

UCLASS()
class PROTOPROJECT_API AAK47 : public AWeaponBase
{
    GENERATED_BODY()

public:
    AAK47();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UArrowComponent* MuzzlePoint;

    virtual void Fire() override;
};
