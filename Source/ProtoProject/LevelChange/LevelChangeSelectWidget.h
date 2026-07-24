// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LevelChangeSelectWidget.generated.h"

class UButton;

UENUM(BlueprintType)
enum class ELevelChangeMode : uint8
{
	Single,
	Multi
};

/**
 *
 */
UCLASS()
class PROTOPROJECT_API ULevelChangeSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SingleMap1Button;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* SingleMap2Button;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* MultiMap1Button;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* MultiMap2Button;

	UFUNCTION()
	void OnClickSingleMap1();

	UFUNCTION()
	void OnClickSingleMap2();

	UFUNCTION()
	void OnClickMultiMap1();

	UFUNCTION()
	void OnClickMultiMap2();

	void RequestLevelChange(ELevelChangeMode Mode, FName MapName);
};
