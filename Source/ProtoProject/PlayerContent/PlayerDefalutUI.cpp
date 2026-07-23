// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerDefalutUI.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Item/DropItem.h"
#include "Item/StorageContainer.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UPlayerDefalutUI::AddPickupPrompt(ADropItem* Item)
{
	if (!PromptCanvas || PromptMap.Contains(Item)) return;

	UTextBlock* NewText = NewObject<UTextBlock>(this);
	NewText->SetText(FText::FromString(TEXT("F 습득")));

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

void UPlayerDefalutUI::AddContainerPrompt(AStorageContainer* Container)
{
	if (!PromptCanvas || ContainerPromptMap.Contains(Container)) return;

	UTextBlock* NewText = NewObject<UTextBlock>(this);
	NewText->SetText(FText::FromString(TEXT("F  열기")));

	UCanvasPanelSlot* CanvasSlot = PromptCanvas->AddChildToCanvas(NewText);
	CanvasSlot->SetAutoSize(true);

	ContainerPromptMap.Add(Container, NewText);
}

void UPlayerDefalutUI::RemoveContainerPrompt(AStorageContainer* Container)
{
	UTextBlock** Found = ContainerPromptMap.Find(Container);
	if (!Found) return;

	(*Found)->RemoveFromParent();
	ContainerPromptMap.Remove(Container);
}

void UPlayerDefalutUI::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	auto UpdatePrompt = [&](AActor* Actor, UTextBlock* Text)
	{
		if (!IsValid(Actor) || !Text) return;

		FVector2D ScreenPos;
		bool bOnScreen = GetOwningPlayer()->ProjectWorldLocationToScreen(
			Actor->GetActorLocation() + FVector(0.f, 0.f, 100.f), ScreenPos
		);

		if (bOnScreen)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Text->Slot))
			{
				float DPIScale = UWidgetLayoutLibrary::GetViewportScale(GetWorld());
				FVector2D ViewportPos = ScreenPos / DPIScale;
				FVector2D TextSize = Text->GetDesiredSize();
				CanvasSlot->SetPosition(ViewportPos - TextSize * 0.5f);
			}
			Text->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			Text->SetVisibility(ESlateVisibility::Hidden);
		}
	};

	for (auto& Pair : PromptMap)         UpdatePrompt(Pair.Key, Pair.Value);
	for (auto& Pair : ContainerPromptMap) UpdatePrompt(Pair.Key, Pair.Value);
}
