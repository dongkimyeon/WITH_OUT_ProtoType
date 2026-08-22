// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "CompanionChatTypes.h"
#include "CompanionBrainComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionReplyReady, const FString&, ReplyText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCompanionActionRequested, const FString&, ActionName, const FString&, ActionArg);

// 플레이어 발화 텍스트를 받아 Gemini API를 직접 호출하고, 답변 텍스트(대화) 또는
// Function Calling 액션을 돌려준다. 세션 내 대화 이력(슬라이딩 윈도우)과 게임 재시작 후에도
// 유지되는 로컬 SaveGame 장기 기억을 함께 관리한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionBrainComponent();

	// 비워두면 BeginPlay에서 환경변수 GEMINI_API_KEY 값을 읽어온다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString GeminiApiKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString GeminiModel = TEXT("gemini-3.5-flash");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain", meta = (MultiLine = "true"))
	FString PersonaSystemPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString FallbackReply = TEXT("응, 알겠어.");

	// 세션 내 유지할 최대 대화 턴 수(1턴 = user+model 한 쌍). 초과분은 오래된 것부터 버린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	int32 MaxHistoryTurns = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString MemorySaveSlotName = TEXT("CompanionMemory");

	UPROPERTY(BlueprintAssignable, Category = "Companion|Brain")
	FOnCompanionReplyReady OnReplyReady;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Brain")
	FOnCompanionActionRequested OnActionRequested;

	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void RequestReply(const FString& UserText);

	// 비전 폴백 전용: 텍스트와 함께 그 순간의 화면 캡처(PNG 바이트)를 같이 보낸다.
	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void RequestReplyWithImage(const FString& UserText, const TArray<uint8>& ImageBytes);

	// 플레이어가 한동안 말이 없을 때 짧은 혼잣말을 유도한다(CommandRouter의 침묵 타이머가 호출).
	// Function Calling 도구를 붙이지 않아 실행 명령으로 오인되어 엉뚱한 행동이 트리거될 위험이 없다.
	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void RequestAmbientLine();

	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void SaveMemory();

	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void LoadMemory();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TArray<FCompanionChatTurn> ConversationHistory;

	FString MemorySummary;
	int32 NextRequestId = 0;
	int32 LatestRequestId = -1;

	void SendGeminiRequest(const FString& UserText, const TArray<uint8>* OptionalImageBytes, bool bIncludeTools = true);
	FString BuildSystemInstruction() const;
	void TrimHistory();

	void HandleGeminiResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 RequestId);
};
