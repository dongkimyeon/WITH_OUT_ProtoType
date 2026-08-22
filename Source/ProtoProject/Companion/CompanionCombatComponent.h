// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompanionCombatComponent.generated.h"

class AWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompanionDied, AActor*, Companion);

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
	TSubclassOf<AWeaponBase> WeaponClass;

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

	UFUNCTION(BlueprintPure, Category = "Companion|Combat")
	AWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<AWeaponBase> EquippedWeapon;

	float LastAttackTime = -999.0f;
	bool bIsDead = false;

	void Die();
};
