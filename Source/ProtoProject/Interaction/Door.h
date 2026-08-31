// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "../PlayerContent/Interactable.h"
#include "Door.generated.h"

class UStaticMeshComponent;
class AProtoCharacter;

UCLASS()
class PROTOPROJECT_API ADoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADoor();

	// 힌지(경첩) 위치가 원점인 루트. 문짝 메시는 피벗이 경첩에 있으므로 그대로 부착.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InteractBox;

	// 열렸을 때 회전 각도(크기). 방향(부호)은 플레이어 위치에 따라 자동 결정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenAngle = 110.f;

	// 메시 제작 방향이 반대라 스윙이 거꾸로면 체크.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bInvertSwing = false;

	// 회전 보간 속도(FInterpTo Speed). 클수록 빠르게 열림.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float RotateSpeed = 6.f;

	virtual void OnInteract_Implementation(AProtoCharacter* InPlayer) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AProtoCharacter* InPlayer) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	virtual void Tick(float DeltaTime) override;

private:
	bool bIsOpen = false;
	float CurrentYaw = 0.f;
	// 열 때 결정된 목표 각도(스윙 방향 포함). 닫으면 0.
	float OpenTargetYaw = 0.f;
};
