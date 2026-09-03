// Fill out your copyright notice in the Description page of Project Settings.


#include "Door.h"
#include "Components/StaticMeshComponent.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "../Network/ProtoNetClientSubsystem.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Root);

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(Root);
	InteractBox->SetBoxExtent(FVector(120.f, 120.f, 120.f));
	InteractBox->SetCollisionProfileName(TEXT("Trigger"));

	// 라인트레이스 전용 히트 박스: 쿼리만, Visibility만 Block(총알/이동/물리에는 영향 없음).
	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Root);
	HitBox->SetBoxExtent(FVector(10.f, 60.f, 100.f));
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PromptAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("PromptAnchor"));
	PromptAnchor->SetupAttachment(Root);
	PromptAnchor->SetRelativeLocation(FVector(0.f, 60.f, 120.f));
	PromptAnchor->ComponentTags.Add(TEXT("PromptAnchor"));
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &ADoor::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &ADoor::OnInteractEndOverlap);

	CurrentYaw = DoorMesh->GetRelativeRotation().Yaw;

	// 다른 클라이언트가 이 문을 열고/닫으면 우리 쪽도 같이 반영한다 (see
	// UProtoNetClientSubsystem::OnDoorInteract's comment).
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->OnDoorInteract.AddDynamic(this, &ADoor::HandleDoorInteract);
		}
	}
}

void ADoor::OnInteract_Implementation(AProtoCharacter* InPlayer)
{
	const bool bNewOpen = !bIsOpen;

	float Sign = 1.f;
	if (bNewOpen && InPlayer)
	{
		// 플레이어가 문 앞쪽(ForwardVector 방향)에 있으면 뒤로, 뒤쪽에 있으면 앞으로 = 항상 플레이어 반대편으로 열림.
		const FVector ToPlayer = (InPlayer->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const bool bPlayerInFront = FVector::DotProduct(ToPlayer, GetActorForwardVector()) > 0.f;
		Sign = bPlayerInFront ? -1.f : 1.f;
	}
	if (bInvertSwing)
	{
		Sign = -Sign;
	}

	SetOpen(bNewOpen, Sign);

	// 다른 클라이언트들에게도 이 상태를 알린다 -- 서버는 별도 판정 없이 그대로 중계한다
	// (see UProtoNetClientSubsystem::SendDoorInteract's comment). Single 맵/오프라인이면
	// bMultiplayerVisualsEnabled 체크에서 조용히 no-op.
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->SendDoorInteract(GetDoorId(), bNewOpen);
		}
	}
}

int32 ADoor::GetDoorId() const
{
	return static_cast<int32>(GetTypeHash(GetName()));
}

void ADoor::HandleDoorInteract(int32 DoorId, bool bOpen)
{
	if (DoorId != GetDoorId())
	{
		// Not our door -- every door in the level shares this one
		// broadcast delegate.
		return;
	}

	// Remote toggle: no player reference to swing away from, so just use
	// the door's default sign (still respects bInvertSwing) -- see this
	// function's header comment.
	const float Sign = bInvertSwing ? -1.f : 1.f;
	SetOpen(bOpen, Sign);
}

void ADoor::SetOpen(bool bNewOpen, float SwingSign)
{
	bIsOpen = bNewOpen;
	OpenTargetYaw = bIsOpen ? SwingSign * FMath::Abs(OpenAngle) : 0.f;
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
