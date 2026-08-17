#include "AK47.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "../../Network/ProtoNetClientSubsystem.h"

AAK47::AAK47()
{
    WeaponType = EWeaponType::Rifle;
    bAutomatic = true;
    FireRate = 10.0f;

    RifleSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RifleSkeletalMesh"));
    RifleSkeletalMesh->SetupAttachment(WeaponMesh);
    RifleSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMeshFinder(TEXT("/Game/FPS_Weapon_Bundle/Weapons/Meshes/Ka47/SK_KA47.SK_KA47"));
    if (RifleMeshFinder.Succeeded())
    {
        RifleSkeletalMesh->SetSkeletalMesh(RifleMeshFinder.Object);
    }

    static ConstructorHelpers::FClassFinder<AActor> RifleAmmoClassFinder(TEXT("/Game/Blueprint/weapon/BP_RifleAmmo"));
    if (RifleAmmoClassFinder.Succeeded())
    {
        RifleAmmoClass = RifleAmmoClassFinder.Class;
    }

    MuzzlePoint = CreateDefaultSubobject<UArrowComponent>(TEXT("MuzzlePoint"));
    MuzzlePoint->SetupAttachment(RifleSkeletalMesh);

    static ConstructorHelpers::FObjectFinder<USoundBase> FireSound01(TEXT("/Game/FreeWeaponSounds/Wav/AssaultRifle/Gunshots/assault_rifle_gunshot_01_Wav.assault_rifle_gunshot_01_Wav"));
    if (FireSound01.Succeeded())
    {
        RifleFireSounds.Add(FireSound01.Object);
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> FireSound02(TEXT("/Game/FreeWeaponSounds/Wav/AssaultRifle/Gunshots/assault_rifle_gunshot_02_Wav.assault_rifle_gunshot_02_Wav"));
    if (FireSound02.Succeeded())
    {
        RifleFireSounds.Add(FireSound02.Object);
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> FireSound03(TEXT("/Game/FreeWeaponSounds/Wav/AssaultRifle/Gunshots/assault_rifle_gunshot_03_Wav.assault_rifle_gunshot_03_Wav"));
    if (FireSound03.Succeeded())
    {
        RifleFireSounds.Add(FireSound03.Object);
    }
}


bool AAK47::GetLeftHandSocketTransform(FTransform& OutTransform) const
{
    static const FName LeftHandSocketName(TEXT("LeftHandSocket"));

    if (RifleSkeletalMesh && RifleSkeletalMesh->DoesSocketExist(LeftHandSocketName))
    {
        OutTransform = RifleSkeletalMesh->GetSocketTransform(LeftHandSocketName, RTS_World);
        return true;
    }

    return Super::GetLeftHandSocketTransform(OutTransform);
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

    if (RifleFireSounds.Num() > 0)
    {
        const int32 RandomFireSoundIndex = FMath::RandRange(0, RifleFireSounds.Num() - 1);
        if (USoundBase* FireSound = RifleFireSounds[RandomFireSoundIndex])
        {
            UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleStart, FireSoundVolume, FireSoundPitch);
        }
    }

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

    /*-------------------
     네트워킹: 발사 브로드캐스트
    -------------------*/
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendAttackFire(MuzzleStart, FireDirection);
        }
    }

    const FVector FireEnd = MuzzleStart + FireDirection * TraceRange;

    FHitResult FireHit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        FireHit,
        MuzzleStart,
        FireEnd,
        ECC_Visibility,
        Params);
    // Demo build: fire trace debug lines disabled.

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
        UE_LOG(LogTemp, Warning, TEXT("AK47 hit: %s"), *FireHit.GetActor()->GetName());

        if (GEngine)
        {
        }
    }
}
void AAK47::ReloadAmmoAttach(USkeletalMeshComponent* CharacterMesh)
{
    SetWeaponMagazineHidden(true);

    if (HandAmmoActor)
    {
        HandAmmoActor->Destroy();
        HandAmmoActor = nullptr;
    }

    HandAmmoActor = SpawnAmmoInHand(CharacterMesh);
}

void AAK47::ReloadAmmoDetach()
{
    if (!HandAmmoActor)
    {
        return;
    }

    HandAmmoActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(HandAmmoActor->GetRootComponent()))
    {
        Primitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Primitive->SetSimulatePhysics(true);
    }

    HandAmmoActor = nullptr;
}

void AAK47::ReloadNewAmmo(USkeletalMeshComponent* CharacterMesh)
{
    if (HandAmmoActor)
    {
        HandAmmoActor->Destroy();
        HandAmmoActor = nullptr;
    }

    HandAmmoActor = SpawnAmmoInHand(CharacterMesh);
}

void AAK47::ReloadNewAmmoAttach()
{
    if (HandAmmoActor)
    {
        HandAmmoActor->Destroy();
        HandAmmoActor = nullptr;
    }

    SetWeaponMagazineHidden(false);
}

AActor* AAK47::SpawnAmmoInHand(USkeletalMeshComponent* CharacterMesh)
{
    if (!GetWorld() || !CharacterMesh || !RifleAmmoClass)
    {
        return nullptr;
    }

    const FName AttachSocketName = CharacterMesh->DoesSocketExist(TEXT("HandGrip_L")) ? TEXT("HandGrip_L") : AmmoHandSocketName;
    const FTransform SpawnTransform = CharacterMesh->GetSocketTransform(AttachSocketName, RTS_World);
    AActor* AmmoActor = GetWorld()->SpawnActor<AActor>(RifleAmmoClass, SpawnTransform);
    if (!AmmoActor)
    {
        return nullptr;
    }

    if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(AmmoActor->GetRootComponent()))
    {
        Primitive->SetSimulatePhysics(false);
        Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (AWeaponBase* AmmoAsWeapon = Cast<AWeaponBase>(AmmoActor))
    {
        if (AmmoAsWeapon->CollisionBox)
        {
            AmmoAsWeapon->CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            AmmoAsWeapon->CollisionBox->SetGenerateOverlapEvents(false);
        }
    }

    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        false);

    AmmoActor->AttachToComponent(CharacterMesh, AttachRules, AttachSocketName);
    AmmoActor->SetActorRelativeTransform(HandAmmoRelativeTransform);
    return AmmoActor;
}

void AAK47::SetWeaponMagazineHidden(bool bShouldHide)
{
    if (!RifleSkeletalMesh)
    {
        return;
    }

    if (bShouldHide)
    {
        RifleSkeletalMesh->HideBoneByName(MagazineBoneName, EPhysBodyOp::PBO_None);
    }
    else
    {
        RifleSkeletalMesh->UnHideBoneByName(MagazineBoneName);
    }
}




