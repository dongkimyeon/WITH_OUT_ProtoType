// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelChangeSelectWidget.h"
#include "Components/Button.h"
#include  "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../Network/ProtoNetClientSubsystem.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"

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

	// 매칭 서버 설계 (재설계): Multi를 고르면 실제 Game 서버에 티켓으로 합류를
	// 시도하고, 최대 10초(EchoServer::kMatchWindow) 동안 다른 플레이어가 모이는
	// 걸 기다린다 -- 그동안 "플레이어를 기다리는 중.. (N/4)" 오버레이를 보여준다
	// (ShowMatchmakingOverlay/HandleMatchmakingStatus). 실패하거나
	// MatchmakingTimeoutSeconds 안에 응답이 없어도(아직 Game 서버가 안 떠 있거나,
	// Login/Game이 분리 안 된 예전 서버에 붙어있는 경우 포함) 레벨은 그냥 연다 --
	// 그 폴백 경로가 Login 연결로 게임플레이를 계속 정상 동작시키므로, 매칭은
	// "되면 더 좋은" 개선이지 필수 관문이 아니다.
	if (Mode == ELevelChangeMode::Multi && NetClient && NetClient->IsConnected())
	{
		bWaitingForMatch = true;
		PendingMultiLevel = Level;

		NetClient->OnMatchTicketReceived.AddUniqueDynamic(this, &ULevelChangeSelectWidget::HandleMatchTicketReceived);
		NetClient->OnJoinMatchFailed.AddUniqueDynamic(this, &ULevelChangeSelectWidget::HandleJoinMatchFailed);
		NetClient->OnMatchmakingStatus.AddUniqueDynamic(this, &ULevelChangeSelectWidget::HandleMatchmakingStatus);
		NetClient->OnMatchmakingComplete.AddUniqueDynamic(this, &ULevelChangeSelectWidget::HandleMatchmakingComplete);

		ShowMatchmakingOverlay();

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
	// Continues via HandleMatchmakingStatus (still waiting, headcount
	// update) / HandleMatchmakingComplete (done, join a Room) /
	// HandleJoinMatchFailed (bad/expired ticket) -- or HandleMatchTimeout,
	// if the Game server never replies at all (e.g. down, unreachable).
}

void ULevelChangeSelectWidget::HandleMatchmakingStatus(int32 Current, int32 Max)
{
	if (!bWaitingForMatch)
		return; // Stale/unrelated status -- not ours anymore.

	UpdateMatchmakingOverlay(Current, Max);
}

void ULevelChangeSelectWidget::HandleMatchmakingComplete(int32 MemberCount)
{
	if (!bWaitingForMatch)
		return;

	UE_LOG(LogTemp, Log, TEXT("매칭 완료 (%d명) -- 레벨 진입"), MemberCount);
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

	// A ticket may have arrived and ConnectToGameServerAndJoin may have
	// already opened GameSocket (connected at the TCP level) with
	// C2S_JoinMatch still in flight -- the Game server just never answered
	// within MatchmakingTimeoutSeconds. SendGameplayPacketBytes only checks
	// for a non-null GameSocket, not whether the join actually completed,
	// so leaving it up here would silently blackhole every gameplay packet
	// from now on instead of falling back to the Login connection (same
	// failure mode HandleJoinMatchFailed's S2C_JoinMatchFail guards against
	// -- see UProtoNetClientSubsystem::HandleIncomingPacket's comment).
	// No-op if the ticket never arrived at all (GameSocket was never opened).
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->DisconnectFromGameServer();
		}
	}

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
			NetClient->OnJoinMatchFailed.RemoveDynamic(this, &ULevelChangeSelectWidget::HandleJoinMatchFailed);
			NetClient->OnMatchmakingStatus.RemoveDynamic(this, &ULevelChangeSelectWidget::HandleMatchmakingStatus);
			NetClient->OnMatchmakingComplete.RemoveDynamic(this, &ULevelChangeSelectWidget::HandleMatchmakingComplete);
		}
	}

	HideMatchmakingOverlay();

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

/*-------------------
 매칭 대기 오버레이 (순수 Slate, UMG 없음)
-------------------*/
void ULevelChangeSelectWidget::ShowMatchmakingOverlay()
{
	if (MatchmakingStatusText.IsValid())
		return; // Already showing (e.g. a double-click) -- nothing to do.

	SAssignNew(MatchmakingStatusText, STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
		.ColorAndOpacity(FLinearColor::White)
		.ShadowOffset(FVector2D(1.0f, 1.0f))
		.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		// 첫 S2C_MatchmakingStatus가 도착하기 전까지의 짧은 공백(티켓 발급 +
		// Game 서버 재접속 왕복) 동안만 보이는 자리표시자 -- max(정원)를
		// 클라이언트에 하드코딩하지 않기 위해 실제 숫자는 서버가 알려줄 때까지
		// 기다린다.
		.Text(FText::FromString(TEXT("매칭 준비 중..")));

	MatchmakingOverlayWidget = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			.Padding(FMargin(28.f, 18.f))
			[
				MatchmakingStatusText.ToSharedRef()
			]
		];

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(MatchmakingOverlayWidget.ToSharedRef(), /*ZOrder=*/ 100);
	}
}

void ULevelChangeSelectWidget::UpdateMatchmakingOverlay(int32 Current, int32 Max)
{
	if (!MatchmakingStatusText.IsValid())
		return; // Overlay was never shown (or already torn down) -- nothing to update.

	MatchmakingStatusText->SetText(FText::FromString(
		FString::Printf(TEXT("플레이어를 기다리는 중.. (%d/%d)"), Current, Max)));
}

void ULevelChangeSelectWidget::HideMatchmakingOverlay()
{
	if (MatchmakingOverlayWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MatchmakingOverlayWidget.ToSharedRef());
	}
	MatchmakingOverlayWidget.Reset();
	MatchmakingStatusText.Reset();
}
