// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelChangeSelectWidget.h"
#include "Components/Button.h"
#include  "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "../Network/ProtoNetClientSubsystem.h"

void ULevelChangeSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SingleMap1Button) SingleMap1Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap1);
	if (SingleMap2Button) SingleMap2Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap2);
	if (MultiMap1Button) MultiMap1Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap1);
	if (MultiMap2Button) MultiMap2Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap2);
}

void ULevelChangeSelectWidget::OnClickSingleMap1()
{
	RequestLevelChange(ELevelChangeMode::Single, Stage1Level);
}

void ULevelChangeSelectWidget::OnClickSingleMap2()
{
	RequestLevelChange(ELevelChangeMode::Single, Stage2Level);
}

void ULevelChangeSelectWidget::OnClickMultiMap1()
{
	RequestLevelChange(ELevelChangeMode::Multi, Stage1Level);
}

void ULevelChangeSelectWidget::OnClickMultiMap2()
{
	RequestLevelChange(ELevelChangeMode::Multi, Stage2Level);
}

void ULevelChangeSelectWidget::RequestLevelChange(ELevelChangeMode Mode, const TSoftObjectPtr<UWorld>& Level)
{
	const FString ModeText = (Mode == ELevelChangeMode::Single) ? TEXT("Single") : TEXT("Multi");
	const FString Msg = FString::Printf(TEXT("레벨 변경 요청: %s / %s "),
		*ModeText, *Level.ToSoftObjectPath().GetAssetName());

	// Single: stop broadcasting our moves/actions and stop showing other
	// players (see SetMultiplayerVisualsEnabled). Multi: same behavior as
	// the "test" level, which is the default -- explicit here in case the
	// player is coming back from a Single map. UProtoNetClientSubsystem is a
	// GameInstanceSubsystem so this setting (and the connection itself)
	// survives the OpenLevelBySoftObjectPtr() below.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->SetMultiplayerVisualsEnabled(Mode == ELevelChangeMode::Multi);
		}
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(), Level);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, Msg);
	UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
}
