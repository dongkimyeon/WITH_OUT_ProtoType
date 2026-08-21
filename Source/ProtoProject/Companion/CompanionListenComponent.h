// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AudioCaptureCore.h"
#include "Interfaces/IHttpRequest.h"
#include "CompanionListenComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionTranscribed, const FString&, RecognizedText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionListenFailed, const FString&, ErrorMessage);

// 마이크를 눌러 녹음(PTT)하고, 뗀 시점에 로컬 STT 브릿지(NVIDIA NVIGI Whisper 사이드카)로 전송해 텍스트를 받아온다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionListenComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionListenComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Listen")
	FString SttBridgeUrl = TEXT("http://127.0.0.1:8090/stt");

	UPROPERTY(BlueprintAssignable, Category = "Companion|Listen")
	FOnCompanionTranscribed OnTranscribed;

	UPROPERTY(BlueprintAssignable, Category = "Companion|Listen")
	FOnCompanionListenFailed OnListenFailed;

	UFUNCTION(BlueprintCallable, Category = "Companion|Listen")
	void StartListening();

	UFUNCTION(BlueprintCallable, Category = "Companion|Listen")
	void StopListeningAndSend();

	UFUNCTION(BlueprintPure, Category = "Companion|Listen")
	bool IsListening() const { return bIsCapturing; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TUniquePtr<Audio::FAudioCaptureSynth> CaptureSynth;
	TArray<float> AccumulatedSamples;
	bool bIsCapturing = false;
	int32 CapturedSampleRate = 48000;
	int32 CapturedNumChannels = 1;

	void HandleSttResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
