// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelChangeSelectWidget.h"
#include "Components/Button.h"
#include  "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../Network/ProtoNetClientSubsystem.h"
#include "../PlayerContent/ProtoCharacter.h"

void ULevelChangeSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// AddToViewport() 이후 RemoveFromParent()로 뗐다가 다시 AddToViewport()하면
	// 슬레이트 위젯이 재생성되며 NativeConstruct가 다시 호출될 수 있다.
	// RemoveDynamic으로 먼저 정리해서 델리게이트가 중복 바인딩되지 않게 한다.
	if (SingleMap1Button)
	{
		SingleMap1Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap1);
		SingleMap1Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap1);
	}
	if (SingleMap2Button)
	{
		SingleMap2Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap2);
		SingleMap2Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap2);
	}
	if (MultiMap1Button)
	{
		MultiMap1Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap1);
		MultiMap1Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap1);
	}
	if (MultiMap2Button)
	{
		MultiMap2Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap2);
		MultiMap2Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap2);
	}
}

void ULevelChangeSelectWidget::OnClickSingleMap1()
{
	RequestLevelChange(ELevelChangeMode::Single, Stage1Level);
}

void ULevelChangeSelectWidget::OnClickSingleMap2()
{
	RequestLevelChange(ELevelChangeMode::Single, Stage2Level);
}

void ULevelChangeSelectWidget::OnClickMultiMap1()
{
	RequestLevelChange(ELevelChangeMode::Multi, Stage1Level);
}

void ULevelChangeSelectWidget::OnClickMultiMap2()
{
	RequestLevelChange(ELevelChangeMode::Multi, Stage2Level);
}

void ULevelChangeSelectWidget::RequestLevelChange(ELevelChangeMode Mode, const TSoftObjectPtr<UWorld>& Level)
{
	const FString ModeText = (Mode == ELevelChangeMode::Single) ? TEXT("Single") : TEXT("Multi");
	const FString Msg = FString::Printf(TEXT("레벨 변경 요청: %s / %s "),
		*ModeText, *Level.ToSoftObjectPath().GetAssetName());
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, Msg);
	UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);

	// Single: stop broadcasting our moves/actions and stop showing other
	// players (see SetMultiplayerVisualsEnabled). Multi: same behavior as
	// the "test" level, which is the default -- explicit here in case the
	// player is coming back from a Single map. UProtoNetClientSubsystem is a
	// GameInstanceSubsystem so this setting (and the connection itself)
	// survives the OpenLevelBySoftObjectPtr() below.
	UProtoNetClientSubsystem* NetClient = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>();
	}

	if (NetClient)
	{
		NetClient->SetMultiplayerVisualsEnabled(Mode == ELevelChangeMode::Multi);
	}

	// 매칭 서버 설계 (step 5): Multi를 고르면 실제 Game 서버에 티켓으로 합류를
	// 시도한다 -- 성공하면 이후 모든 게임플레이 패킷이 그 Game 연결로 나간다
	// (SendGameplayPacketBytes). 실패하거나 MatchmakingTimeoutSeconds 안에
	// 응답이 없어도(아직 Game 서버가 안 떠 있거나, Login/Game이 분리 안 된
	// 예전 서버에 붙어있는 경우 포함) 레벨은 그냥 연다 -- 그 폴백 경로가
	// Login 연결로 게임플레이를 계속 정상 동작시키므로, 매칭은 "되면 더
	// 좋은" 개선이지 필수 관문이 아니다.
	if (Mode == ELevelChangeMode::Multi && NetClient && NetClient->IsConnected())
	{
		bWaitingForMatch = true;
		PendingMultiLevel = Level;

		NetClient->OnMatchTicketReceived.AddUniqueDynamic(this, &ULevelChangeSelectWidget::HandleMatchTicketReceived);
		NetClient->OnLoginSucceeded.AddUniqueDynamic(this, &ULevelChangeSelectWidget::HandleJoinMatchLoginSucceeded);
		NetClient->OnJoinMatchFailed.AddUniqueDynamic(this, &ULevelChangeSelectWidget::HandleJoinMatchFailed);

		NetClient->RequestMatch();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(MatchTimeoutHandle, this,
				&ULevelChangeSelectWidget::HandleMatchTimeout, MatchmakingTimeoutSeconds, false);
		}
		return;
	}

	FinishLevelChange(Level);
}

void ULevelChangeSelectWidget::HandleMatchTicketReceived(const FString& GameServerHost, int32 GameServerPort, const FString& Ticket, int64 ExpiresAtUnixMs)
{
	if (!bWaitingForMatch)
		return; // Ticket for a request we're not (or no longer) waiting on.

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->ConnectToGameServerAndJoin(Ticket, GameServerHost, GameServerPort);
		}
	}
	// Continues via HandleJoinMatchLoginSucceeded (join succeeded) or
	// HandleJoinMatchFailed (bad/expired ticket) -- or HandleMatchTimeout,
	// if the Game server never replies at all (e.g. down, unreachable).
}

void ULevelChangeSelectWidget::HandleJoinMatchLoginSucceeded(int32 PlayerId, bool bHasSavedProgress)
{
	if (!bWaitingForMatch)
		return; // An unrelated login succeeding (see this function's header comment) -- not ours.

	UE_LOG(LogTemp, Log, TEXT("매칭 합류 성공 (player_id=%d)"), PlayerId);
	FinishMatchmaking();
}

void ULevelChangeSelectWidget::HandleJoinMatchFailed(EProtoJoinMatchFailReason Reason)
{
	if (!bWaitingForMatch)
		return;

	UE_LOG(LogTemp, Warning, TEXT("매칭 합류 실패(reason=%d) -- 로그인 연결로 계속 진행"), static_cast<int32>(Reason));
	FinishMatchmaking();
}

void ULevelChangeSelectWidget::HandleMatchTimeout()
{
	if (!bWaitingForMatch)
		return;

	UE_LOG(LogTemp, Warning, TEXT("매칭 응답 타임아웃 -- 로그인 연결로 계속 진행"));
	FinishMatchmaking();
}

void ULevelChangeSelectWidget::FinishMatchmaking()
{
	bWaitingForMatch = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchTimeoutHandle);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->OnMatchTicketReceived.RemoveDynamic(this, &ULevelChangeSelectWidget::HandleMatchTicketReceived);
			NetClient->OnLoginSucceeded.RemoveDynamic(this, &ULevelChangeSelectWidget::HandleJoinMatchLoginSucceeded);
			NetClient->OnJoinMatchFailed.RemoveDynamic(this, &ULevelChangeSelectWidget::HandleJoinMatchFailed);
		}
	}

	const TSoftObjectPtr<UWorld> Level = PendingMultiLevel;
	PendingMultiLevel.Reset();
	FinishLevelChange(Level);
}

void ULevelChangeSelectWidget::FinishLevelChange(const TSoftObjectPtr<UWorld>& Level)
{
	// 허브에서 들고 가는 인벤토리 그리드/장비/퀵슬롯을 다음 레벨로 이월한다. EndPlay(LevelTransition)에도
	// 백업 경로가 있지만, OpenLevel 직전 여기서 명시적으로 캐시해 EEndPlayReason 값에 의존하지 않게 한다.
	if (AProtoCharacter* LocalCharacter = Cast<AProtoCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
	{
		LocalCharacter->CacheTravelStateToNetClient();
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), Level);
}
