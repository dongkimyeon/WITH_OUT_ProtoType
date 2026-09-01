// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaidTimerWidget.generated.h"

class UTextBlock;

UCLASS()
class PROTOPROJECT_API URaidTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Raid")
	void SetRaidTime(float RemainingSeconds, bool bOverdue);

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TimeText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;
};
