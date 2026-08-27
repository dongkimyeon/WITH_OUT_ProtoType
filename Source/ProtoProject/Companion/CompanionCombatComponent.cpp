// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionCombatComponent.h"
#include "CompanionNPC.h"
#include "CompanionAIComponent.h"
#include "../PlayerContent/weapon/WeaponBase.h"
#include "../PlayerContent/Inventory/InventoryGridComponent.h"
#include "../PlayerContent/Item/WeaponItemData.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"

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
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> UpperBodyMontageFinder(TEXT("/Game/Blueprint/AM_Player_Upper.AM_Player_Upper"));
	if (UpperBodyMontageFinder.Succeeded())
	{
		CompanionUpperBodyMontage = UpperBodyMontageFinder.Object;
	}
}

void UCompanionCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
			{
				AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UCompanionCombatComponent::HandleMontageNotifyBegin);
				AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &UCompanionCombatComponent::HandleMontageEnded);
			}
		}
	}
}

void UCompanionCombatComponent::EquipWeaponFromInventory(UInventoryGridComponent* Inventory)
{
	EquipWeaponFromInventoryIndex(Inventory, 0);
}

bool UCompanionCombatComponent::EquipWeaponFromInventoryIndex(UInventoryGridComponent* Inventory, int32 WeaponIndex)
{
	if (!Inventory || WeaponIndex < 0)
	{
		return false;
	}

	int32 CurrentWeaponIndex = 0;
	for (const FInventoryItemInstance& Item : Inventory->Items)
	{
		const UWeaponItemData* WeaponData = Cast<UWeaponItemData>(Item.ItemData);
		if (!WeaponData)
		{
			continue;
		}

		if (CurrentWeaponIndex == WeaponIndex)
		{
			if (!WeaponData->WeaponActorClass)
			{
				return false;
			}

			const AWeaponBase* WeaponDefault = WeaponData->WeaponActorClass->GetDefaultObject<AWeaponBase>();
			const EWeaponType TargetWeaponType = WeaponDefault ? WeaponDefault->WeaponType : EWeaponType::None;
			if (EquippedWeapon && EquippedWeapon->GetClass() == WeaponData->WeaponActorClass)
			{
				return true;
			}

			StartWeaponSwap(TargetWeaponType, WeaponData->WeaponActorClass);
			return true;
		}

		++CurrentWeaponIndex;
	}

	return false;
}

void UCompanionCombatComponent::HolsterWeapon()
{
	if (!EquippedWeapon || bIsSwappingWeapon)
	{
		return;
	}

	StartWeaponSwap(EWeaponType::None, nullptr);
}

bool UCompanionCombatComponent::SpawnAndAttachWeapon(TSubclassOf<AWeaponBase> WeaponClass)
{
	if (!WeaponClass)
	{
		return false;
	}

	if (EquippedWeapon && EquippedWeapon->GetClass() == WeaponClass)
	{
		return true;
	}

	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
		EquippedWeapon = nullptr;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	EquippedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, OwnerCharacter->GetActorTransform(), SpawnParams);
	if (!EquippedWeapon)
	{
		return false;
	}

	EquippedWeapon->SetOwner(OwnerCharacter);
	EquippedWeapon->SetActorEnableCollision(false);

	const FName AttachSocketName = EquippedWeapon->WeaponType == EWeaponType::Pistol ? PistolWeaponSocketName : WeaponSocketName;
	if (OwnerCharacter->GetMesh()->DoesSocketExist(AttachSocketName))
	{
		const FAttachmentTransformRules AttachRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepRelative,
			true);
		EquippedWeapon->AttachToComponent(OwnerCharacter->GetMesh(), AttachRules, AttachSocketName);
	}

	return true;
}
void UCompanionCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsSwappingWeapon)
	{
		return;
	}

	SwapRemainingTime = FMath::Max(0.0f, SwapRemainingTime - DeltaTime);
	if (SwapRemainingTime <= 0.0f)
	{
		FinishWeaponSwap();
	}
}

bool UCompanionCombatComponent::ShouldUseLeftHandIK() const
{
	return IsValid(EquippedWeapon) && !bIsSwappingWeapon && !bIsReloadingWeapon;
}

void UCompanionCombatComponent::StartWeaponSwap(EWeaponType TargetWeaponType, TSubclassOf<AWeaponBase> TargetWeaponClass)
{
	if (bIsDead || bIsSwappingWeapon || bIsReloadingWeapon)
	{
		return;
	}

	const EWeaponType PreviousWeaponType = EquippedWeapon ? EquippedWeapon->WeaponType : EWeaponType::None;
	if (TargetWeaponType != EWeaponType::None && !TargetWeaponClass)
	{
		return;
	}

	SwapFromWeaponType = PreviousWeaponType;
	PendingWeaponType = TargetWeaponType;
	PendingWeaponClass = TargetWeaponClass;
	bPendingHolster = TargetWeaponType == EWeaponType::None;
	bIsSwappingWeapon = true;

	const AWeaponBase* TargetDefault = TargetWeaponClass ? TargetWeaponClass->GetDefaultObject<AWeaponBase>() : nullptr;
	const float SwapTime = bPendingHolster
		? (EquippedWeapon ? EquippedWeapon->UnequipSwapTime : 0.0f)
		: (TargetDefault ? TargetDefault->EquipSwapTime : 0.0f);
	SwapRemainingTime = FMath::Max(0.0f, SwapTime);

	UAnimMontage* DrawMontage = CompanionUpperBodyMontage ? CompanionUpperBodyMontage : WeaponSwapMontage;
	UAnimMontage* HolsterMontage = WeaponSwapMontage ? WeaponSwapMontage : CompanionUpperBodyMontage;
	if (bPendingHolster)
	{
		if (PreviousWeaponType == EWeaponType::Rifle)
		{
			PlayCompanionMontage(HolsterMontage, RifleToHandSectionName);
		}
		else if (PreviousWeaponType == EWeaponType::Pistol)
		{
			PlayCompanionMontage(HolsterMontage, PistolToHandSectionName);
		}
	}
	else if (TargetWeaponType == EWeaponType::Rifle)
	{
		PlayCompanionMontage(DrawMontage, HandToRifleSectionName);
	}
	else if (TargetWeaponType == EWeaponType::Pistol)
	{
		PlayCompanionMontage(DrawMontage, HandToPistolSectionName);
	}

	if (SwapRemainingTime <= 0.0f)
	{
		FinishWeaponSwap();
	}
}

