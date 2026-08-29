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
#include "Particles/ParticleSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "../../Network/ProtoNetClientSubsystem.h"
#include "../../Enemy/EnemyBase.h"
#include "../../Companion/CompanionNPC.h"
#include "../../Companion/CompanionAIComponent.h"

AAK47::AAK47()
{
    WeaponType = EWeaponType::Rifle;
    bAutomatic = true;
    FireRate = 10.0f;
    MagazineCapacity = 60;
    CurrentAmmoInMagazine = MagazineCapacity;
    MaxReserveAmmo = 720;
    ReserveAmmo = MaxReserveAmmo;

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
    static ConstructorHelpers::FObjectFinder<UParticleSystem> BloodEffectFinder(TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Blood/P_Blood_Splat_Cone.P_Blood_Splat_Cone"));
    if (BloodEffectFinder.Succeeded())
    {
        BloodHitEffect = BloodEffectFinder.Object;
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

    if (!ConsumeAmmo())
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
    // 동료가 쏘는 무기는 자신이 지키는 플레이어를 조준 보정/명중 판정 모두에서 무시한다.
    // 그렇지 않으면 플레이어가 동료-적 사선에 끼었을 때 조준 보정용 카메라 트레이스가 플레이어에서
    // 막혀 그 지점을 조준점으로 삼아버려, 동료가 플레이어를 겨냥한 것처럼 보이는 문제가 생긴다.
    if (Cast<ACompanionNPC>(WeaponOwner))
    {
        if (APawn* OwningPlayer = UGameplayStatics::GetPlayerPawn(this, 0))
        {
            Params.AddIgnoredActor(OwningPlayer);
        }
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

    if (ACompanionNPC* CompanionOwnerActor = Cast<ACompanionNPC>(WeaponOwner))
    {
        // AIController::GetControlRotation()은 컨트롤러 자신의 틱에서 갱신되므로 이번 틱에 막
        // SetFocalPoint()한 값이 아직 반영 안 됐을 수 있다(교전 진입 첫 발이 직전 방향, 예를 들어
        // 플레이어 쪽을 향하는 원인). 컨트롤러 회전을 거치지 않고 실제 조준 대상 위치를 직접 조준한다.
        if (AActor* CombatTarget = CompanionOwnerActor->AIComponent ? CompanionOwnerActor->AIComponent->GetCombatTarget() : nullptr)
        {
            CameraStart = MuzzleStart;
            CameraForward = (CombatTarget->GetActorLocation() - CameraStart).GetSafeNormal();
        }
    }
    else if (const APawn* OwnerPawn = Cast<APawn>(WeaponOwner))
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

    if (bHit)
    {
        AEnemyBase* HitEnemy = Cast<AEnemyBase>(FireHit.GetActor());
        if (!HitEnemy && FireHit.GetComponent())
        {
            HitEnemy = Cast<AEnemyBase>(FireHit.GetComponent()->GetOwner());
        }

        if (HitEnemy)
        {
            HitEnemy->TakeEnemyDamage(10.0f);

            if (BloodHitEffect)
            {
                const FRotator BloodRotation = FRotationMatrix::MakeFromZ(FireHit.ImpactNormal).Rotator();
                UGameplayStatics::SpawnEmitterAtLocation(
                    GetWorld(),
                    BloodHitEffect,
                    FireHit.ImpactPoint,
                    BloodRotation,
                    FVector(0.1f),
                    true);
            }
        }
        else if (FireHit.GetActor())
        {
            UE_LOG(LogTemp, Warning, TEXT("AK47 hit: %s"), *FireHit.GetActor()->GetName());
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





