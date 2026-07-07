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
}

void UPlayerStatusComponent::SetHealth(float newHealth)
{
	Health = newHealth;
}

void UPlayerStatusComponent::SetInfection(float newInfection)
{
	Infection = newInfection;
}

void UPlayerStatusComponent::SetHunger(float newHunger)
{
	Hunger = newHunger;
}

void UPlayerStatusComponent::SetThirst(float newThirst)
{
	Thirst = newThirst;
}

void UPlayerStatusComponent::SetStamina(float newStamina)
{
	Stamina = newStamina;
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

