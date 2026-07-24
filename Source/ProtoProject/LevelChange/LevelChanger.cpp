// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelChanger.h"
#include "../PlayerContent/ProtoCharacter.h"


// Sets default values
ALevelChanger::ALevelChanger()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(StaticMeshComp);
	InteractBox->SetBoxExtent(FVector(150.f, 150.f, 150.f));
	InteractBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ALevelChanger::OnInteract_Implementation(AProtoCharacter* InPlayer)
{
	if (InPlayer) InPlayer->OpenLevelChangeScreen(this);
}

FText ALevelChanger::GetInteractPrompt_Implementation() const
{
	return FText::FromString(TEXT("F  이동  [레벨 변경]"));
}

bool ALevelChanger::CanInteract_Implementation(AProtoCharacter* InPlayer) const
{
	return true;
}

// Called when the game starts or when spawned
void ALevelChanger::BeginPlay()
{
	Super::BeginPlay();

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &ALevelChanger::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &ALevelChanger::OnInteractEndOverlap);
}

void ALevelChanger::OnInteractBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableEnter(this);
}

void ALevelChanger::OnInteractEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableExit(this);
}

// Called every frame
void ALevelChanger::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

