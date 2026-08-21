// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CompanionNPC.generated.h"

class UCompanionListenComponent;
class UCompanionBrainComponent;
class UCompanionSpeechComponent;

// STT(로컬 브릿지) -> Gemini LLM -> TTS(로컬 서버)로 음성 대화하는 동료 NPC.
// PTT 입력은 플레이어 쪽(AProtoCharacter)에서 바인딩하고, 이 액터는 대화 파이프라인 컴포넌트만 들고 있는다.
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

protected:
	virtual void BeginPlay() override;
};
