// Fill out your copyright notice in the Description page of Project Settings.


#include "TitleLevelWidget.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "ProtoProject/Network/ProtoNetClientSubsystem.h"

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

	if (UProtoNetClientSubsystem* NetClient = GetNetClient())
	{
		if (!NetClient->ConnectAndLogin(FIP, FID, FPassword))
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("서버에 연결할 수 없습니다. 서버가 꺼져있는지 확인하세요."));
		}
	}
}

void UTitleLevelWidget::OnClickSignIn()
{
	if (Id_Input_field) FID = Id_Input_field->GetText().ToString();
	if (Passwd_Input_field) FPassword = Passwd_Input_field->GetText().ToString();
	if (IP_Input_field) FIP = IP_Input_field->GetText().ToString();

	if (UProtoNetClientSubsystem* NetClient = GetNetClient())
	{
		if (!NetClient->ConnectAndRegister(FIP, FID, FPassword))
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("서버에 연결할 수 없습니다. 서버가 꺼져있는지 확인하세요."));
		}
	}
}

void UTitleLevelWidget::HandleLoginSucceeded(int32 PlayerId, bool bHasSavedProgress)
{
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), TestLevel);
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


