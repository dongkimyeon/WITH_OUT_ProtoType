// Fill out your copyright notice in the Description page of Project Settings.

#include "RaidManager.h"
#include "RaidTimerWidget.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "../PlayerContent/PlayerStatusComponent.h"
#include "../Network/ProtoNetClientSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

ARaidManager::ARaidManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARaidManager::BeginPlay()
{
	Super::BeginPlay();
	// 로컬 플레이어는 아직 스폰 전일 수 있으므로 Tick에서 폴링해 잡는다.
}

void ARaidManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LocalStatus.IsValid())
	{
		LocalStatus->OnPlayerDied.RemoveDynamic(this, &ARaidManager::HandlePlayerDied);
		// 레이드 맵이 언로드되면 생존 시뮬레이션도 확실히 끈다. 폰이 트래블을 넘어
		// 유지되는 경우 허브(안전구역)에서 감염/굶주림으로 체력이 계속 깎이는 것을 막는다.
		LocalStatus->ResetSurvivalState();
	}

	if (TimerWidgetInstance)
	{
		TimerWidgetInstance->RemoveFromParent();
		TimerWidgetInstance = nullptr;
	}

	if (DeathScreenInstance)
	{
		DeathScreenInstance->RemoveFromParent();
		DeathScreenInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool ARaidManager::TryAcquireLocalPlayer()
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AProtoCharacter* Character = Cast<AProtoCharacter>(Pawn);
	if (!Character || !Character->IsLocallyControlled())
	{
		return false;
	}

	UPlayerStatusComponent* Status = Character->FindComponentByClass<UPlayerStatusComponent>();
	if (!Status)
	{
		return false;
	}

	LocalStatus = Status;

	Status->SetSurvivalSimulationActive(true);
	Status->OnPlayerDied.AddDynamic(this, &ARaidManager::HandlePlayerDied);

	if (RaidTimerWidgetClass)
	{
		TimerWidgetInstance = CreateWidget<URaidTimerWidget>(GetWorld(), RaidTimerWidgetClass);
		if (TimerWidgetInstance)
		{
			TimerWidgetInstance->AddToViewport();
			TimerWidgetInstance->SetRaidTime(RaidDuration, false);
		}
	}

	return true;
}

void ARaidManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRaidActive)
	{
		bRaidActive = TryAcquireLocalPlayer();
		return;
	}

	// 플레이어가 사라졌으면(레벨 이동 등) 더 할 일이 없다.
	if (!LocalStatus.IsValid())
	{
		return;
	}

	RaidElapsed += DeltaTime;

	if (!bOverdueTriggered && RaidElapsed >= RaidDuration)
	{
		bOverdueTriggered = true;
		LocalStatus->SetInfectionOverdue(true);
	}

	if (TimerWidgetInstance)
	{
		TimerWidgetInstance->SetRaidTime(FMath::Max(0.0f, RaidDuration - RaidElapsed), bOverdueTriggered);
	}
}

void ARaidManager::HandlePlayerDied()
{
	// 캐릭터가 지니고 있던 것(그리드/장비/퀵슬롯)은 AProtoCharacter::HandleDeath에서 전부 소실 처리한다.
	// 안전 창고(허브의 StorageContainer = 스태시)는 별개이므로 손대지 않는다.
	// RaidManager는 사망 연출과 허브 복귀만 담당한다.

	if (LocalStatus.IsValid())
	{
		// 사망 시점에 생존 시뮬레이션을 끄고 스탯을 안전 상태로 되돌린다.
		// 사망 화면 대기 중이나 허브 복귀 후에 굶주림 데미지가 이어지지 않게 한다.
		LocalStatus->ResetSurvivalState();
	}

	if (DeathScreenWidgetClass && !DeathScreenInstance)
	{
		DeathScreenInstance = CreateWidget<UUserWidget>(GetWorld(), DeathScreenWidgetClass);
		if (DeathScreenInstance)
		{
			DeathScreenInstance->AddToViewport(100);
		}
	}

	if (TimerWidgetInstance)
	{
		TimerWidgetInstance->RemoveFromParent();
		TimerWidgetInstance = nullptr;
	}

	GetWorldTimerManager().SetTimer(
		DeathReturnTimerHandle, this, &ARaidManager::ReturnToHub, FMath::Max(0.1f, DeathReturnDelay), false);
}

void ARaidManager::ReturnToHub()
{
	if (ExtractionFailLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[RaidManager] ExtractionFailLevel이 지정되지 않아 복귀할 수 없습니다."));
		return;
	}

	// Same reasoning as AExitPoint's successful-extraction path: without
	// this, the rest of the party's mirror of this (already ragdolled via
	// S2C_PlayerDied) player just stands frozen forever once we've loaded
	// away into ExtractionFailLevel, instead of despawning like a real
	// disconnect would.
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->SendSetVisible(false);
		}
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), ExtractionFailLevel);
}
