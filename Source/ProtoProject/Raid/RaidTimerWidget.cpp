// Fill out your copyright notice in the Description page of Project Settings.

#include "RaidTimerWidget.h"
#include "Components/TextBlock.h"

void URaidTimerWidget::SetRaidTime(float RemainingSeconds, bool bOverdue)
{
	if (TimeText)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(RemainingSeconds));
		TimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60)));
	}

	if (StatusText)
	{
		StatusText->SetText(bOverdue
			? FText::FromString(TEXT("감염 폭증 — 즉시 탈출하세요"))
			: FText::GetEmpty());
	}
}
