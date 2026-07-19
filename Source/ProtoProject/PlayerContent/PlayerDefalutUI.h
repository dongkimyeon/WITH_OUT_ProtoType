// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusComponent.h"
#include "PlayerDefalutUI.generated.h"

class UTextBlock;
class UCanvasPanel;
class ADropItem;
class AStorageContainer;

UCLASS()
class PROTOPROJECT_API UPlayerDefalutUI : public UUserWidget
{
	GENERATED_BODY()

public:
	void AddPickupPrompt(ADropItem* Item);
	void RemovePickupPrompt(ADropItem* Item);

	void AddContainerPrompt(AStorageContainer* Container);
	void RemoveContainerPrompt(AStorageContainer* Container);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* PromptCanvas;

	UPlayerStatusComponent* PlayerStatusComponent;

	TMap<ADropItem*, UTextBlock*> PromptMap;
	TMap<AStorageContainer*, UTextBlock*> ContainerPromptMap;
};
