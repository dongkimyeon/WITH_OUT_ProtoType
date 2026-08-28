#include "ItemContainerBase.h"
#include "../ProtoCharacter.h"
#include "../Inventory/InventoryGridComponent.h"

AItemContainerBase::AItemContainerBase()
{
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(StaticMeshComp);
	InteractBox->SetBoxExtent(FVector(150.f, 150.f, 150.f));
	InteractBox->SetCollisionProfileName(TEXT("Trigger"));

	ContainerInventory = CreateDefaultSubobject<UInventoryGridComponent>(TEXT("ContainerInventory"));
	ContainerInventory->GridColumns = 5;
	ContainerInventory->GridRows = 4;

	ContainerName = FText::FromString(TEXT("상자"));
}

void AItemContainerBase::BeginPlay()
{
	Super::BeginPlay();

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &AItemContainerBase::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &AItemContainerBase::OnInteractEndOverlap);

	SeedContents();
}

void AItemContainerBase::OnInteractBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableEnter(this);
}

void AItemContainerBase::OnInteractEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableExit(this);
}

void AItemContainerBase::OnInteract_Implementation(AProtoCharacter* InPlayer)
{
	if (InPlayer) InPlayer->OpenContainerScreen(this);
}

FText AItemContainerBase::GetInteractPrompt_Implementation() const
{
	return FText::Format(FText::FromString(TEXT("F  열기  [{0}]")), ContainerName);
}

bool AItemContainerBase::CanInteract_Implementation(AProtoCharacter* InPlayer) const
{
	return true;
}

int32 AItemContainerBase::GetContainerId() const
{
	return static_cast<int32>(GetTypeHash(GetName()));
}
