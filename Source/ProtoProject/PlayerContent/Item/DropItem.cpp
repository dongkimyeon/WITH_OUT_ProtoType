// Fill out your copyright notice in the Description page of Project Settings.

#include "DropItem.h"
#include "../ProtoCharacter.h"

ADropItem::ADropItem()
{
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;

	// 라인트레이스 대상 - Visibility 채널만 블록
	BoundingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundingBox"));
	BoundingBox->SetupAttachment(StaticMeshComp);
	BoundingBox->SetBoxExtent(FVector(30.f, 30.f, 30.f));
	BoundingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundingBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 근접 감지 트리거 - 오버랩 전용
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
		UStaticMesh* SelectedMesh = ItemData->ItemMesh.LoadSynchronous();
		StaticMeshComp->SetStaticMesh(SelectedMesh);
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
	if (Player) Player->ShowPickupPrompt(this);
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, TEXT("BeginOverlap"));
	
}

void ADropItem::OnInteractEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->HidePickupPrompt(this);
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, TEXT("EndOverLap"));
}

void ADropItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
