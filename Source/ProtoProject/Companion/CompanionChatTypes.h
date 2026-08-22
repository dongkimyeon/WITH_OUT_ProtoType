// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CompanionChatTypes.generated.h"

// Gemini에 보내는 대화 턴 하나. Role은 "user" 또는 "model".
USTRUCT()
struct FCompanionChatTurn
{
	GENERATED_BODY()

	UPROPERTY()
	FString Role;

	UPROPERTY()
	FString Text;

	FCompanionChatTurn() = default;
	FCompanionChatTurn(const FString& InRole, const FString& InText)
		: Role(InRole), Text(InText)
	{
	}
};
