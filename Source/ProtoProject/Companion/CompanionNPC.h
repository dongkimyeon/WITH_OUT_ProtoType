// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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

protected:
	virtual void BeginPlay() override;
};
