// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "CompanionChatTypes.h"
#include "CompanionBrainComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionReplyReady, const FString&, ReplyText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCompanionActionRequested, const FString&, ActionName, const FString&, ActionArg);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionBrainComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString GeminiApiKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString GeminiModel = TEXT("gemini-3.5-flash");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain", meta = (MultiLine = "true"))
	FString PersonaSystemPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString FallbackReply = TEXT("응, 알겠어.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	int32 MaxHistoryTurns = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	FString MemorySaveSlotName = TEXT("CompanionMemory");


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Brain")
	int32 MaxMemorySummaryLength = 2000;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Brain")
	FOnCompanionReplyReady OnReplyReady;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Brain")
	FOnCompanionActionRequested OnActionRequested;

	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void RequestReply(const FString& UserText);

	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void RequestReplyWithImage(const FString& UserText, const TArray<uint8>& ImageBytes);

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
