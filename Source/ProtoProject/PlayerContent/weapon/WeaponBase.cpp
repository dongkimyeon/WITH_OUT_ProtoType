#include "WeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AWeaponBase::AWeaponBase()
{
    PrimaryActorTick.bCanEverTick = true;

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(WeaponMesh);
    CollisionBox->SetBoxExtent(FVector(50.0f, 15.0f, 15.0f));
    CollisionBox->SetGenerateOverlapEvents(true);
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BulletHoleMaterialFinder(TEXT("/Game/Blueprint/weapon/M_BulletHole.M_BulletHole"));
    if (BulletHoleMaterialFinder.Succeeded())
    {
        BulletHoleDecalMaterial = BulletHoleMaterialFinder.Object;
    }
}

void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();
}

void AWeaponBase::Fire()
{
    UE_LOG(LogTemp, Warning, TEXT("AWeaponBase::Fire called."));
}
bool AWeaponBase::CanFire() const
{
    return CurrentAmmoInMagazine > 0;
}

bool AWeaponBase::ConsumeAmmo(int32 Amount)
{
    if (Amount <= 0)
    {
        return true;
    }

    if (CurrentAmmoInMagazine < Amount)
    {
        return false;
    }

    CurrentAmmoInMagazine -= Amount;
    return true;
}

bool AWeaponBase::CanReload() const
{
    return MagazineCapacity > 0 && CurrentAmmoInMagazine < MagazineCapacity && ReserveAmmo > 0;
}

bool AWeaponBase::ReloadMagazine()
{
    if (!CanReload())
    {
        return false;
    }

    const int32 NeededAmmo = MagazineCapacity - CurrentAmmoInMagazine;
    const int32 AmmoToLoad = FMath::Min(NeededAmmo, ReserveAmmo);
    CurrentAmmoInMagazine += AmmoToLoad;
    ReserveAmmo -= AmmoToLoad;
    return AmmoToLoad > 0;
}

FString AWeaponBase::GetAmmoText() const
{
    return FString::Printf(TEXT("%d/%d"), CurrentAmmoInMagazine, ReserveAmmo);
}


bool AWeaponBase::GetLeftHandSocketTransform(FTransform& OutTransform) const
{
    static const FName LeftHandSocketName(TEXT("LeftHandSocket"));

    if (WeaponMesh && WeaponMesh->DoesSocketExist(LeftHandSocketName))
    {
        OutTransform = WeaponMesh->GetSocketTransform(LeftHandSocketName, RTS_World);
        return true;
    }

    if (const USceneComponent* RootScene = GetRootComponent())
    {
        if (RootScene->DoesSocketExist(LeftHandSocketName))
        {
            OutTransform = RootScene->GetSocketTransform(LeftHandSocketName, RTS_World);
            return true;
        }
    }

    return false;
}
void AWeaponBase::ReloadAmmoAttach(USkeletalMeshComponent* CharacterMesh)
{
}

void AWeaponBase::ReloadAmmoDetach()
{
}

void AWeaponBase::ReloadNewAmmo(USkeletalMeshComponent* CharacterMesh)
{
}

void AWeaponBase::ReloadNewAmmoAttach()
{
}
void AWeaponBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
