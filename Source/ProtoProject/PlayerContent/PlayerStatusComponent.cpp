// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerStatusComponent.h"

// Sets default values for this component's properties
UPlayerStatusComponent::UPlayerStatusComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


void UPlayerStatusComponent::SetMaxHealth(float newMaxHealth)
{
	MaxHealth = newMaxHealth;
	Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UPlayerStatusComponent::SetHealth(float newHealth)
{
	const float Clamped = FMath::Clamp(newHealth, 0.0f, MaxHealth);
	if (FMath::IsNearlyEqual(Health, Clamped)) return;

	Health = Clamped;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UPlayerStatusComponent::SetInfection(float newInfection)
{
	const float Clamped = FMath::Clamp(newInfection, 0.0f, MaxInfection);
	if (FMath::IsNearlyEqual(Infection, Clamped)) return;

	Infection = Clamped;
	OnInfectionChanged.Broadcast(Infection, MaxInfection);
}

void UPlayerStatusComponent::SetHunger(float newHunger)
{
	const float Clamped = FMath::Clamp(newHunger, 0.0f, MaxHunger);
	if (FMath::IsNearlyEqual(Hunger, Clamped)) return;

	Hunger = Clamped;
	OnHungerChanged.Broadcast(Hunger, MaxHunger);
}

void UPlayerStatusComponent::SetThirst(float newThirst)
{
	const float Clamped = FMath::Clamp(newThirst, 0.0f, MaxThirst);
	if (FMath::IsNearlyEqual(Thirst, Clamped)) return;

	Thirst = Clamped;
	OnThirstChanged.Broadcast(Thirst, MaxThirst);
}

void UPlayerStatusComponent::SetStamina(float newStamina)
{
	const float Clamped = FMath::Clamp(newStamina, 0.0f, MaxStamina);
	if (FMath::IsNearlyEqual(Stamina, Clamped)) return;

	Stamina = Clamped;
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

// Called when the game starts
void UPlayerStatusComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UPlayerStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

