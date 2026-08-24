// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExitPointWidget.generated.h"

class UTextBlock;

UCLASS()
class PROTOPROJECT_API UExitPointWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ExitPoint")
	void SetRemainingTime(float RemainingTime);

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TimeText;
};
