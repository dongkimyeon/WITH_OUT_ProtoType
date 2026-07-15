// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerDefalutUI.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Item/DropItem.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UPlayerDefalutUI::AddPickupPrompt(ADropItem* Item)
{
	if (!PromptCanvas || PromptMap.Contains(Item)) return;

	UTextBlock* NewText = NewObject<UTextBlock>(this);
	NewText->SetText(FText::FromString(TEXT("F  습득")));

	UCanvasPanelSlot* CanvasSlot = PromptCanvas->AddChildToCanvas(NewText);
	CanvasSlot->SetAutoSize(true);

	PromptMap.Add(Item, NewText);
}

void UPlayerDefalutUI::RemovePickupPrompt(ADropItem* Item)
{
	UTextBlock** Found = PromptMap.Find(Item);
	if (!Found) return;

	(*Found)->RemoveFromParent();
	PromptMap.Remove(Item);
}

void UPlayerDefalutUI::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	for (auto& Pair : PromptMap)
	{
		if (!IsValid(Pair.Key) || !Pair.Value) continue;

		FVector2D ScreenPos;
		bool bOnScreen = GetOwningPlayer()->ProjectWorldLocationToScreen(
			Pair.Key->GetActorLocation() + FVector(0.f, 0.f, 100.f), ScreenPos
		);

		if (bOnScreen)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Pair.Value->Slot))
			{
				float DPIScale = UWidgetLayoutLibrary::GetViewportScale(GetWorld());
				FVector2D ViewportPos = ScreenPos / DPIScale;
				FVector2D TextSize = Pair.Value->GetDesiredSize();
				CanvasSlot->SetPosition(ViewportPos - TextSize * 0.5f);
			}
			Pair.Value->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			Pair.Value->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
