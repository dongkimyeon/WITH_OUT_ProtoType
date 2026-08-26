// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionCombatComponent.h"
#include "CompanionNPC.h"
#include "../PlayerContent/weapon/WeaponBase.h"
#include "../PlayerContent/Inventory/InventoryGridComponent.h"
#include "../PlayerContent/Item/WeaponItemData.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"

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
}

void UCompanionCombatComponent::EquipWeaponFromInventory(UInventoryGridComponent* Inventory)
{
	if (!Inventory)
	{
		return;
	}

	const FInventoryItemInstance* WeaponEntry = Inventory->Items.FindByPredicate(
		[](const FInventoryItemInstance& Item) { return Item.ItemData && Item.ItemData->IsA<UWeaponItemData>(); });

	if (!WeaponEntry)
	{
		if (EquippedWeapon)
		{
			EquippedWeapon->Destroy();
			EquippedWeapon = nullptr;
		}
		return;
	}

	const UWeaponItemData* WeaponData = Cast<UWeaponItemData>(WeaponEntry->ItemData);
	if (!WeaponData || !WeaponData->WeaponActorClass)
	{
		return;
	}

	if (EquippedWeapon && EquippedWeapon->GetClass() == WeaponData->WeaponActorClass)
	{
		return;
	}

	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
		EquippedWeapon = nullptr;
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

	EquippedWeapon = World->SpawnActor<AWeaponBase>(WeaponData->WeaponActorClass, OwnerCharacter->GetActorTransform(), SpawnParams);
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
	const UWorld* World = GetWorld();
	const ACompanionNPC* CompanionOwner = Cast<ACompanionNPC>(GetOwner());

	const bool bHasEquippedWeapon = IsValid(EquippedWeapon);
	const bool bWeaponCanFire = bHasEquippedWeapon && EquippedWeapon->CanFire();
	const bool bOwnerValid = IsValid(CompanionOwner);
	const bool bOwnerHasWeapon = bOwnerValid && CompanionOwner->bHasWeapon;
	const bool bWeaponTypeValid = bOwnerValid && CompanionOwner->CurrentWeaponType != EWeaponType::None;
	const bool bWeaponInHand = bOwnerValid && CompanionOwner->SwappingAlpha;
	const bool bAiming = bOwnerValid && CompanionOwner->bIsAiming;
	const bool bNotReloading = bOwnerValid && !CompanionOwner->bIsReloading;
	const bool bCooldownReady = World && World->GetTimeSeconds() - LastAttackTime >= AttackCooldown;
	const bool bCanAttack = !bIsDead
		&& bHasEquippedWeapon
		&& bWeaponCanFire
		&& bOwnerValid
		&& bOwnerHasWeapon
		&& bWeaponTypeValid
		&& bWeaponInHand
		&& bAiming
		&& bNotReloading
		&& bCooldownReady;

#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		const float RemainingCooldown = World ? FMath::Max(0.0f, AttackCooldown - (World->GetTimeSeconds() - LastAttackTime)) : AttackCooldown;
		const int32 WeaponTypeValue = bOwnerValid ? static_cast<int32>(CompanionOwner->CurrentWeaponType) : -1;
		const FString DebugText = FString::Printf(
			TEXT("Companion CanAttack: %s\nDead:%s Equipped:%s WeaponCanFire:%s Owner:%s HasWeapon:%s WeaponType:%d ValidType:%s InHand(SwappingAlpha):%s Aiming:%s NotReloading:%s Cooldown:%s(%.2f)"),
			bCanAttack ? TEXT("TRUE") : TEXT("FALSE"),
			bIsDead ? TEXT("TRUE") : TEXT("FALSE"),
			bHasEquippedWeapon ? TEXT("TRUE") : TEXT("FALSE"),
			bWeaponCanFire ? TEXT("TRUE") : TEXT("FALSE"),
			bOwnerValid ? TEXT("TRUE") : TEXT("FALSE"),
			bOwnerHasWeapon ? TEXT("TRUE") : TEXT("FALSE"),
			WeaponTypeValue,
			bWeaponTypeValid ? TEXT("TRUE") : TEXT("FALSE"),
			bWeaponInHand ? TEXT("TRUE") : TEXT("FALSE"),
			bAiming ? TEXT("TRUE") : TEXT("FALSE"),
			bNotReloading ? TEXT("TRUE") : TEXT("FALSE"),
			bCooldownReady ? TEXT("TRUE") : TEXT("FALSE"),
			RemainingCooldown);
		GEngine->AddOnScreenDebugMessage(92010, 0.0f, bCanAttack ? FColor::Green : FColor::Red, DebugText);
	}
#endif

	return bCanAttack;
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
