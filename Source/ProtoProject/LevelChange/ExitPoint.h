// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Components/BoxComponent.h"

#include "ExitPoint.generated.h"

class UStaticMeshComponent;
class AProtoCharacter;
class UExitPointWidget;

// Standing inside ExitBox for ExitDuration seconds (without leaving) sends
// the locally controlled player back to SafePlaceLevel. Leaving early resets
// the countdown.
UCLASS()
class PROTOPROJECT_API AExitPoint : public AActor
{
	GENERATED_BODY()

public:
	AExitPoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* ExitBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ExitPoint")
	float ExitDuration = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ExitPoint")
	TSoftObjectPtr<UWorld> SafePlaceLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ExitPoint")
	TSubclassOf<UExitPointWidget> ExitPointWidgetClass;

	UFUNCTION(BlueprintPure, Category = "ExitPoint")
	float GetRemainingTime() const { return RemainingTime; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnExitBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnExitBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	UPROPERTY()
	AProtoCharacter* OverlappingPlayer = nullptr;

	UPROPERTY()
	UExitPointWidget* ExitPointWidgetInstance = nullptr;

	float RemainingTime = 0.f;
};
