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
		NetClient->ConnectAndLogin(FIP, FID, FPassword);
	}
}

void UTitleLevelWidget::OnClickSignIn()
{
	if (Id_Input_field) FID = Id_Input_field->GetText().ToString();
	if (Passwd_Input_field) FPassword = Passwd_Input_field->GetText().ToString();
	if (IP_Input_field) FIP = IP_Input_field->GetText().ToString();

	if (UProtoNetClientSubsystem* NetClient = GetNetClient())
	{
		NetClient->ConnectAndRegister(FIP, FID, FPassword);
	}
}

void UTitleLevelWidget::HandleLoginSucceeded(int32 PlayerId, bool bHasSavedProgress)
{
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), TestLevel);
}

void UTitleLevelWidget::HandleLoginFailed(EProtoLoginFailReason Reason, const FString& Message)
{
	const FString Msg = FString::Printf(TEXT("로그인/회원가입 실패: %s"), *Message);
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Msg);
}

UProtoNetClientSubsystem* UTitleLevelWidget::GetNetClient() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
}


