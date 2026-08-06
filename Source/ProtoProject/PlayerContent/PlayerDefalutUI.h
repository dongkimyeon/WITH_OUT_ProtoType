#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusComponent.h"
#include "PlayerDefalutUI.generated.h"

class UTextBlock;
class UCanvasPanel;
class UProgressBar;
class UQuickSlotHudWidget;

UCLASS()
class PROTOPROJECT_API UPlayerDefalutUI : public UUserWidget
{
	GENERATED_BODY()

public:
	void AddInteractPrompt(AActor* Actor, FText Text);
	void RemoveInteractPrompt(AActor* Actor);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleHungerChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleThirstChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleInfectionChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleStaminaChanged(float NewValue, float MaxValue);

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* PromptCanvas;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HungerProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* ThirstProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* InfectionProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* StaminaProgressBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealthText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HungerText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ThirstText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* InfectionText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StaminaText;

	// 4번 키를 짧게 탭했을 때 바로 사용될 아이템을 우측 하단에 표시
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotHudWidget* QuickSlotHud;

	UPlayerStatusComponent* PlayerStatusComponent;

	TMap<AActor*, UTextBlock*> PromptMap;
};
