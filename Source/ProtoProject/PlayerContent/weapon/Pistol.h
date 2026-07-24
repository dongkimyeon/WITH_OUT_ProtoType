#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "Pistol.generated.h"

class UArrowComponent;

UCLASS()
class PROTOPROJECT_API APistol : public AWeaponBase
{
    GENERATED_BODY()

public:
    APistol();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UArrowComponent* MuzzlePoint;

    virtual void Fire() override;
};