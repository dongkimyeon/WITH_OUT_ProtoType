#include "Pistol.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameInstance.h"
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

APistol::APistol()
{
    WeaponType = EWeaponType::Pistol;
    MagazineCapacity = 20;
    CurrentAmmoInMagazine = MagazineCapacity;
    MaxReserveAmmo = 240;
    ReserveAmmo = MaxReserveAmmo;

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

    static ConstructorHelpers::FObjectFinder<USoundBase> FireSound01(TEXT("/Game/FreeWeaponSounds/Wav/Handgun/Gunshots/handgun_sil_gunshot_01_Wav.handgun_sil_gunshot_01_Wav"));
    if (FireSound01.Succeeded())
    {
        PistolFireSounds.Add(FireSound01.Object);
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> FireSound02(TEXT("/Game/FreeWeaponSounds/Wav/Handgun/Gunshots/handgun_sil_gunshot_02_Wav.handgun_sil_gunshot_02_Wav"));
    if (FireSound02.Succeeded())
    {
        PistolFireSounds.Add(FireSound02.Object);
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> FireSound03(TEXT("/Game/FreeWeaponSounds/Wav/Handgun/Gunshots/handgun_sil_gunshot_03_Wav.handgun_sil_gunshot_03_Wav"));
    if (FireSound03.Succeeded())
    {
        PistolFireSounds.Add(FireSound03.Object);
    }
    static ConstructorHelpers::FObjectFinder<UParticleSystem> BloodEffectFinder(TEXT("/Game/Realistic_Starter_VFX_Pack_Vol2/Particles/Blood/P_Blood_Splat_Cone.P_Blood_Splat_Cone"));
    if (BloodEffectFinder.Succeeded())
    {
        BloodHitEffect = BloodEffectFinder.Object;
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

    if (PistolFireSounds.Num() > 0)
    {
        const int32 RandomFireSoundIndex = FMath::RandRange(0, PistolFireSounds.Num() - 1);
        if (USoundBase* FireSound = PistolFireSounds[RandomFireSoundIndex])
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
            UE_LOG(LogTemp, Warning, TEXT("Pistol hit: %s"), *FireHit.GetActor()->GetName());
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





