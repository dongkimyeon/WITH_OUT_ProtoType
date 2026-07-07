// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include  "Components/TextBlock.h"
#include "PlayerStatWidget.generated.h"

/**
 * 
 */
UCLASS()
class PROTO_TYPE_API UPlayerStatWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Health;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Infection;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Hunger;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Thirst;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* PB_Stamina;
	
	UFUNCTION(BlueprintCallable)
	void UpdateStats(float Health, float MaxHealth,
					 float Infection,
					 float Hunger,
					 float Thirst,
					 float Stamina);
protected:
	virtual void NativeConstruct() override;
};
