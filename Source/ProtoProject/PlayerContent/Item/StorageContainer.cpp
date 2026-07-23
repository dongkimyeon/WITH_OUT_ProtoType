#include "StorageContainer.h"
#include "../ProtoCharacter.h"
#include "../Inventory/InventoryGridComponent.h"

AStorageContainer::AStorageContainer()
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

void AStorageContainer::BeginPlay()
{
	Super::BeginPlay();

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &AStorageContainer::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &AStorageContainer::OnInteractEndOverlap);

	for (UItemDataBase* Item : DefaultLootItems)
	{
		if (Item) ContainerInventory->AddItem(Item);
	}
}

void AStorageContainer::OnInteractBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableEnter(this);
}

void AStorageContainer::OnInteractEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableExit(this);
}

void AStorageContainer::OnInteract_Implementation(AProtoCharacter* InPlayer)
{
	if (InPlayer) InPlayer->OpenContainerScreen(this);
}

FText AStorageContainer::GetInteractPrompt_Implementation() const
{
	return FText::Format(FText::FromString(TEXT("F  열기  [{0}]")), ContainerName);
}

bool AStorageContainer::CanInteract_Implementation(AProtoCharacter* InPlayer) const
{
	return true;
}
