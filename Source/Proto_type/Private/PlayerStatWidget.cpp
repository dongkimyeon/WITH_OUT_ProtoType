// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerStatWidget.h"

void UPlayerStatWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateStats(100.f, 100.f, 0.f, 100.f, 100.f, 100.f);
}

void UPlayerStatWidget::UpdateStats(float Health, float MaxHealth,
									float Infection,
									float Hunger,
									float Thirst,
									float Stamina)
{
	if (PB_Health)    PB_Health->SetPercent(Health / MaxHealth);
	if (PB_Infection) PB_Infection->SetPercent(Infection / 100.f);
	if (PB_Hunger)    PB_Hunger->SetPercent(Hunger / 100.f);
	if (PB_Thirst)    PB_Thirst->SetPercent(Thirst / 100.f);
	if (PB_Stamina)   PB_Stamina->SetPercent(Stamina / 100.f);
}