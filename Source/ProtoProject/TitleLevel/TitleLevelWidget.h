// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/IHttpRequest.h"
#include "ProtoProject/Network/ProtoNetClientSubsystem.h"
#include "TitleLevelWidget.generated.h"

class UButton;
class UEditableText;
class UProtoNetClientSubsystem;
UCLASS()
class PROTOPROJECT_API UTitleLevelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* LoginButton;
	
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SignInButton;
	
	UPROPERTY(meta = (BindWidgetOptional))
	UEditableText* Id_Input_field;
	
	UPROPERTY(meta = (BindWidgetOptional))
	UEditableText* Passwd_Input_field;
	
	UPROPERTY(meta = (BindWidgetOptional))
	UEditableText* IP_Input_field;
	
	
	UPROPERTY(meta = (BindWidgetOptional))
	UEditableText* API_Input_field;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LevelChange")
	TSoftObjectPtr<UWorld> SafePlaceLevel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, category = Test)
	FString FID;
		
	UPROPERTY(BlueprintReadWrite, EditAnywhere, category = Test)
	FString FPassword;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, category = Test)
	FString FIP;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, category = Test)
    FString FAPI;
	
	
	UFUNCTION()
	void OnClickLogIn();

	UFUNCTION()
	void OnClickSignIn();

	UFUNCTION()
	void HandleLoginSucceeded(int32 PlayerId, bool bHasSavedProgress);

	UFUNCTION()
	void HandleLoginFailed(EProtoLoginFailReason Reason, const FString& Message);

	UProtoNetClientSubsystem* GetNetClient() const;

private:
	// true면 검증 통과 후 ConnectAndRegister, false면 ConnectAndLogin을 호출한다.
	bool bPendingRegister = false;

	// FAPI가 비어있으면 검증 없이 바로 ProceedWithConnect. 비어있지 않으면 Gemini
	// ListModels 엔드포인트로 가벼운 GET을 날려 키가 실제로 유효한지 확인한 뒤에만
	// 서버 로그인/회원가입으로 진행한다 (레벨 전환은 그 로그인 성공에 달려있으므로).
	void ValidateApiKeyThenConnect();
	void HandleApiKeyValidationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ProceedWithConnect();
};
