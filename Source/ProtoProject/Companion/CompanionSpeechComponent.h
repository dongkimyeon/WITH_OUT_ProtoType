// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "CompanionSpeechComponent.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCompanionSpeechFinished);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionSpeechComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionSpeechComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Speech")
	FString TtsServerUrl = TEXT("http://127.0.0.1:8080/tts");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Speech")
	FString Speaker = TEXT("Sohee");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Speech")
	FString Language = TEXT("Korean");

	UPROPERTY(BlueprintAssignable, Category = "Companion|Speech")
	FOnCompanionSpeechFinished OnSpeechFinished;

	// 지금 말하는 중이면 큐에 쌓아두고, 끝난 뒤 순서대로 재생한다(겹쳐 말하거나 끊기지 않게).
	UFUNCTION(BlueprintCallable, Category = "Companion|Speech")
	void Speak(const FString& Text);

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent;

	bool bIsSpeaking = false;
	TArray<FString> PendingLines;
	FTimerHandle PlaybackFinishedTimerHandle;

	void StartSpeaking(const FString& Text);
	void FinishSpeaking();
	void HandleTtsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
