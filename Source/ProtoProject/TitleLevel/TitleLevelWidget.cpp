// Fill out your copyright notice in the Description page of Project Settings.


#include "TitleLevelWidget.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "ProtoProject/Network/ProtoNetClientSubsystem.h"
#include "ProtoProject/Companion/CompanionBridgeSubsystem.h"

void UTitleLevelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LoginButton) LoginButton->OnClicked.AddDynamic(this, &UTitleLevelWidget::OnClickLogIn);

	if (SignInButton) SignInButton->OnClicked.AddDynamic(this, &UTitleLevelWidget::OnClickSignIn);

	if (UProtoNetClientSubsystem* NetClient = GetNetClient())
	{
		NetClient->OnLoginSucceeded.AddDynamic(this, &UTitleLevelWidget::HandleLoginSucceeded);
		NetClient->OnLoginFailed.AddDynamic(this, &UTitleLevelWidget::HandleLoginFailed);
	}
}

void UTitleLevelWidget::OnClickLogIn()
{
	if (Id_Input_field) FID = Id_Input_field->GetText().ToString();
	if (Passwd_Input_field) FPassword = Passwd_Input_field->GetText().ToString();
	if (IP_Input_field) FIP = IP_Input_field->GetText().ToString();
	if (API_Input_field) FAPI = API_Input_field->GetText().ToString();

	bPendingRegister = false;
	ValidateApiKeyThenConnect();
}

void UTitleLevelWidget::OnClickSignIn()
{
	if (Id_Input_field) FID = Id_Input_field->GetText().ToString();
	if (Passwd_Input_field) FPassword = Passwd_Input_field->GetText().ToString();
	if (IP_Input_field) FIP = IP_Input_field->GetText().ToString();
	if (API_Input_field) FAPI = API_Input_field->GetText().ToString();

	bPendingRegister = true;
	ValidateApiKeyThenConnect();
}

void UTitleLevelWidget::ValidateApiKeyThenConnect()
{
	if (FAPI.IsEmpty())
	{
		ProceedWithConnect();
		return;
	}

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Gemini API 키 확인 중..."));

	const FString Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models?key=%s"), *FAPI);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindUObject(this, &UTitleLevelWidget::HandleApiKeyValidationResponse);
	Request->ProcessRequest();
}

void UTitleLevelWidget::HandleApiKeyValidationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		ProceedWithConnect();
		return;
	}

	const int32 Code = Response.IsValid() ? Response->GetResponseCode() : -1;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
			FString::Printf(TEXT("Gemini API 키가 올바르지 않습니다 (code=%d). 확인 후 다시 시도하세요."), Code));
	}
}

void UTitleLevelWidget::ProceedWithConnect()
{
	if (!FAPI.IsEmpty())
	{
		if (UCompanionBridgeSubsystem* Bridge = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCompanionBridgeSubsystem>() : nullptr)
		{
			Bridge->RuntimeGeminiApiKey = FAPI;
		}
	}

	UProtoNetClientSubsystem* NetClient = GetNetClient();
	if (!NetClient)
	{
		return;
	}

	const bool bConnected = bPendingRegister
		? NetClient->ConnectAndRegister(FIP, FID, FPassword)
		: NetClient->ConnectAndLogin(FIP, FID, FPassword);

	if (!bConnected && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("서버에 연결할 수 없습니다. 서버가 꺼져있는지 확인하세요."));
	}
}

void UTitleLevelWidget::HandleLoginSucceeded(int32 PlayerId, bool bHasSavedProgress)
{
	if (UProtoNetClientSubsystem* NetClient = GetNetClient())
	{
		NetClient->SetMultiplayerVisualsEnabled(false);
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), SafePlaceLevel);
}

void UTitleLevelWidget::HandleLoginFailed(EProtoLoginFailReason Reason, const FString& Message)
{
	FString Msg;
	switch (Reason)
	{
	case EProtoLoginFailReason::AccountNotFound:
		Msg = TEXT("아이디가 존재하지 않습니다.");
		break;
	case EProtoLoginFailReason::UsernameTaken:
		Msg = TEXT("이미 존재하는 계정입니다.");
		break;
	case EProtoLoginFailReason::InvalidToken:
		Msg = TEXT("비밀번호가 일치하지 않습니다.");
		break;
	default:
		Msg = FString::Printf(TEXT("로그인/회원가입 실패: %s"), *Message);
		break;
	}
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Msg);
}

UProtoNetClientSubsystem* UTitleLevelWidget::GetNetClient() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
}


