#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../ProtoCharacter.h"
#include "WeaponBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

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

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void Fire();

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
