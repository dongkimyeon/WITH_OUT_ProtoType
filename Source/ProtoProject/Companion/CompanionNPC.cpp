// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionNPC.h"
#include "CompanionListenComponent.h"
#include "CompanionBrainComponent.h"
#include "CompanionSpeechComponent.h"

ACompanionNPC::ACompanionNPC()
{
	ListenComponent = CreateDefaultSubobject<UCompanionListenComponent>(TEXT("ListenComponent"));
	BrainComponent = CreateDefaultSubobject<UCompanionBrainComponent>(TEXT("BrainComponent"));
	SpeechComponent = CreateDefaultSubobject<UCompanionSpeechComponent>(TEXT("SpeechComponent"));
}

void ACompanionNPC::BeginPlay()
{
	Super::BeginPlay();

	// 대화 파이프라인 체이닝: 인식 -> LLM 요청 -> TTS 발화
	ListenComponent->OnTranscribed.AddDynamic(BrainComponent, &UCompanionBrainComponent::RequestReply);
	BrainComponent->OnReplyReady.AddDynamic(SpeechComponent, &UCompanionSpeechComponent::Speak);
}
