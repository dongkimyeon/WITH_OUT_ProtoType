// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "../Network/ProtoNetClientSubsystem.h"
#include "LevelChangeSelectWidget.generated.h"

class UButton;

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

	// How long to wait for RequestMatch/ConnectToGameServerAndJoin to
	// resolve before giving up and opening the level anyway (see
	// HandleMatchTimeout). Generous for a LAN/localhost setup; tune down
	// once a real deployment's round trip is known.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LevelChange")
	float MatchmakingTimeoutSeconds = 5.0f;

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
	 매칭 서버 설계 (step 5): Multi 선택 시 실제 Game 서버 티켓 합류를 시도
	-------------------*/
	// True only between RequestLevelChange(Multi, ...) starting the
	// RequestMatch/ConnectToGameServerAndJoin round trip and it resolving
	// (success, failure, or timeout) -- guards Handle*() below against
	// reacting to an unrelated OnLoginSucceeded/OnJoinMatchFailed (e.g. a
	// completely separate login elsewhere) firing while nothing here is
	// actually waiting on one.
	bool bWaitingForMatch = false;

	// The level RequestLevelChange(Multi, ...) was actually asked to open
	// -- opened once the matchmaking round trip above resolves, whichever
	// way it resolves (see FinishMatchmaking).
	TSoftObjectPtr<UWorld> PendingMultiLevel;

	FTimerHandle MatchTimeoutHandle;

	// S2C_MatchTicket arrived -- redeem it immediately. Success/failure
	// continue via HandleJoinMatchLoginSucceeded/HandleJoinMatchFailed
	// below, not this function.
	UFUNCTION()
	void HandleMatchTicketReceived(const FString& GameServerHost, int32 GameServerPort, const FString& Ticket, int64 ExpiresAtUnixMs);

	// Fires for ANY successful login, not just a match join (see
	// UProtoNetClientSubsystem::OnLoginSucceeded's comment) -- the
	// bWaitingForMatch guard is what makes this specifically "our
	// ConnectToGameServerAndJoin succeeded".
	UFUNCTION()
	void HandleJoinMatchLoginSucceeded(int32 PlayerId, bool bHasSavedProgress);

	UFUNCTION()
	void HandleJoinMatchFailed(EProtoJoinMatchFailReason Reason);

	UFUNCTION()
	void HandleMatchTimeout();

	// Common tail for HandleJoinMatchLoginSucceeded/HandleJoinMatchFailed/
	// HandleMatchTimeout: unbinds the temporary listeners, clears the
	// timeout timer, and opens PendingMultiLevel regardless of how
	// matchmaking actually resolved -- see RequestLevelChange's comment
	// for why a failed/timed-out match still lets the player in (
	// SendGameplayPacketBytes's Login-connection fallback keeps gameplay
	// working exactly as it did before this feature existed).
	void FinishMatchmaking();

	// Shared tail of RequestLevelChange (Single, immediately) and
	// FinishMatchmaking (Multi, once matchmaking resolves) -- the level-
	// transition inventory carryover + the actual OpenLevelBySoftObjectPtr
	// call.
	void FinishLevelChange(const TSoftObjectPtr<UWorld>& Level);
};
