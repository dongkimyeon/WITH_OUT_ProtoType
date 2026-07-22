#include "AK47.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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

    constexpr float TraceRange = 10000.0f;

    AActor* WeaponOwner = GetOwner();
    if (!WeaponOwner)
    {
        WeaponOwner = GetAttachParentActor();
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(AK47Fire), false, this);
    Params.AddIgnoredActor(this);
    if (WeaponOwner)
    {
        Params.AddIgnoredActor(WeaponOwner);
    }
    if (AActor* AttachParent = GetAttachParentActor())
    {
        Params.AddIgnoredActor(AttachParent);
    }

    const FVector MuzzleStart = MuzzlePoint->GetComponentLocation();

    FVector CameraStart = MuzzleStart;
    FVector CameraForward = MuzzlePoint->GetForwardVector();

    if (const APawn* OwnerPawn = Cast<APawn>(WeaponOwner))
    {
        if (AController* Controller = OwnerPawn->GetController())
        {
            FVector ViewLocation;
            FRotator ViewRotation;
            Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

            CameraStart = ViewLocation;
            CameraForward = ViewRotation.Vector();
        }
    }

    const FVector CameraEnd = CameraStart + CameraForward * TraceRange;

    FHitResult CameraHit;
    GetWorld()->LineTraceSingleByChannel(
        CameraHit,
        CameraStart,
        CameraEnd,
        ECC_Visibility,
        Params);

    FVector AimTarget = CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraEnd;

    FVector FireDirection = (AimTarget - MuzzleStart).GetSafeNormal();
    if (FireDirection.IsNearlyZero() || FVector::DotProduct(FireDirection, CameraForward) <= 0.0f)
    {
        FireDirection = CameraForward;
        AimTarget = MuzzleStart + FireDirection * TraceRange;
    }

    const FVector FireEnd = MuzzleStart + FireDirection * TraceRange;

    FHitResult FireHit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        FireHit,
        MuzzleStart,
        FireEnd,
        ECC_Visibility,
        Params);

    DrawDebugLine(
        GetWorld(),
        CameraStart,
        CameraHit.bBlockingHit ? CameraHit.ImpactPoint : CameraEnd,
        FColor::Blue,
        false,
        1.0f,
        0,
        0.75f);

    DrawDebugLine(
        GetWorld(),
        MuzzleStart,
        bHit ? FireHit.ImpactPoint : FireEnd,
        bHit ? FColor::Red : FColor::Green,
        false,
        1.0f,
        0,
        1.5f);

    if (bHit && FireHit.GetActor())
    {
        UE_LOG(LogTemp, Warning, TEXT("AK47 hit: %s"), *FireHit.GetActor()->GetName());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red,
                FString::Printf(TEXT("Hit: %s"), *FireHit.GetActor()->GetName()));
        }
    }
}