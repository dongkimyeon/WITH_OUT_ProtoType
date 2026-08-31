// Fill out your copyright notice in the Description page of Project Settings.


#include "Door.h"
#include "Components/StaticMeshComponent.h"
#include "../PlayerContent/ProtoCharacter.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Root);
	// 카메라 라인트레이스(ECC_Visibility)에 맞아야 상호작용이 실행됨.
	DoorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(Root);
	InteractBox->SetBoxExtent(FVector(120.f, 120.f, 120.f));
	InteractBox->SetCollisionProfileName(TEXT("Trigger"));
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &ADoor::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &ADoor::OnInteractEndOverlap);

	CurrentYaw = DoorMesh->GetRelativeRotation().Yaw;
}

void ADoor::OnInteract_Implementation(AProtoCharacter* InPlayer)
{
	bIsOpen = !bIsOpen;

	if (bIsOpen && InPlayer)
	{
		// 플레이어가 문 앞쪽(ForwardVector 방향)에 있으면 뒤로, 뒤쪽에 있으면 앞으로 = 항상 플레이어 반대편으로 열림.
		const FVector ToPlayer = (InPlayer->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const bool bPlayerInFront = FVector::DotProduct(ToPlayer, GetActorForwardVector()) > 0.f;
		float Sign = bPlayerInFront ? -1.f : 1.f;
		if (bInvertSwing)
		{
			Sign = -Sign;
		}
		OpenTargetYaw = Sign * FMath::Abs(OpenAngle);
	}
	else
	{
		OpenTargetYaw = 0.f;
	}
}

FText ADoor::GetInteractPrompt_Implementation() const
{
	return FText::FromString(bIsOpen ? TEXT("F  문 닫기") : TEXT("F  문 열기"));
}

bool ADoor::CanInteract_Implementation(AProtoCharacter* InPlayer) const
{
	return true;
}

void ADoor::OnInteractBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor))
	{
		Player->OnInteractableEnter(this);
	}
}

void ADoor::OnInteractEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor))
	{
		Player->OnInteractableExit(this);
	}
}

void ADoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 반대쪽으로 열려면 반드시 닫힘(0도)을 거친다: 목표가 현재와 부호가 반대면 먼저 0으로.
	float Goal = OpenTargetYaw;
	if (OpenTargetYaw * CurrentYaw < -KINDA_SMALL_NUMBER && !FMath::IsNearlyZero(CurrentYaw, 0.5f))
	{
		Goal = 0.f;
	}

	if (FMath::IsNearlyEqual(CurrentYaw, Goal, 0.05f))
	{
		return;
	}

	CurrentYaw = FMath::FInterpTo(CurrentYaw, Goal, DeltaTime, RotateSpeed);
	DoorMesh->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));
}
