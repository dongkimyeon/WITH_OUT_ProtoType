// Fill out your copyright notice in the Description page of Project Settings.


#include "DropItem.h"

// Sets default values
ADropItem::ADropItem()
{
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;
}

void ADropItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!StaticMeshComp)
		return;

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

// Called when the game starts or when spawned
void ADropItem::BeginPlay()
{
	Super::BeginPlay();
	

}

// Called every frame
void ADropItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

