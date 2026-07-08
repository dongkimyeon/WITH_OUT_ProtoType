#include "AK47.h"

AAK47::AAK47()
{
    PrimaryActorTick.bCanEverTick = true;

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    UE_LOG(LogTemp, Warning, TEXT("AAK47 Constructor"));
}

void AAK47::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("AAK47 BeginPlay"));
}

void AAK47::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
