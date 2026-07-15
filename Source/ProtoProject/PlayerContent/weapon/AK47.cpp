#include "AK47.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

AAK47::AAK47()
{
    WeaponType = EWeaponType::Rifle;

    MuzzlePoint = CreateDefaultSubobject<UArrowComponent>(TEXT("MuzzlePoint"));
    MuzzlePoint->SetupAttachment(WeaponMesh);
}

void AAK47::Fire()
{
    if (!GetWorld() || !MuzzlePoint)
    {
        return;
    }

    const FVector Start = MuzzlePoint->GetComponentLocation();
    const FVector End = Start + MuzzlePoint->GetForwardVector() * 10000.0f;

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AK47Fire), false, this);
    Params.AddIgnoredActor(this);
    if (AActor* OwnerActor = GetOwner())
    {
        Params.AddIgnoredActor(OwnerActor);
    }
    if (AActor* AttachParent = GetAttachParentActor())
    {
        Params.AddIgnoredActor(AttachParent);
    }

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        Params);

    DrawDebugLine(
        GetWorld(),
        Start,
        bHit ? Hit.ImpactPoint : End,
        bHit ? FColor::Red : FColor::Green,
        false,
        1.0f,
        0,
        1.5f);

    if (bHit && Hit.GetActor())
    {
        UE_LOG(LogTemp, Warning, TEXT("AK47 hit: %s"), *Hit.GetActor()->GetName());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red,
                FString::Printf(TEXT("Hit: %s"), *Hit.GetActor()->GetName()));
        }
    }
}
