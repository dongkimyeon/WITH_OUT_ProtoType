// Fill out your copyright notice in the Description page of Project Settings.


#include "ExitPointWidget.h"
#include "Components/TextBlock.h"

void UExitPointWidget::SetRemainingTime(float RemainingTime)
{
	if (!TimeText) return;

	TimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), FMath::Max(0.f, RemainingTime))));
}
