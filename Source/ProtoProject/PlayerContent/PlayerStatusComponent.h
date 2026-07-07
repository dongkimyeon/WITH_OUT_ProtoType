// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerStatusComponent.generated.h"


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
    float Infection = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Hunger = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Thirst = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = "true"))
    float Stamina = 100.0f;
    
    
public: 
    // Sets default values for this component's properties
    UPlayerStatusComponent();
public:
    //Getter
    float GetMaxHealth() const { return MaxHealth; }
    float GetHealth() const { return Health; }
    float GetInfection() const { return Infection; }
    float GetHunger() const{ return Hunger; }
    float GetThirst() const { return Thirst; }
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