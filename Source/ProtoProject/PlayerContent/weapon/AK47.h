#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AK47.generated.h"

class UBoxComponent;

UCLASS()
class PROTOPROJECT_API AAK47 : public AActor
{
    GENERATED_BODY()

public:
    AAK47();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* CollisionBox;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnWeaponBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

public:
    virtual void Tick(float DeltaTime) override;
};
