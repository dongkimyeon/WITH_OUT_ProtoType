// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionSpeechComponent.h"
#include "CompanionAudioUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"

UCompanionSpeechComponent::UCompanionSpeechComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCompanionSpeechComponent::Speak(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("text"), Text);
	RootObject->SetStringField(TEXT("language"), Language);
	RootObject->SetStringField(TEXT("speaker"), Speaker);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RootObject, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TtsServerUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestBody);
	Request->OnProcessRequestComplete().BindUObject(this, &UCompanionSpeechComponent::HandleTtsResponse);
	Request->ProcessRequest();
}

void UCompanionSpeechComponent::HandleTtsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("CompanionSpeechComponent: TTS 서버 응답 실패"));
		OnSpeechFinished.Broadcast();
		return;
	}

	TArray<uint8> PCMData;
	int32 SampleRate = 0, NumChannels = 0, BitsPerSample = 0;
	if (!CompanionAudioUtils::ParseWavPCM(Response->GetContent(), PCMData, SampleRate, NumChannels, BitsPerSample) || BitsPerSample != 16)
	{
		UE_LOG(LogTemp, Warning, TEXT("CompanionSpeechComponent: WAV 파싱 실패 (BitsPerSample=%d)"), BitsPerSample);
		OnSpeechFinished.Broadcast();
		return;
	}

	USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(this);
	SoundWave->NumChannels = NumChannels;
	SoundWave->SetSampleRate(SampleRate);
	SoundWave->SoundGroup = SOUNDGROUP_Voice;
	SoundWave->bLooping = false;

	const int32 NumFrames = PCMData.Num() / (NumChannels * 2);
	SoundWave->Duration = NumChannels > 0 && SampleRate > 0 ? (float)NumFrames / (float)SampleRate : 0.0f;

	SoundWave->QueueAudio(PCMData.GetData(), PCMData.Num());

	if (AActor* Owner = GetOwner())
	{
		UGameplayStatics::SpawnSoundAttached(SoundWave, Owner->GetRootComponent());
	}

	OnSpeechFinished.Broadcast();
}
