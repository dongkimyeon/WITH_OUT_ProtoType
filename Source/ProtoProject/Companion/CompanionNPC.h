// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "CompanionNPC.generated.h"

class UCompanionListenComponent;
class UCompanionBrainComponent;
class UCompanionSpeechComponent;
class UCompanionAIComponent;
class UCompanionCombatComponent;
class UCompanionPerceptionComponent;
class UCompanionCommandRouterComponent;
class UCompanionReportComponent;
class USceneCaptureComponent2D;
class UInventoryGridComponent;
class AWeaponBase;

// STT(로컬 브릿지) -> Gemini LLM -> TTS(로컬 서버)로 음성 대화하고, AIController 기반 커스텀 BT로
// 플레이어를 따라다니거나 명령에 따라 이동/전투하는 동료 NPC.
// PTT 입력은 플레이어 쪽(AProtoCharacter)에서 바인딩하고, 이 액터는 대화+AI 파이프라인 컴포넌트를 들고 있는다.
// 스켈레탈 메시는 파생 블루프린트(BP_CompanionNPC)에서 대충 배정해 배치한다.
UCLASS()
class PROTOPROJECT_API ACompanionNPC : public ACharacter
{
	GENERATED_BODY()

public:
	ACompanionNPC();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionListenComponent> ListenComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionBrainComponent> BrainComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionSpeechComponent> SpeechComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionAIComponent> AIComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionCombatComponent> CombatComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionPerceptionComponent> PerceptionComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionCommandRouterComponent> CommandRouterComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UCompanionReportComponent> ReportComponent;

	// 비전 폴백(시각 질의)에서 플레이어 카메라 시점으로 정렬해 온디맨드 캡처하는 데 쓰인다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<USceneCaptureComponent2D> VisionCaptureComponent;

	// "주변 탐색해봐" 명령으로 주운 아이템을 담는 동료 전용 인벤토리(플레이어 인벤토리와 별개).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TObjectPtr<UInventoryGridComponent> InventoryComponent;

	// 스폰한 쪽(AProtoCharacter)이 스폰 직후 호출해, 이 동료가 누구를 따라다닐지 명시적으로 지정한다.
	UFUNCTION(BlueprintCallable, Category = "Companion")
	void SetOwningPlayer(APawn* Player);

	// Player ABP와 같은 이름으로 노출하는 동료 애니메이션용 상태값.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	bool bIsSprint = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	bool bHasWeapon = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	FTransform LeftHandTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	FVector Joint = FVector(1000.0f, -2000.0f, 0.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	EWeaponType CurrentWeaponType = EWeaponType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	bool bIsAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	float AimPitch = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	bool SwappingAlpha = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Companion|Animation")
	bool bIsReloading = false;
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	// Throttled position/facing reporting to the server (same idea as
	// AProtoCharacter's own NetSyncInterval) so other clients can mirror
	// this companion via a placeholder actor -- see
	// UProtoNetClientSubsystem::SendCompanionMoveInput/UpdateRemoteCompanion.
	// Only meaningful, and only sent, when the owning player is the LOCAL
	// player (see SetOwningPlayer) -- there's no such thing as "someone
	// else's companion" spawned on this client to report a position for.
	float NetSyncTimer = 0.0f;
	static constexpr float NetSyncInterval = 0.1f;
	bool bOwnedByLocalPlayer = false;
};
