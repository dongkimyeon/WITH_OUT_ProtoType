#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "AK47.generated.h"

class UArrowComponent;
class USkeletalMeshComponent;
class USoundBase;
class UParticleSystem;

UCLASS()
class PROTOPROJECT_API AAK47 : public AWeaponBase
{
    GENERATED_BODY()

public:
    AAK47();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UArrowComponent* MuzzlePoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    USkeletalMeshComponent* RifleSkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    TSubclassOf<AActor> RifleAmmoClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    FName MagazineBoneName = TEXT("b_gun_mag");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    FName AmmoHandSocketName = TEXT("HandGrip_L");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
    FTransform HandAmmoRelativeTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sound")
    TArray<USoundBase*> RifleFireSounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sound", meta = (ClampMin = "0.0"))
    float FireSoundVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sound", meta = (ClampMin = "0.0"))
    float FireSoundPitch = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sound")
    USoundBase* ReloadSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sound", meta = (ClampMin = "0.0"))
    float ReloadSoundVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sound", meta = (ClampMin = "0.0"))
    float ReloadSoundPitch = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Sound", meta = (ClampMin = "0.0"))
    float ReloadSoundStartTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|VFX")
    UParticleSystem* BloodHitEffect = nullptr;

    virtual void Fire() override;
    virtual bool GetLeftHandSocketTransform(FTransform& OutTransform) const override;
    virtual void ReloadAmmoAttach(USkeletalMeshComponent* CharacterMesh) override;
    virtual void ReloadAmmoDetach() override;
    virtual void ReloadNewAmmo(USkeletalMeshComponent* CharacterMesh) override;
    virtual void ReloadNewAmmoAttach() override;
    virtual void PlayReloadSound() override;

private:
    UPROPERTY()
    AActor* HandAmmoActor = nullptr;

    AActor* SpawnAmmoInHand(USkeletalMeshComponent* CharacterMesh);
    void SetWeaponMagazineHidden(bool bShouldHide);
};
