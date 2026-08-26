// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "CompanionCombatComponent.generated.h"

class AWeaponBase;
class UInventoryGridComponent;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionDied, AActor*, Companion);

struct FBranchingPointNotifyPayload;

// 동료 NPC의 무기 장착/발사와 자신의 체력을 담당한다. 데미지 적용 자체는 AWeaponBase::Fire()가
// 히트한 AEnemyBase에 직접 TakeEnemyDamage를 호출하므로(AK47.cpp/Pistol.cpp), 여기서는
// 무기 스폰/부착과 발사 트리거, Companion 자신의 피격 처리만 담당한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionCombatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	FName WeaponSocketName = TEXT("WeaponSocket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Combat")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float AttackRange = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Combat")
	float AttackCooldown = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FVector CompanionRifleLeftHandJointTarget = FVector(1000.0f, -2000.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FVector CompanionPistolLeftHandJointTarget = FVector(20.0f, 1.0f, -1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	UAnimMontage* WeaponSwapMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	UAnimMontage* CompanionUpperBodyMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FName RifleToHandSectionName = TEXT("rifletohand");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FName HandToRifleSectionName = TEXT("handtorifle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FName PistolToHandSectionName = TEXT("pistoltohand");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FName HandToPistolSectionName = TEXT("handtopistol");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FName RifleReloadSectionName = TEXT("RifleReload");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Animation")
	FName PistolReloadSectionName = TEXT("pistolreload");
	UPROPERTY(BlueprintAssignable, Category = "Companion|Combat")
	FOnCompanionDied OnCompanionDied;

	UFUNCTION(BlueprintPure, Category = "Companion|Combat")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Companion|Combat")
	bool CanAttack() const;

	// companion.AttackRange 콘솔 변수로 덮어써진 경우 그 값을, 아니면 AttackRange를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Companion|Combat")
	float GetEffectiveAttackRange() const;

	// 쿨다운을 소모하고 장착 무기를 발사한다. 호출 전에 CanAttack()/사거리/사선 확인은 호출자 책임.
	UFUNCTION(BlueprintCallable, Category = "Companion|Combat")
	void FireWeapon();

	UFUNCTION(BlueprintCallable, Category = "Companion|Combat")
	void TakeCompanionDamage(float DamageAmount);

	// 인벤토리에 무기 아이템(UWeaponItemData)이 있으면 그걸 장착하고, 없으면 맨손으로 만든다.
	// 이미 같은 무기를 들고 있으면 아무것도 하지 않는다(재스폰 깜빡임 방지). 인벤토리가 바뀔 때마다
	// 호출되므로 여러 번 불려도 안전해야 한다.
	UFUNCTION(BlueprintCallable, Category = "Companion|Combat")
	void EquipWeaponFromInventory(UInventoryGridComponent* Inventory);

	UFUNCTION(BlueprintCallable, Category = "Companion|Combat")
	bool EquipWeaponFromInventoryIndex(UInventoryGridComponent* Inventory, int32 WeaponIndex);

	UFUNCTION(BlueprintCallable, Category = "Companion|Combat")
	void HolsterWeapon();

	UFUNCTION(BlueprintCallable, Category = "Companion|Combat")
	void ReloadWeapon();

	UFUNCTION(BlueprintPure, Category = "Companion|Animation")
	bool IsSwappingWeapon() const { return bIsSwappingWeapon; }

	UFUNCTION(BlueprintPure, Category = "Companion|Animation")
	bool IsReloadingWeapon() const { return bIsReloadingWeapon; }

	UFUNCTION(BlueprintPure, Category = "Companion|Animation")
	bool ShouldUseLeftHandIK() const;

	UFUNCTION(BlueprintPure, Category = "Companion|Combat")
	AWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Companion|Animation")
	FVector GetLeftHandJointTargetForEquippedWeapon() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<AWeaponBase> EquippedWeapon;

	float LastAttackTime = -999.0f;
	bool bIsDead = false;
	bool bIsSwappingWeapon = false;
	bool bIsReloadingWeapon = false;
	float SwapRemainingTime = 0.0f;
	EWeaponType SwapFromWeaponType = EWeaponType::None;
	EWeaponType PendingWeaponType = EWeaponType::None;
	TSubclassOf<AWeaponBase> PendingWeaponClass;
	bool bPendingHolster = false;

	bool SpawnAndAttachWeapon(TSubclassOf<AWeaponBase> WeaponClass);
	void StartWeaponSwap(EWeaponType TargetWeaponType, TSubclassOf<AWeaponBase> TargetWeaponClass);
	void FinishWeaponSwap();
	float PlayCompanionMontage(UAnimMontage* Montage, FName SectionName);

	UFUNCTION()
	void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void Die();
};





