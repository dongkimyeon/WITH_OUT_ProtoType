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

	// 머리 위 상태 라벨(UI, PlayerDefalutUI::AddCompanionStatusLabel)에 표시할 이름.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	FText CompanionDisplayName = FText::FromString(TEXT("서아"));

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

	// SpawnActorDeferred 직후, FinishSpawning 전에 호출해야 한다 (see
	// UProtoNetClientSubsystem::UpdateRemoteCompanion) -- 다른 클라이언트의
	// 동료를 이 클라이언트에서 시각적으로만 미러링하는 인스턴스로 표시한다.
	// AutoPossessAI를 여기서 꺼서 AIController가 아예 안 붙게 하고,
	// BeginPlay()가 마이크/LLM/AI/전투 파이프라인 컴포넌트 전체를 destroy하게
	// 만든다 -- 이 인형에게 명령을 내리거나 마이크를 켜야 할 주체는 오직 그
	// 동료의 진짜 주인(다른 클라이언트) 뿐이다.
	void MarkAsRemotePuppet();

	// true면 이 인스턴스는 원격 동료 시각화용 인형이다 -- MarkAsRemotePuppet 참고.
	UPROPERTY(BlueprintReadOnly, Category = "Companion")
	bool bIsRemotePuppet = false;

	// Mirrored health/dead state for a REMOTE puppet ONLY (bIsRemotePuppet) --
	// MarkAsRemotePuppet() destroys CombatComponent entirely (see its own
	// comment), so a remote puppet has nowhere else to keep this. Written
	// only by UProtoNetClientSubsystem::UpdateRemoteCompanion, read by
	// TickRemotePlayers() to stop walking a dead companion around. The
	// real, locally-owned companion's health/dead state stays on
	// CombatComponent as always -- this is never touched for one of those.
	UPROPERTY(BlueprintReadOnly, Category = "Companion")
	float MirroredHealth = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Companion")
	bool bIsMirroredDead = false;

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
