// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerStatusComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStatChanged, float, NewValue, float, MaxValue);

// 체력이 0에 도달했을 때 1회 브로드캐스트된다(감염/굶주림/피격 등 원인 무관).
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROTOPROJECT_API UPlayerStatusComponent : public UActorComponent
{
    GENERATED_BODY()

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float MaxHealth = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Health = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float MaxInfection = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Infection = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float MaxHunger = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Hunger = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float MaxThirst = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Thirst = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float MaxStamina = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Stamina = 100.0f;

    /*-------------------
     레이드 생존 시뮬레이션 (ARaidManager가 SetSurvivalSimulationActive(true)로 켠다)
    -------------------*/
    // 평상시 감염도 상승 속도(초당). 방치 시 이 속도로 100까지 차오른다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float InfectionRatePerSecond = 0.15f;

    // 레이드 제한시간(15분) 경과 후의 감염도 상승 속도(초당) - 사실상 즉사 유도.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float InfectionOverdueRatePerSecond = 20.0f;

    // 감염도가 최대(100)일 때 초당 깎이는 체력.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float InfectionDamagePerSecond = 1.0f;

    // 제한시간 경과 + 감염도 최대일 때 초당 깎이는 체력.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float InfectionOverdueDamagePerSecond = 10.0f;

    // 배고픔/목마름 감소 속도(초당). 0.5 = 2초당 1.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float HungerDrainPerSecond = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float ThirstDrainPerSecond = 0.5f;

    // 배고픔이 0일 때 초당 깎이는 체력.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float StarvationDamagePerSecond = 1.0f;

    // 목마름 또는 배고픔이 이 값 이하면 달리기 불가.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (AllowPrivateAccess = "true"))
    float SprintBlockThreshold = 20.0f;

    bool bIsDead = false;
    bool bSurvivalSimActive = false;
    bool bInfectionOverdue = false;

public:
    // Sets default values for this component's properties
    UPlayerStatusComponent();

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnStatChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnStatChanged OnInfectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnStatChanged OnHungerChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnStatChanged OnThirstChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnStatChanged OnStaminaChanged;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnPlayerDied OnPlayerDied;

    // ARaidManager가 레이드 진입 시 켜고, 허브에서는 켜지 않는다.
    void SetSurvivalSimulationActive(bool bActive) { bSurvivalSimActive = bActive; }

    // ARaidManager가 레이드 제한시간(15분) 경과 시 호출한다.
    void SetInfectionOverdue(bool bOverdue) { bInfectionOverdue = bOverdue; }

    // 레이드 종료(익스트랙션 성공 / 사망 후 허브 복귀 / 레이드 맵 언로드) 시 호출한다.
    // 생존 시뮬레이션을 끄고 감염/배고픔/목마름을 안전 상태로 되돌린다.
    // 폰이 레벨 트래블을 넘어 유지되더라도 허브(안전구역)에서 체력이 계속 깎이지 않게 한다.
    void ResetSurvivalState();

    bool IsDead() const { return bIsDead; }

    // 목마름/배고픔이 모두 임계치를 넘어야 달릴 수 있다.
    bool CanSprint() const { return Thirst > SprintBlockThreshold && Hunger > SprintBlockThreshold; }

public:
    //Getter
    float GetMaxHealth() const { return MaxHealth; }
    float GetHealth() const { return Health; }
    float GetMaxInfection() const { return MaxInfection; }
    float GetInfection() const { return Infection; }
    float GetMaxHunger() const { return MaxHunger; }
    float GetHunger() const{ return Hunger; }
    float GetMaxThirst() const { return MaxThirst; }
    float GetThirst() const { return Thirst; }
    float GetMaxStamina() const { return MaxStamina; }
    float GetStamina() const { return Stamina; }
public:
    //Setter
    void SetMaxHealth(float newMaxHealth);
    void SetHealth(float newHealth);
    void SetInfection(float newInfection);
    void SetHunger(float newHunger);
    void SetThirst(float newThirst);
    void SetStamina(float newStamina);
    
protected:
    // Called when the game starts
    virtual void BeginPlay() override;

public: 
    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

       
};