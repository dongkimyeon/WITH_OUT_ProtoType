#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AK47.generated.h"

UCLASS()
class PROTOPROJECT_API AAK47 : public AActor
{
    GENERATED_BODY()

public:
    AAK47();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStaticMeshComponent* WeaponMesh;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
};
