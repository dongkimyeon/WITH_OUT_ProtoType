// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusComponent.h"
#include "PlayerDefalutUI.generated.h"

class UTextBlock;
class ADropItem;

UCLASS()
class PROTOPROJECT_API UPlayerDefalutUI : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowPickupPrompt(ADropItem* Item);
	void HidePickupPrompt();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PickupPromptText;

	UPlayerStatusComponent* PlayerStatusComponent;

	ADropItem* FocusedItem = nullptr;
};
