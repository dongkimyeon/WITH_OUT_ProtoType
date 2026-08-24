// Fill out your copyright notice in the Description page of Project Settings.


#include "ExitPoint.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "ExitPointWidget.h"
#include "Kismet/GameplayStatics.h"

AExitPoint::AExitPoint()
{
	PrimaryActorTick.bCanEverTick = true;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;

	ExitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitBox"));
	ExitBox->SetupAttachment(StaticMeshComp);
	ExitBox->SetBoxExtent(FVector(150.f, 150.f, 150.f));
	ExitBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AExitPoint::BeginPlay()
{
	Super::BeginPlay();

	RemainingTime = ExitDuration;

	ExitBox->OnComponentBeginOverlap.AddDynamic(this, &AExitPoint::OnExitBoxBeginOverlap);
	ExitBox->OnComponentEndOverlap.AddDynamic(this, &AExitPoint::OnExitBoxEndOverlap);
}

void AExitPoint::OnExitBoxBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (!Player || !Player->IsLocallyControlled()) return;

	OverlappingPlayer = Player;
	RemainingTime = ExitDuration;

	if (ExitPointWidgetClass && !ExitPointWidgetInstance)
	{
		ExitPointWidgetInstance = CreateWidget<UExitPointWidget>(GetWorld(), ExitPointWidgetClass);
		if (ExitPointWidgetInstance)
		{
			ExitPointWidgetInstance->AddToViewport();
			ExitPointWidgetInstance->SetRemainingTime(RemainingTime);
		}
	}
}

void AExitPoint::OnExitBoxEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (!Player || Player != OverlappingPlayer) return;

	OverlappingPlayer = nullptr;
	RemainingTime = ExitDuration;

	if (ExitPointWidgetInstance)
	{
		ExitPointWidgetInstance->RemoveFromParent();
		ExitPointWidgetInstance = nullptr;
	}
}

void AExitPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!OverlappingPlayer) return;

	RemainingTime -= DeltaTime;
	if (RemainingTime <= 0.f)
	{
		OverlappingPlayer = nullptr;
		RemainingTime = ExitDuration;

		if (ExitPointWidgetInstance)
		{
			ExitPointWidgetInstance->RemoveFromParent();
			ExitPointWidgetInstance = nullptr;
		}

		UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), SafePlaceLevel);
		return;
	}

	if (ExitPointWidgetInstance)
	{
		ExitPointWidgetInstance->SetRemainingTime(RemainingTime);
	}
}
