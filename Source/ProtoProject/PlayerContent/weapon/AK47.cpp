#include "AK47.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "../ProtoCharacter.h"

AAK47::AAK47()
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
    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AAK47::OnWeaponBeginOverlap);

    UE_LOG(LogTemp, Warning, TEXT("AAK47 Constructor"));
}

void AAK47::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("AAK47 BeginPlay"));
}

void AAK47::OnWeaponBeginOverlap(
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

    const FString OtherActorName = OtherActor->GetName();
    const FString OtherCompName = OtherComp ? OtherComp->GetName() : TEXT("None");

    UE_LOG(LogTemp, Warning, TEXT("AK47 overlap actor: %s, component: %s"), *OtherActorName, *OtherCompName);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
            FString::Printf(TEXT("Overlap: %s / %s"), *OtherActorName, *OtherCompName));
    }

    AProtoCharacter* Character = Cast<AProtoCharacter>(OtherActor);
    if (!Character)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Not AProtoCharacter"));
        }
        return;
    }

    USkeletalMeshComponent* TargetMesh = Character->GetMesh();
    if (!TargetMesh)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Character Mesh None"));
        }
        return;
    }

    static const FName WeaponSocketName(TEXT("WeaponSocket"));

    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        true);
    if (!TargetMesh->DoesSocketExist(WeaponSocketName))
    {
        UE_LOG(LogTemp, Warning, TEXT("Socket '%s' does NOT exist on %s. Attach will still be attempted."),
            *WeaponSocketName.ToString(),
            *TargetMesh->GetName());
    }

    AttachToComponent(TargetMesh, AttachRules, WeaponSocketName);
    Character->bHasWeapon = true;
    Character->CurrentWeapon = this;

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
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("COL"));
    }

    UE_LOG(LogTemp, Warning, TEXT("AK47 attached to %s.%s on %s"),
        *Character->GetName(),
        *WeaponSocketName.ToString(),
        *TargetMesh->GetName());
}

void AAK47::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}



