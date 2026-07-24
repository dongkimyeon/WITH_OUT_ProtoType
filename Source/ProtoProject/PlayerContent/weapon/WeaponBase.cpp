#include "WeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"

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
    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::OnWeaponBeginOverlap);
}

void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();
}

void AWeaponBase::OnWeaponBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this)
    {
        return;
    }

    AProtoCharacter* Character = Cast<AProtoCharacter>(OtherActor);
    if (!Character)
    {
        return;
    }

    EquipToCharacter(Character);
}

void AWeaponBase::EquipToCharacter(AProtoCharacter* Character)
{
    if (!Character)
    {
        return;
    }

    USkeletalMeshComponent* TargetMesh = Character->GetMesh();
    if (!TargetMesh)
    {
        return;
    }

    const FName EquipSocketName = WeaponType == EWeaponType::Pistol
        ? TEXT("PistolSocket")
        : TEXT("WeaponSocket");
    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        true);

    AttachToComponent(TargetMesh, AttachRules, EquipSocketName);
    SetOwner(Character);
    SetInstigator(Character);

    Character->bHasWeapon = WeaponType != EWeaponType::None;
    Character->CurrentWeapon = this;
    if (WeaponType == EWeaponType::Rifle)
    {
        Character->CurrentRifle = this;
    }
    else if (WeaponType == EWeaponType::Pistol)
    {
        Character->CurrentPistol = this;
    }
    Character->CurrentWeaponType = WeaponType;
    Character->PendingWeaponType = WeaponType;
    Character->SwapFromWeaponType = EWeaponType::None;
    Character->Swapping = 0.0f;
    Character->SwappingAlpha = true;

    SetActorEnableCollision(false);

    if (WeaponMesh)
    {
        WeaponMesh->SetSimulatePhysics(false);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        WeaponMesh->SetGenerateOverlapEvents(false);
    }

    if (CollisionBox)
    {
        CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
        CollisionBox->SetGenerateOverlapEvents(false);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Weapon Equipped"));
    }
}

void AWeaponBase::Fire()
{
    UE_LOG(LogTemp, Warning, TEXT("AWeaponBase::Fire called."));
}

void AWeaponBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
