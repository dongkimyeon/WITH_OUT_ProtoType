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

	// 근접 감지용 트리거(오버랩).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InteractBox;

	// F키 라인트레이스가 맞는 대상. Root에 붙어 문과 함께 회전하지 않고 문틀에 고정.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* HitBox;

	// "F 열기" 프롬프트가 뜨는 월드 위치. BP에서 위치 조정.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* PromptAnchor;

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

	// Stable id shared across every client's copy of this level, derived
	// from the placed actor's own in-level name -- same idea as
	// AItemContainerBase::GetContainerId. Used to key the network relay
	// (see UProtoNetClientSubsystem::SendDoorInteract).
	int32 GetDoorId() const;

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
	// Bound (if connected) to UProtoNetClientSubsystem::OnDoorInteract --
	// applies another client's open/close toggle to this door too. Never
	// fires for this client's own toggle (see that delegate's comment).
	UFUNCTION()
	void HandleDoorInteract(int32 DoorId, bool bOpen);

	// Shared by OnInteract_Implementation (local, player-relative swing
	// direction) and HandleDoorInteract (remote -- no player reference to
	// swing away from, so it just uses the default/inverted sign).
	void SetOpen(bool bNewOpen, float SwingSign);

	bool bIsOpen = false;
	float CurrentYaw = 0.f;
	// 열 때 결정된 목표 각도(스윙 방향 포함). 닫으면 0.
	float OpenTargetYaw = 0.f;
};
