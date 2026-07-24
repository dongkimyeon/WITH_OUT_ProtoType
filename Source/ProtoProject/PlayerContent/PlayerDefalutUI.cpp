#include "PlayerDefalutUI.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Pawn.h"

void UPlayerDefalutUI::NativeConstruct()
{
	Super::NativeConstruct();

	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		PlayerStatusComponent = OwningPawn->FindComponentByClass<UPlayerStatusComponent>();
	}
}

void UPlayerDefalutUI::UpdateStatUI()
{
	if (!PlayerStatusComponent) return;

	auto SetStat = [](UProgressBar* Bar, UTextBlock* Text, float Current, float Max)
	{
		if (Bar) Bar->SetPercent(Max > 0.0f ? Current / Max : 0.0f);
		if (Text) Text->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(Current), FMath::RoundToInt(Max))));
	};

	SetStat(HealthProgressBar, HealthText, PlayerStatusComponent->GetHealth(), PlayerStatusComponent->GetMaxHealth());
	SetStat(HungerProgressBar, HungerText, PlayerStatusComponent->GetHunger(), PlayerStatusComponent->GetMaxHunger());
	SetStat(ThirstProgressBar, ThirstText, PlayerStatusComponent->GetThirst(), PlayerStatusComponent->GetMaxThirst());
	SetStat(InfectionProgressBar, InfectionText, PlayerStatusComponent->GetInfection(), PlayerStatusComponent->GetMaxInfection());

	if (StaminaProgressBar)
	{
		const float MaxStamina = PlayerStatusComponent->GetMaxStamina();
		StaminaProgressBar->SetPercent(MaxStamina > 0.0f ? PlayerStatusComponent->GetStamina() / MaxStamina : 0.0f);
	}
	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(PlayerStatusComponent->GetStamina()))));
	}
}

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

	UpdateStatUI();

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
