#include "DropItem.h"
#include "../ProtoCharacter.h"
#include "../Inventory/InventoryGridComponent.h"

ADropItem::ADropItem()
{
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;

	BoundingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundingBox"));
	BoundingBox->SetupAttachment(StaticMeshComp);
	BoundingBox->SetBoxExtent(FVector(30.f, 30.f, 30.f));
	BoundingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundingBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(StaticMeshComp);
	InteractBox->SetBoxExtent(FVector(150.f, 150.f, 150.f));
	InteractBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ADropItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!StaticMeshComp) return;

	if (ItemData && !ItemData->ItemMesh.IsNull())
	{
		StaticMeshComp->SetStaticMesh(ItemData->ItemMesh.LoadSynchronous());
	}
	else
	{
		StaticMeshComp->SetStaticMesh(nullptr);
	}
}

void ADropItem::BeginPlay()
{
	Super::BeginPlay();

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &ADropItem::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &ADropItem::OnInteractEndOverlap);
}

void ADropItem::OnInteractBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableEnter(this);
}

void ADropItem::OnInteractEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableExit(this);
}

void ADropItem::OnInteract_Implementation(AProtoCharacter* InPlayer)
{
	if (!InPlayer || !ItemData) return;
	InPlayer->GetInventoryComponent()->AddItem(ItemData);
	Destroy();
}

FText ADropItem::GetInteractPrompt_Implementation() const
{
	if (ItemData)
	{
		return FText::Format(FText::FromString(TEXT("F  줍기  [{0}]")), ItemData->DisplayName);
	}
	return FText::FromString(TEXT("F  줍기"));
}

bool ADropItem::CanInteract_Implementation(AProtoCharacter* InPlayer) const
{
	return true;
}

void ADropItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
