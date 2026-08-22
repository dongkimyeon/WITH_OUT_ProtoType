// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CompanionChatTypes.h"
#include "CompanionMemorySaveGame.generated.h"

// 동료 NPC의 장기 기억(대화 이력/요약)을 로컬 슬롯에 저장하기 위한 SaveGame.
UCLASS()
class PROTOPROJECT_API UCompanionMemorySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FCompanionChatTurn> SavedHistory;

	UPROPERTY()
	FString MemorySummary;
};
