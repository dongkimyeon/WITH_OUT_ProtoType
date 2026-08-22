// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompanionCommandRouterComponent.generated.h"

class UCompanionAIComponent;
class UCompanionBrainComponent;
class UCompanionSpeechComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class ECompanionCommandType : uint8
{
	None,
	Follow,
	Stop,
	MoveHere,
	Engage
};

USTRUCT(BlueprintType)
struct FCompanionKeywordRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	TArray<FString> Keywords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	ECompanionCommandType Command = ECompanionCommandType::None;
};

// CompanionListenComponent::OnTranscribed를 가로채 3단 계층으로 처리한다:
// (1) 로컬 키워드 즉시 실행 -> (2) 미매치 시 Brain 텍스트 폴백 -> (3) 타겟 모호/시각 질의일 때만
// 화면 캡처를 Brain에 함께 전송. Brain의 Function Calling 액션(OnActionRequested)도 여기서 받는다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOPROJECT_API UCompanionCommandRouterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionCommandRouterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	TArray<FCompanionKeywordRule> KeywordRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	TArray<FString> VisionQueryKeywords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	float MoveCommandTraceRange = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	int32 VisionCaptureResolution = 512;

	// 전투 중에는 비전 캡처(GPU 스톨 위험)를 걸지 않고 이 대사만 즉시 응답한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	FString BusyDuringCombatReply = TEXT("지금은 좀 바빠!");

	// 키워드/Brain 액션으로 실행된 명령에 대해 즉시 들려줄 확인 대사(LLM을 거치지 않아 지연 없음).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	FString FollowAckLine = TEXT("알겠어, 따라갈게!");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	FString StopAckLine = TEXT("여기서 기다릴게.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	FString EngageAckLine = TEXT("좋아, 싸우자!");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Companion|Command")
	FString MoveHereAckLine = TEXT("그쪽으로 갈게.");

	UFUNCTION()
	void HandleTranscribed(const FString& RecognizedText);

	UFUNCTION()
	void HandleActionRequested(const FString& ActionName, const FString& ActionArg);

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<UCompanionAIComponent> AIComponent;
	TWeakObjectPtr<UCompanionBrainComponent> BrainComponent;
	TWeakObjectPtr<UCompanionSpeechComponent> SpeechComponent;
	// 소유는 ACompanionNPC(액터)가 한다 - 컴포넌트 서브오브젝트로 만들면 액터 컴포넌트 등록에서
	// 누락돼 렌더링이 안 될 수 있어 액터에 직접 부착하고 여기서는 참조만 들고 있는다.
	TWeakObjectPtr<USceneCaptureComponent2D> VisionCapture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> VisionRenderTarget;

	ECompanionCommandType MatchKeywordCommand(const FString& Text) const;
	bool MatchesVisionQuery(const FString& Text) const;
	void ExecuteCommand(ECompanionCommandType Command);
	void PerformMoveToWherePlayerIsLooking();
	void TriggerVisionReply(const FString& UserText);
};
