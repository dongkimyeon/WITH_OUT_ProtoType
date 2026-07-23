#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusComponent.h"
#include "PlayerDefalutUI.generated.h"

class UTextBlock;
class UCanvasPanel;

UCLASS()
class PROTOPROJECT_API UPlayerDefalutUI : public UUserWidget
{
	GENERATED_BODY()

public:
	void AddInteractPrompt(AActor* Actor, FText Text);
	void RemoveInteractPrompt(AActor* Actor);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* PromptCanvas;

	UPlayerStatusComponent* PlayerStatusComponent;

	TMap<AActor*, UTextBlock*> PromptMap;
};
