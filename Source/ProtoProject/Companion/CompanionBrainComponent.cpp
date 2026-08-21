// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionBrainComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformMisc.h"
#include "Engine/Engine.h"

namespace
{
	void BrainDebugMsg(const FString& Msg, FColor Color = FColor::Yellow)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.0f, Color, Msg);
		}
	}
}

UCompanionBrainComponent::UCompanionBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	PersonaSystemPrompt =
		TEXT("너는 플레이어의 오래된 동료야. 따뜻하고 든든하게, 상대 말에 먼저 공감한 뒤 의견을 덧붙여.\n")
		TEXT("무조건 한국어 반말로, 2~3문장으로만 대답해. 행동 묘사(*..*)나 화자 표시, 괄호는 쓰지 마.\n")
		TEXT("네가 AI라는 사실은 절대 말하지 마.");
}

void UCompanionBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GeminiApiKey.IsEmpty())
	{
		GeminiApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("GEMINI_API_KEY"));
	}

	if (GeminiApiKey.IsEmpty())
	{
		BrainDebugMsg(TEXT("[Brain] GEMINI_API_KEY가 설정되어 있지 않습니다."), FColor::Red);
	}
}

void UCompanionBrainComponent::RequestReply(const FString& UserText)
{
	BrainDebugMsg(FString::Printf(TEXT("[Brain] RequestReply: \"%s\""), *UserText), FColor::Cyan);

	if (GeminiApiKey.IsEmpty())
	{
		BrainDebugMsg(TEXT("[Brain] API 키 없음 -> 폴백"), FColor::Red);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	TSharedRef<FJsonObject> SystemInstruction = MakeShared<FJsonObject>();
	{
		TSharedRef<FJsonObject> SystemPart = MakeShared<FJsonObject>();
		SystemPart->SetStringField(TEXT("text"), PersonaSystemPrompt);
		TArray<TSharedPtr<FJsonValue>> SystemParts;
		SystemParts.Add(MakeShared<FJsonValueObject>(SystemPart));
		SystemInstruction->SetArrayField(TEXT("parts"), SystemParts);
	}
	RootObject->SetObjectField(TEXT("system_instruction"), SystemInstruction);

	TSharedRef<FJsonObject> UserContent = MakeShared<FJsonObject>();
	{
		UserContent->SetStringField(TEXT("role"), TEXT("user"));
		TSharedRef<FJsonObject> UserPart = MakeShared<FJsonObject>();
		UserPart->SetStringField(TEXT("text"), UserText);
		TArray<TSharedPtr<FJsonValue>> UserParts;
		UserParts.Add(MakeShared<FJsonValueObject>(UserPart));
		UserContent->SetArrayField(TEXT("parts"), UserParts);
	}
	TArray<TSharedPtr<FJsonValue>> Contents;
	Contents.Add(MakeShared<FJsonValueObject>(UserContent));
	RootObject->SetArrayField(TEXT("contents"), Contents);

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RootObject, Writer);

	const FString Url = FString::Printf(
		TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"),
		*GeminiModel, *GeminiApiKey);

	BrainDebugMsg(FString::Printf(TEXT("[Brain] Gemini 요청 전송: model=%s"), *GeminiModel), FColor::Cyan);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestBody);
	Request->OnProcessRequestComplete().BindUObject(this, &UCompanionBrainComponent::HandleGeminiResponse);
	Request->ProcessRequest();
}

void UCompanionBrainComponent::HandleGeminiResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		const FString ErrorBody = Response.IsValid() ? Response->GetContentAsString() : TEXT("no response");
		const int32 Code = Response.IsValid() ? Response->GetResponseCode() : -1;
		BrainDebugMsg(FString::Printf(TEXT("[Brain] Gemini 호출 실패 (code=%d): %s"), Code, *ErrorBody), FColor::Red);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	BrainDebugMsg(FString::Printf(TEXT("[Brain] Gemini 응답 수신 (code=200): %s"), *Response->GetContentAsString()), FColor::Green);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		BrainDebugMsg(TEXT("[Brain] JSON 파싱 실패 -> 폴백"), FColor::Red);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Candidates;
	if (!JsonObject->TryGetArrayField(TEXT("candidates"), Candidates) || Candidates->Num() == 0)
	{
		BrainDebugMsg(TEXT("[Brain] candidates 없음 -> 폴백"), FColor::Red);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TSharedPtr<FJsonObject>* ContentObject;
	if (!(*Candidates)[0]->AsObject()->TryGetObjectField(TEXT("content"), ContentObject))
	{
		BrainDebugMsg(TEXT("[Brain] content 없음 -> 폴백"), FColor::Red);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Parts;
	if (!(*ContentObject)->TryGetArrayField(TEXT("parts"), Parts) || Parts->Num() == 0)
	{
		BrainDebugMsg(TEXT("[Brain] parts 없음 -> 폴백"), FColor::Red);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	FString ReplyText;
	if (!(*Parts)[0]->AsObject()->TryGetStringField(TEXT("text"), ReplyText) || ReplyText.IsEmpty())
	{
		BrainDebugMsg(TEXT("[Brain] text 없음 -> 폴백"), FColor::Red);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	ReplyText = ReplyText.TrimStartAndEnd();
	BrainDebugMsg(FString::Printf(TEXT("[Brain] 최종 응답: \"%s\""), *ReplyText), FColor::Green);
	OnReplyReady.Broadcast(ReplyText);
}
