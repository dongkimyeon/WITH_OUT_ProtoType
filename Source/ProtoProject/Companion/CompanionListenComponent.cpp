// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionListenComponent.h"
#include "CompanionAudioUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UCompanionListenComponent::UCompanionListenComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bIsCapturing = false;
}

void UCompanionListenComponent::StartListening()
{
	if (bIsCapturing)
	{
		return;
	}

	if (!CaptureSynth.IsValid())
	{
		CaptureSynth = MakeUnique<Audio::FAudioCaptureSynth>();
	}

	Audio::FCaptureDeviceInfo DeviceInfo;
	if (CaptureSynth->GetDefaultCaptureDeviceInfo(DeviceInfo))
	{
		CapturedSampleRate = DeviceInfo.PreferredSampleRate > 0 ? DeviceInfo.PreferredSampleRate : 48000;
		CapturedNumChannels = DeviceInfo.InputChannels > 0 ? DeviceInfo.InputChannels : 1;
	}

	if (!CaptureSynth->OpenDefaultStream())
	{
		OnListenFailed.Broadcast(TEXT("마이크 스트림을 열 수 없습니다."));
		return;
	}

	AccumulatedSamples.Reset();
	CaptureSynth->StartCapturing();
	bIsCapturing = true;
}

void UCompanionListenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsCapturing && CaptureSynth.IsValid())
	{
		CaptureSynth->GetAudioData(AccumulatedSamples);
	}
}

void UCompanionListenComponent::StopListeningAndSend()
{
	if (!bIsCapturing || !CaptureSynth.IsValid())
	{
		return;
	}

	CaptureSynth->StopCapturing();
	CaptureSynth->GetAudioData(AccumulatedSamples);
	bIsCapturing = false;

	// 발화 하나가 끝났으면 스트림 자체도 닫는다 (열어둔 채로 두면 컴포넌트/액터 파괴 시
	// WASAPI 캡처 스레드가 이미 해제된 메모리를 건드려 크래시가 난다).
	CaptureSynth->AbortCapturing();

	if (AccumulatedSamples.Num() == 0)
	{
		OnListenFailed.Broadcast(TEXT("녹음된 오디오가 없습니다."));
		return;
	}

	// 스테레오 이상이면 첫 채널만 뽑아 모노로 보냄 (Whisper 등 STT는 모노 입력을 기대함).
	TArray<float> MonoSamples;
	if (CapturedNumChannels > 1)
	{
		MonoSamples.Reserve(AccumulatedSamples.Num() / CapturedNumChannels);
		for (int32 i = 0; i < AccumulatedSamples.Num(); i += CapturedNumChannels)
		{
			MonoSamples.Add(AccumulatedSamples[i]);
		}
	}
	else
	{
		MonoSamples = AccumulatedSamples;
	}

	TArray<uint8> WavBytes;
	CompanionAudioUtils::BuildWavFromFloatPCM(MonoSamples, CapturedSampleRate, 1, WavBytes);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(SttBridgeUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("audio/wav"));
	Request->SetContent(WavBytes);
	Request->OnProcessRequestComplete().BindUObject(this, &UCompanionListenComponent::HandleSttResponse);
	Request->ProcessRequest();
}

void UCompanionListenComponent::HandleSttResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		OnListenFailed.Broadcast(TEXT("STT 브릿지 응답 실패"));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OnListenFailed.Broadcast(TEXT("STT 응답 파싱 실패"));
		return;
	}

	FString RecognizedText;
	if (!JsonObject->TryGetStringField(TEXT("text"), RecognizedText) || RecognizedText.IsEmpty())
	{
		OnListenFailed.Broadcast(TEXT("인식된 텍스트가 없습니다."));
		return;
	}

	OnTranscribed.Broadcast(RecognizedText);
}

void UCompanionListenComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CaptureSynth.IsValid() && CaptureSynth->IsStreamOpen())
	{
		CaptureSynth->AbortCapturing();
	}
	bIsCapturing = false;
	Super::EndPlay(EndPlayReason);
}
