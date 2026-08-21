// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "CompanionBrainComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionReplyReady, const FString&, ReplyText);

// 플레이어 발화 텍스트를 받아 Gemini API를 직접 호출하고, 답변 텍스트를 돌려준다.
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

	UPROPERTY(BlueprintAssignable, Category = "Companion|Brain")
	FOnCompanionReplyReady OnReplyReady;

	UFUNCTION(BlueprintCallable, Category = "Companion|Brain")
	void RequestReply(const FString& UserText);

protected:
	virtual void BeginPlay() override;

private:
	void HandleGeminiResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
