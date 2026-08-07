#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "Pistol.generated.h"

class UArrowComponent;
class USkeletalMeshComponent;

UCLASS()
class PROTOPROJECT_API APistol : public AWeaponBase
{
    GENERATED_BODY()

public:
    APistol();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UArrowComponent* MuzzlePoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    USkeletalMeshComponent* PistolSkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    TSubclassOf<AActor> PistolAmmoClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    FName MagazineBoneName = TEXT("b_gun_mag");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    FName AmmoHandSocketName = TEXT("HandGrip_L");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    FTransform HandAmmoRelativeTransform = FTransform::Identity;

    virtual void Fire() override;
    virtual bool GetLeftHandSocketTransform(FTransform& OutTransform) const override;
    virtual void ReloadAmmoDetach() override;
    virtual void ReloadNewAmmo(USkeletalMeshComponent* CharacterMesh) override;
    virtual void ReloadNewAmmoAttach() override;

private:
    UPROPERTY()
    AActor* HandAmmoActor = nullptr;

    USkeletalMeshComponent* GetPistolSkeletalMesh() const;
    AActor* SpawnAmmoInHand(USkeletalMeshComponent* CharacterMesh);
    AActor* SpawnAmmoAtMagazine();
    void SetWeaponMagazineHidden(bool bShouldHide);
};