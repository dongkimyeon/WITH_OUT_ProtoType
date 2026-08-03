#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../ProtoCharacter.h"
#include "WeaponBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInterface;

UCLASS()
class PROTOPROJECT_API AWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    AWeaponBase();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* CollisionBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    EWeaponType WeaponType = EWeaponType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Swap", meta = (ClampMin = "0.0"))
    float EquipSwapTime = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Swap", meta = (ClampMin = "0.0"))
    float UnequipSwapTime = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Fire", meta = (ClampMin = "0.0"))
    float FireRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Fire")
    bool bAutomatic = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Impact")
    UMaterialInterface* BulletHoleDecalMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Impact")
    FVector BulletHoleDecalSize = FVector(1.0f, 10.0f, 10.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Impact", meta = (ClampMin = "0.0"))
    float BulletHoleDecalLifeSpan = 0.0f;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void Fire();

    UFUNCTION(BlueprintCallable, Category = "Weapon|IK")
    virtual bool GetLeftHandSocketTransform(FTransform& OutTransform) const;

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    virtual void ReloadAmmoAttach(USkeletalMeshComponent* CharacterMesh);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    virtual void ReloadAmmoDetach();

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    virtual void ReloadNewAmmo(USkeletalMeshComponent* CharacterMesh);

    UFUNCTION(BlueprintCallable, Category = "Weapon|Reload")
    virtual void ReloadNewAmmoAttach();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    virtual void OnWeaponBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    virtual void EquipToCharacter(AProtoCharacter* Character);

public:
    virtual void Tick(float DeltaTime) override;
};
