// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UStatComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROTO_TYPE_API UUStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
  UUStatComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
    float MaxHealth = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stat")
    float Health = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stat")
    float Infection = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stat")
    float Hunger = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stat")
    float Thirst = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stat")
    float Stamina = 100.f;
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