void UCompanionCombatComponent::FinishWeaponSwap()
{
	if (bPendingHolster)
	{
		if (EquippedWeapon)
		{
			EquippedWeapon->Destroy();
			EquippedWeapon = nullptr;
		}
	}
	else
	{
		SpawnAndAttachWeapon(PendingWeaponClass);
	}

	PendingWeaponClass = nullptr;
	PendingWeaponType = EWeaponType::None;
	SwapFromWeaponType = EWeaponType::None;
	SwapRemainingTime = 0.0f;
	bPendingHolster = false;
	bIsSwappingWeapon = false;
}

float UCompanionCombatComponent::PlayCompanionMontage(UAnimMontage* Montage, FName SectionName)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !Montage)
	{
		return 0.0f;
	}

	return OwnerCharacter->PlayAnimMontage(Montage, 1.0f, SectionName);
}
FVector UCompanionCombatComponent::GetLeftHandJointTargetForEquippedWeapon() const
{
	if (!EquippedWeapon)
	{
		return FVector(1000.0f, -2000.0f, 0.0f);
	}

	switch (EquippedWeapon->WeaponType)
	{
	case EWeaponType::Pistol:
		return CompanionPistolLeftHandJointTarget;
	case EWeaponType::Rifle:
		return CompanionRifleLeftHandJointTarget;
	default:
		return EquippedWeapon->LeftHandJointTarget;
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
	if (EquippedWeapon && !EquippedWeapon->CanFire() && EquippedWeapon->CanReload() && !bIsReloadingWeapon && !bIsSwappingWeapon)
	{
		ReloadWeapon();
		return;
	}

	if (!CanAttack())
	{
		return;
	}

	LastAttackTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastAttackTime;
	EquippedWeapon->Fire();
}

void UCompanionCombatComponent::ReloadWeapon()
{
	if (bIsDead || bIsReloadingWeapon || bIsSwappingWeapon || !EquippedWeapon || !EquippedWeapon->CanReload())
	{
		return;
	}

	if (ACompanionNPC* CompanionOwner = Cast<ACompanionNPC>(GetOwner()))
	{
		if (CompanionOwner->AIComponent)
		{
			CompanionOwner->AIComponent->ClearAimingRequest();
		}
	}

	bIsReloadingWeapon = true;

	const FName ReloadSectionName = EquippedWeapon->WeaponType == EWeaponType::Pistol ? PistolReloadSectionName : RifleReloadSectionName;
	const float MontageLength = PlayCompanionMontage(CompanionUpperBodyMontage, ReloadSectionName);
	if (MontageLength <= 0.0f)
	{
		EquippedWeapon->ReloadMagazine();
		bIsReloadingWeapon = false;
	}
}

void UCompanionCombatComponent::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (!EquippedWeapon)
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* CharacterMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;

	static const FName AmmoAttachNotifyName(TEXT("AmmoAttach"));
	static const FName AmmoDetachNotifyName(TEXT("AmmoDetach"));
	static const FName NewAmmoNotifyName(TEXT("NewAmmo"));
	static const FName NewAmmoAttachNotifyName(TEXT("NewAmmoAttach"));
	static const FName NewAmmoDetachNotifyName(TEXT("NewAmmoDetach"));

	if (NotifyName == AmmoAttachNotifyName)
	{
		EquippedWeapon->ReloadAmmoAttach(CharacterMesh);
	}
	else if (NotifyName == AmmoDetachNotifyName)
	{
		EquippedWeapon->ReloadAmmoDetach();
	}
	else if (NotifyName == NewAmmoNotifyName)
	{
		EquippedWeapon->ReloadNewAmmo(CharacterMesh);
	}
	else if (NotifyName == NewAmmoAttachNotifyName)
	{
		if (EquippedWeapon->WeaponType == EWeaponType::Pistol)
		{
			EquippedWeapon->ReloadNewAmmo(CharacterMesh);
		}
		else
		{
			EquippedWeapon->ReloadNewAmmoAttach();
		}
	}
	else if (NotifyName == NewAmmoDetachNotifyName)
	{
		EquippedWeapon->ReloadNewAmmoAttach();
	}
}

void UCompanionCombatComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != CompanionUpperBodyMontage || !bIsReloadingWeapon)
	{
		return;
	}

	if (!bInterrupted && EquippedWeapon)
	{
		EquippedWeapon->ReloadMagazine();
	}

	bIsReloadingWeapon = false;
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









