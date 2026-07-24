// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleLevelWidget.generated.h"

class UButton;
class UEditableText;
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
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, category = Test)
	FString FID;
		
	UPROPERTY(BlueprintReadWrite, EditAnywhere, category = Test)
	FString FPassword;
	
	UFUNCTION()
	void OnClickLogIn();
	
	UFUNCTION()
	void OnClickSignIn();
	
};
