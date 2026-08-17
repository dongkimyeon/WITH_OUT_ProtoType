// Fill out your copyright notice in the Description page of Project Settings.


#include "TitleLevelWidget.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Kismet/GameplayStatics.h"

void UTitleLevelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LoginButton) LoginButton->OnClicked.AddDynamic(this, &UTitleLevelWidget::OnClickLogIn);
	
	if (SignInButton) SignInButton->OnClicked.AddDynamic(this, &UTitleLevelWidget::OnClickSignIn);
}

void UTitleLevelWidget::OnClickLogIn()
{
	if (Id_Input_field) FID = Id_Input_field->GetText().ToString();
	if (Passwd_Input_field) FPassword = Passwd_Input_field->GetText().ToString();
	if (IP_Input_field) FIP = IP_Input_field->GetText().ToString();
	const FString Msg = FString::Printf(TEXT("로그인 : ID: %s / PW: %s IP : %s"),
			*FID, *FPassword, *FIP);
	GEngine->AddOnScreenDebugMessage(-1,1.0f,FColor::Green, Msg);
	
	//if(아이디 비번 IP 맞으면) 조건 추가 
	UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("TestMap")));
}

void UTitleLevelWidget::OnClickSignIn()
{
	if (Id_Input_field) FID = Id_Input_field->GetText().ToString();
	if (Passwd_Input_field) FPassword = Passwd_Input_field->GetText().ToString();
	const FString Msg = FString::Printf(TEXT("회원가입 : ID: %s / PW: %s "),
			*FID, *FPassword);
	
	GEngine->AddOnScreenDebugMessage(-1,1.0f,FColor::Red, Msg);
}


