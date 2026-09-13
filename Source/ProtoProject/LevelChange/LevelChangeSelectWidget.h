// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "../Network/ProtoNetClientSubsystem.h"
#include "LevelChangeSelectWidget.generated.h"

class UButton;
class SWidget;
class STextBlock;

UENUM(BlueprintType)
enum class ELevelChangeMode : uint8
{
	Single,
	Multi
};

/**
 *
 */
UCLASS()
class PROTOPROJECT_API ULevelChangeSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SingleMap1Button;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SingleMap2Button;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* MultiMap1Button;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* MultiMap2Button;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LevelChange")
	TSoftObjectPtr<UWorld> Stage1Level;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LevelChange")
	TSoftObjectPtr<UWorld> Stage2Level;

	// 서버의 매칭 대기열 자체가 최대 10초(EchoServer::kMatchWindow)까지 기다린다 --
	// 이 값은 그보다 넉넉하게 여유를 둬서(10초 + 네트워크 왕복/티켓 교환 시간) 정상
	// 매칭이라면 항상 서버 쪽 OnMatchmakingComplete가 먼저 도착하게 하기 위한
	// 최후의 안전장치일 뿐이다(HandleMatchTimeout -- 티켓 발급 실패, GameServer
	// 다운 등 아무 응답도 없는 경우에만 실제로 발동해야 정상). 서버의
	// kMatchWindow를 바꾸면 이 값도 같이 늘려야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LevelChange")
	float MatchmakingTimeoutSeconds = 13.0f;

	UFUNCTION()
	void OnClickSingleMap1();

	UFUNCTION()
	void OnClickSingleMap2();

	UFUNCTION()
	void OnClickMultiMap1();

	UFUNCTION()
	void OnClickMultiMap2();

	void RequestLevelChange(ELevelChangeMode Mode, const TSoftObjectPtr<UWorld>& Level);

private:
	/*-------------------
	 매칭 서버 설계 (재설계): Multi 선택 시 실제 Game 서버 티켓 합류 + 최대 10초
	 대기열 매칭을 시도하고, 그동안 "플레이어를 기다리는 중.. (N/4)"를 보여준다.
	-------------------*/
	// True only between RequestLevelChange(Multi, ...) starting the
	// matchmaking round trip and it resolving (success, failure, or
	// timeout) -- guards Handle*() below against reacting to a stale signal
	// (e.g. one arriving after HandleMatchTimeout already gave up) or one
	// meant for an unrelated flow.
	bool bWaitingForMatch = false;

	// The level RequestLevelChange(Multi, ...) was actually asked to open
	// -- opened once the matchmaking round trip above resolves, whichever
	// way it resolves (see FinishMatchmaking).
	TSoftObjectPtr<UWorld> PendingMultiLevel;

	FTimerHandle MatchTimeoutHandle;

	// "플레이어를 기다리는 중.. (N/4)" 오버레이 -- 순수 Slate로 뷰포트에 직접
	// 추가한다(UMG 위젯이 아님). 기존 로딩 화면(LoadingScreen.uasset, Async
	// Loading Screen 플러그인)은 실제 레벨 스트리밍이 시작되는 순간에만 뜨는
	// 구조라 이 매칭 대기 구간(아직 허브 맵에 그대로 있는 상태)엔 애초에 뜰 수
	// 없어서, 이 화면 전용으로 별도로 만든다. ShowMatchmakingOverlay가 두 멤버
	// 모두 채우고, HideMatchmakingOverlay가 둘 다 정리한다.
	TSharedPtr<SWidget> MatchmakingOverlayWidget;
	TSharedPtr<STextBlock> MatchmakingStatusText;

	void ShowMatchmakingOverlay();
	void UpdateMatchmakingOverlay(int32 Current, int32 Max);
	void HideMatchmakingOverlay();

	// S2C_MatchTicket arrived -- redeem it immediately. Completion continues
	// via HandleMatchmakingStatus/HandleMatchmakingComplete/
	// HandleJoinMatchFailed below, not this function.
	UFUNCTION()
	void HandleMatchTicketReceived(const FString& GameServerHost, int32 GameServerPort, const FString& Ticket, int64 ExpiresAtUnixMs);

	// S2C_MatchmakingStatus -- still waiting, just updates the overlay's
	// headcount. Can fire any number of times while queued.
	UFUNCTION()
	void HandleMatchmakingStatus(int32 Current, int32 Max);

	// S2C_MatchmakingComplete -- this session is actually in a Room now,
	// whether that's because the queue filled up, its 10-second window
	// elapsed, or it joined an already-active room instantly. The one true
	// "done waiting" signal for the Multi flow.
	UFUNCTION()
	void HandleMatchmakingComplete(int32 MemberCount);

	UFUNCTION()
	void HandleJoinMatchFailed(EProtoJoinMatchFailReason Reason);

	UFUNCTION()
	void HandleMatchTimeout();

	// Common tail for HandleMatchmakingComplete/HandleJoinMatchFailed/
	// HandleMatchTimeout: unbinds the temporary listeners, clears the
	// timeout timer, tears down the overlay, and opens PendingMultiLevel
	// regardless of how matchmaking actually resolved -- see
	// RequestLevelChange's comment for why a failed/timed-out match still
	// lets the player in (SendGameplayPacketBytes's Login-connection
	// fallback keeps gameplay working exactly as it did before this
	// feature existed).
	void FinishMatchmaking();

	// Shared tail of RequestLevelChange (Single, immediately) and
	// FinishMatchmaking (Multi, once matchmaking resolves) -- the level-
	// transition inventory carryover + the actual OpenLevelBySoftObjectPtr
	// call.
	void FinishLevelChange(const TSoftObjectPtr<UWorld>& Level);
};
