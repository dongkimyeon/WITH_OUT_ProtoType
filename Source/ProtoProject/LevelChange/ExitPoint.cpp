// Fill out your copyright notice in the Description page of Project Settings.


#include "ExitPoint.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "../PlayerContent/PlayerStatusComponent.h"
#include "../Network/ProtoNetClientSubsystem.h"
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
		// 익스트랙션 성공. 허브(안전구역)로 넘어가기 전에 생존 시뮬레이션을 끄고
		// 감염/배고픔/목마름을 안전 상태로 되돌린다.
		if (UPlayerStatusComponent* Status = OverlappingPlayer->FindComponentByClass<UPlayerStatusComponent>())
		{
			Status->ResetSurvivalState();
		}

		// 파밍한 아이템/장착한 장비·퀵슬롯을 다음 레벨(SafePlace)로 이월한다. EndPlay(LevelTransition)에도
		// 백업 경로가 있지만, OpenLevel 직전 여기서 명시적으로 캐시해 EEndPlayReason 값에 의존하지 않게 한다.
		OverlappingPlayer->CacheTravelStateToNetClient();

		OverlappingPlayer = nullptr;
		RemainingTime = ExitDuration;

		if (ExitPointWidgetInstance)
		{
			ExitPointWidgetInstance->RemoveFromParent();
			ExitPointWidgetInstance = nullptr;
		}

		// Tell the rest of the party we're leaving via extraction -- same
		// mechanism a Single-map level travel already uses (see
		// UProtoNetClientSubsystem::SendSetVisible's schema comment), which
		// makes the server broadcast S2C_PlayerLeft so everyone else
		// despawns our (and our companion's) mirror instead of it just
		// standing there frozen once we've loaded into SafePlaceLevel. Not
		// SetMultiplayerVisualsEnabled(false): that also stops THIS
		// client's own future send/receive, which isn't ours to decide here
		// -- SafePlaceLevel's own BeginPlay logic (same as any other level
		// travel) is what re-establishes visibility correctly for wherever
		// we're actually headed.
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
			{
				NetClient->SendSetVisible(false);
			}
		}

		UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), SafePlaceLevel);
		return;
	}

	if (ExitPointWidgetInstance)
	{
		ExitPointWidgetInstance->SetRemainingTime(RemainingTime);
	}
}
