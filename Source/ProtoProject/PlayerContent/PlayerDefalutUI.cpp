#include "PlayerDefalutUI.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UPlayerDefalutUI::AddInteractPrompt(AActor* Actor, FText Text)
{
	if (!PromptCanvas || !Actor || PromptMap.Contains(Actor)) return;

	UTextBlock* NewText = NewObject<UTextBlock>(this);
	NewText->SetText(Text);

	UCanvasPanelSlot* CanvasSlot = PromptCanvas->AddChildToCanvas(NewText);
	CanvasSlot->SetAutoSize(true);

	PromptMap.Add(Actor, NewText);
}

void UPlayerDefalutUI::RemoveInteractPrompt(AActor* Actor)
{
	UTextBlock** Found = PromptMap.Find(Actor);
	if (!Found) return;

	(*Found)->RemoveFromParent();
	PromptMap.Remove(Actor);
}

void UPlayerDefalutUI::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	for (auto& Pair : PromptMap)
	{
		AActor* Actor = Pair.Key;
		UTextBlock* Text = Pair.Value;
		if (!IsValid(Actor) || !Text) continue;

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
	}
}
