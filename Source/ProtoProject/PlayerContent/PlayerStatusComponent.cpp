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

	// 체력이 0이 되면 사망을 1회 알린다. IsNearlyEqual early-return이 이후 재발동을 막는다.
	if (Health <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		OnPlayerDied.Broadcast();
	}
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

	// 레이드 밖(허브)이거나 이미 사망했으면 생존 스탯을 시뮬레이션하지 않는다.
	if (bIsDead || !bSurvivalSimActive)
	{
		return;
	}

	// 감염도 상승. 제한시간 경과 후에는 폭주 속도.
	const float InfectionRate = bInfectionOverdue ? InfectionOverdueRatePerSecond : InfectionRatePerSecond;
	SetInfection(Infection + InfectionRate * DeltaTime);

	// 감염도가 최대면 체력을 깎는다.
	if (Infection >= MaxInfection - KINDA_SMALL_NUMBER)
	{
		const float InfectionDamage = bInfectionOverdue ? InfectionOverdueDamagePerSecond : InfectionDamagePerSecond;
		SetHealth(Health - InfectionDamage * DeltaTime);
	}

	// 배고픔 / 목마름 감소.
	SetHunger(Hunger - HungerDrainPerSecond * DeltaTime);
	SetThirst(Thirst - ThirstDrainPerSecond * DeltaTime);

	// 배고픔이 바닥나면 굶주림으로 체력을 깎는다.
	if (Hunger <= KINDA_SMALL_NUMBER)
	{
		SetHealth(Health - StarvationDamagePerSecond * DeltaTime);
	}
}

