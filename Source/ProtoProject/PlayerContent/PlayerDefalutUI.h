#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusComponent.h"
#include "PlayerDefalutUI.generated.h"

class UTextBlock;
class UCanvasPanel;
class UProgressBar;

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
	void UpdateStatUI();

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

	UPlayerStatusComponent* PlayerStatusComponent;

	TMap<AActor*, UTextBlock*> PromptMap;
};
