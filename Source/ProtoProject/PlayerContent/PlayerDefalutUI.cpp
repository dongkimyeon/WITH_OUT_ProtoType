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

	if (PlayerStatusComponent)
	{
		PlayerStatusComponent->OnHealthChanged.AddDynamic(this, &UPlayerDefalutUI::HandleHealthChanged);
		PlayerStatusComponent->OnHungerChanged.AddDynamic(this, &UPlayerDefalutUI::HandleHungerChanged);
		PlayerStatusComponent->OnThirstChanged.AddDynamic(this, &UPlayerDefalutUI::HandleThirstChanged);
		PlayerStatusComponent->OnInfectionChanged.AddDynamic(this, &UPlayerDefalutUI::HandleInfectionChanged);
		PlayerStatusComponent->OnStaminaChanged.AddDynamic(this, &UPlayerDefalutUI::HandleStaminaChanged);

		// 델리게이트는 값이 바뀔 때만 브로드캐스트되므로, 위젯을 현재 값으로 한 번 초기화해준다.
		HandleHealthChanged(PlayerStatusComponent->GetHealth(), PlayerStatusComponent->GetMaxHealth());
		HandleHungerChanged(PlayerStatusComponent->GetHunger(), PlayerStatusComponent->GetMaxHunger());
		HandleThirstChanged(PlayerStatusComponent->GetThirst(), PlayerStatusComponent->GetMaxThirst());
		HandleInfectionChanged(PlayerStatusComponent->GetInfection(), PlayerStatusComponent->GetMaxInfection());
		HandleStaminaChanged(PlayerStatusComponent->GetStamina(), PlayerStatusComponent->GetMaxStamina());
	}
}

void UPlayerDefalutUI::HandleHealthChanged(float NewValue, float MaxValue)
{
	if (HealthProgressBar) HealthProgressBar->SetPercent(MaxValue > 0.0f ? NewValue / MaxValue : 0.0f);
	if (HealthText) HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(NewValue), FMath::RoundToInt(MaxValue))));
}

void UPlayerDefalutUI::HandleHungerChanged(float NewValue, float MaxValue)
{
	if (HungerProgressBar) HungerProgressBar->SetPercent(MaxValue > 0.0f ? NewValue / MaxValue : 0.0f);
	if (HungerText) HungerText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(NewValue), FMath::RoundToInt(MaxValue))));
}

void UPlayerDefalutUI::HandleThirstChanged(float NewValue, float MaxValue)
{
	if (ThirstProgressBar) ThirstProgressBar->SetPercent(MaxValue > 0.0f ? NewValue / MaxValue : 0.0f);
	if (ThirstText) ThirstText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(NewValue), FMath::RoundToInt(MaxValue))));
}

void UPlayerDefalutUI::HandleInfectionChanged(float NewValue, float MaxValue)
{
	if (InfectionProgressBar) InfectionProgressBar->SetPercent(MaxValue > 0.0f ? NewValue / MaxValue : 0.0f);
	if (InfectionText) InfectionText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::RoundToInt(NewValue), FMath::RoundToInt(MaxValue))));
}

void UPlayerDefalutUI::HandleStaminaChanged(float NewValue, float MaxValue)
{
	if (StaminaProgressBar) StaminaProgressBar->SetPercent(MaxValue > 0.0f ? NewValue / MaxValue : 0.0f);
	if (StaminaText) StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(NewValue))));
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
