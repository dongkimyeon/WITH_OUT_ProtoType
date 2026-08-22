// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionNPC.h"
#include "CompanionListenComponent.h"
#include "CompanionBrainComponent.h"
#include "CompanionSpeechComponent.h"
#include "CompanionAIComponent.h"
#include "CompanionCombatComponent.h"
#include "CompanionPerceptionComponent.h"
#include "CompanionCommandRouterComponent.h"
#include "CompanionReportComponent.h"
#include "AIController.h"
#include "Components/SceneCaptureComponent2D.h"

ACompanionNPC::ACompanionNPC()
{
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	ListenComponent = CreateDefaultSubobject<UCompanionListenComponent>(TEXT("ListenComponent"));
	BrainComponent = CreateDefaultSubobject<UCompanionBrainComponent>(TEXT("BrainComponent"));
	SpeechComponent = CreateDefaultSubobject<UCompanionSpeechComponent>(TEXT("SpeechComponent"));

	AIComponent = CreateDefaultSubobject<UCompanionAIComponent>(TEXT("AIComponent"));
	CombatComponent = CreateDefaultSubobject<UCompanionCombatComponent>(TEXT("CombatComponent"));
	PerceptionComponent = CreateDefaultSubobject<UCompanionPerceptionComponent>(TEXT("PerceptionComponent"));
	CommandRouterComponent = CreateDefaultSubobject<UCompanionCommandRouterComponent>(TEXT("CommandRouterComponent"));
	ReportComponent = CreateDefaultSubobject<UCompanionReportComponent>(TEXT("ReportComponent"));

	VisionCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("VisionCaptureComponent"));
	VisionCaptureComponent->SetupAttachment(RootComponent);
	VisionCaptureComponent->bCaptureEveryFrame = false;
	VisionCaptureComponent->bCaptureOnMovement = false;
	VisionCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
}

void ACompanionNPC::BeginPlay()
{
	Super::BeginPlay();

	// 명령/대화 라우팅: 인식된 텍스트는 항상 Router를 먼저 거친다(키워드 즉시 실행 -> Brain 폴백).
	ListenComponent->OnTranscribed.AddDynamic(CommandRouterComponent, &UCompanionCommandRouterComponent::HandleTranscribed);

	// 대화 응답(순수 텍스트)은 곧바로 TTS로. 액션(Function Calling)은 Router가 받아 AIComponent에 배선.
	BrainComponent->OnReplyReady.AddDynamic(SpeechComponent, &UCompanionSpeechComponent::Speak);
	BrainComponent->OnActionRequested.AddDynamic(CommandRouterComponent, &UCompanionCommandRouterComponent::HandleActionRequested);
}
