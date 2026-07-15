// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerDefalutUI.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Item/DropItem.h"

void UPlayerDefalutUI::ShowPickupPrompt(ADropItem* Item)
{
	FocusedItem = Item;
	if (PickupPromptText)
	{
		PickupPromptText->SetVisibility(ESlateVisibility::Visible);
		PickupPromptText->SetText(FText::FromString(TEXT("F 습득")));
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("PickupPromptText OK"));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("PickupPromptText is NULL"));
	}
}

void UPlayerDefalutUI::HidePickupPrompt()
{
	FocusedItem = nullptr;
	if (PickupPromptText)
		PickupPromptText->SetVisibility(ESlateVisibility::Hidden);
}

void UPlayerDefalutUI::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (!FocusedItem || !PickupPromptText) return;

	FVector2D ScreenPos;
	
	FVector TargetLocation = FocusedItem->GetActorLocation() + FVector(0.f, 0.f, 50.f);

	bool bOnScreen = GetOwningPlayer()->ProjectWorldLocationToScreen(TargetLocation, ScreenPos);

	if (bOnScreen)
	{
		// 핵심: 화면의 픽셀 좌표를 현재 DPI 스케일 비율로 나누어 UMG 캔버스 좌표로 변환합니다.	
		float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
		FVector2D WidgetPos = ScreenPos / ViewportScale;

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PickupPromptText->Slot))
		{
			CanvasSlot->SetPosition(WidgetPos);
		}
	}
	else
	{
		PickupPromptText->SetVisibility(ESlateVisibility::Hidden);
	}
}
