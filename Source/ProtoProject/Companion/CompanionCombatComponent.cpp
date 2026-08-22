// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionCombatComponent.h"
#include "../PlayerContent/weapon/WeaponBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	// PIE 콘솔(~)에서 "companion.AttackRange 3000"처럼 입력하면 모든 Companion의 공격 사거리를
	// 즉시 덮어쓴다. 음수(기본값)면 각 인스턴스의 AttackRange 프로퍼티를 그대로 사용한다.
	TAutoConsoleVariable<float> CVarCompanionAttackRange(
		TEXT("companion.AttackRange"),
		-1.0f,
		TEXT("0 이상이면 모든 Companion의 AttackRange를 이 값으로 덮어쓴다(디버그용, PIE 중 실시간 반영)."),
		ECVF_Cheat);
}

UCompanionCombatComponent::UCompanionCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCompanionCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	if (!WeaponClass)
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	EquippedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, OwnerCharacter->GetActorTransform(), SpawnParams);
	if (!EquippedWeapon)
	{
		return;
	}

	EquippedWeapon->SetOwner(OwnerCharacter);
	EquippedWeapon->SetActorEnableCollision(false);

	if (OwnerCharacter->GetMesh()->DoesSocketExist(WeaponSocketName))
	{
		const FAttachmentTransformRules AttachRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepRelative,
			true);
		EquippedWeapon->AttachToComponent(OwnerCharacter->GetMesh(), AttachRules, WeaponSocketName);
	}
}

bool UCompanionCombatComponent::CanAttack() const
{
	if (bIsDead || !EquippedWeapon)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastAttackTime >= AttackCooldown;
}

float UCompanionCombatComponent::GetEffectiveAttackRange() const
{
	const float Override = CVarCompanionAttackRange.GetValueOnGameThread();
	return Override >= 0.0f ? Override : AttackRange;
}

void UCompanionCombatComponent::FireWeapon()
{
	if (!CanAttack())
	{
		return;
	}

	LastAttackTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastAttackTime;
	EquippedWeapon->Fire();
}

void UCompanionCombatComponent::TakeCompanionDamage(float DamageAmount)
{
	if (bIsDead || DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	if (CurrentHealth <= 0.0f)
	{
		Die();
	}
}

void UCompanionCombatComponent::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnCompanionDied.Broadcast(GetOwner());
}
