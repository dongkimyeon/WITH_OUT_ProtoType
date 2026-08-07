#include "Pistol.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

APistol::APistol()
{
    WeaponType = EWeaponType::Pistol;

    PistolSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PistolSkeletalMesh"));
    PistolSkeletalMesh->SetupAttachment(WeaponMesh);
    PistolSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> PistolMeshFinder(TEXT("/Game/FPS_Weapon_Bundle/Weapons/Meshes/SMG11/SK_SMG11_Nostock_X.SK_SMG11_Nostock_X"));
    if (PistolMeshFinder.Succeeded())
    {
        PistolSkeletalMesh->SetSkeletalMesh(PistolMeshFinder.Object);
    }

    MuzzlePoint = CreateDefaultSubobject<UArrowComponent>(TEXT("MuzzlePoint"));
    MuzzlePoint->SetupAttachment(PistolSkeletalMesh);

    static ConstructorHelpers::FClassFinder<AActor> PistolAmmoClassFinder(TEXT("/Game/Blueprint/weapon/BP_PistolAmmo"));
    if (PistolAmmoClassFinder.Succeeded())
    {
        PistolAmmoClass = PistolAmmoClassFinder.Class;
    }
}

bool APistol::GetLeftHandSocketTransform(FTransform& OutTransform) const
{
    static const FName LeftHandSocketName(TEXT("LeftHandSocket"));

    if (PistolSkeletalMesh && PistolSkeletalMesh->DoesSocketExist(LeftHandSocketName))
    {
        OutTransform = PistolSkeletalMesh->GetSocketTransform(LeftHandSocketName, RTS_World);
        return true;
    }

    return Super::GetLeftHandSocketTransform(OutTransform);
}

void APistol::Fire()
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

    FCollisionQueryParams Params(SCENE_QUERY_STAT(PistolFire), false, this);
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

    if (bHit && BulletHoleDecalMaterial)
    {
        const FRotator DecalRotation = FRotationMatrix::MakeFromX(FireHit.ImpactNormal).Rotator();
        UGameplayStatics::SpawnDecalAtLocation(
            GetWorld(),
            BulletHoleDecalMaterial,
            BulletHoleDecalSize,
            FireHit.Location,
            DecalRotation,
            BulletHoleDecalLifeSpan);
    }

    if (bHit && FireHit.GetActor())
    {
        UE_LOG(LogTemp, Warning, TEXT("Pistol hit: %s"), *FireHit.GetActor()->GetName());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red,
                FString::Printf(TEXT("Pistol Hit: %s"), *FireHit.GetActor()->GetName()));
        }
    }
}

USkeletalMeshComponent* APistol::GetPistolSkeletalMesh() const
{
    if (PistolSkeletalMesh)
    {
        return PistolSkeletalMesh;
    }

    TArray<USkeletalMeshComponent*> SkeletalMeshes;
    GetComponents(SkeletalMeshes);

    for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
    {
        if (SkeletalMesh && SkeletalMesh->GetBoneIndex(MagazineBoneName) != INDEX_NONE)
        {
            return SkeletalMesh;
        }
    }

    return nullptr;
}

void APistol::SetWeaponMagazineHidden(bool bShouldHide)
{
    USkeletalMeshComponent* TargetPistolSkeletalMesh = GetPistolSkeletalMesh();
    if (!TargetPistolSkeletalMesh)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, TEXT("Pistol skeletal mesh not found"));
        }
        return;
    }

    if (bShouldHide)
    {
        TargetPistolSkeletalMesh->HideBoneByName(MagazineBoneName, EPhysBodyOp::PBO_None);
    }
    else
    {
        TargetPistolSkeletalMesh->UnHideBoneByName(MagazineBoneName);
    }
}

AActor* APistol::SpawnAmmoAtMagazine()
{
    if (!GetWorld() || !PistolAmmoClass)
    {
        return nullptr;
    }

    FTransform SpawnTransform = GetActorTransform();
    if (USkeletalMeshComponent* TargetPistolSkeletalMesh = GetPistolSkeletalMesh())
    {
        const int32 BoneIndex = TargetPistolSkeletalMesh->GetBoneIndex(MagazineBoneName);
        if (BoneIndex != INDEX_NONE)
        {
            SpawnTransform = TargetPistolSkeletalMesh->GetBoneTransform(BoneIndex);
        }
        else if (TargetPistolSkeletalMesh->DoesSocketExist(MagazineBoneName))
        {
            SpawnTransform = TargetPistolSkeletalMesh->GetSocketTransform(MagazineBoneName, RTS_World);
        }
    }

    AActor* AmmoActor = GetWorld()->SpawnActor<AActor>(PistolAmmoClass, SpawnTransform);
    if (!AmmoActor)
    {
        return nullptr;
    }

    if (AWeaponBase* AmmoWeapon = Cast<AWeaponBase>(AmmoActor))
    {
        AmmoWeapon->bCanBePickedUp = false;
        if (AmmoWeapon->CollisionBox)
        {
            AmmoWeapon->CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            AmmoWeapon->CollisionBox->SetGenerateOverlapEvents(false);
        }
    }

    if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AmmoActor->GetRootComponent()))
    {
        Primitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Primitive->SetCollisionResponseToAllChannels(ECR_Block);
        Primitive->SetSimulatePhysics(true);
    }

    return AmmoActor;
}

AActor* APistol::SpawnAmmoInHand(USkeletalMeshComponent* CharacterMesh)
{
    if (!GetWorld() || !CharacterMesh || !PistolAmmoClass)
    {
        return nullptr;
    }

    const FName AttachSocketName = CharacterMesh->DoesSocketExist(TEXT("HandGrip_L")) ? TEXT("HandGrip_L") : AmmoHandSocketName;
    AActor* AmmoActor = GetWorld()->SpawnActor<AActor>(PistolAmmoClass, CharacterMesh->GetSocketTransform(AttachSocketName, RTS_World));
    if (!AmmoActor)
    {
        return nullptr;
    }

    if (AWeaponBase* AmmoWeapon = Cast<AWeaponBase>(AmmoActor))
    {
        AmmoWeapon->bCanBePickedUp = false;
        if (AmmoWeapon->CollisionBox)
        {
            AmmoWeapon->CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            AmmoWeapon->CollisionBox->SetGenerateOverlapEvents(false);
        }
    }

    if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AmmoActor->GetRootComponent()))
    {
        Primitive->SetSimulatePhysics(false);
        Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    AmmoActor->AttachToComponent(
        CharacterMesh,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        AttachSocketName);
    AmmoActor->SetActorRelativeTransform(HandAmmoRelativeTransform);

    return AmmoActor;
}

void APistol::ReloadAmmoDetach()
{
    SpawnAmmoAtMagazine();
    SetWeaponMagazineHidden(true);
}

void APistol::ReloadNewAmmo(USkeletalMeshComponent* CharacterMesh)
{
    if (HandAmmoActor)
    {
        HandAmmoActor->Destroy();
        HandAmmoActor = nullptr;
    }

    HandAmmoActor = SpawnAmmoInHand(CharacterMesh);
}

void APistol::ReloadNewAmmoAttach()
{
    if (HandAmmoActor)
    {
        HandAmmoActor->Destroy();
        HandAmmoActor = nullptr;
    }

    SetWeaponMagazineHidden(false);
}
