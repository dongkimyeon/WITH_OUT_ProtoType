// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelChangeSelectWidget.h"
#include "Components/Button.h"
#include  "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "../Network/ProtoNetClientSubsystem.h"

void ULevelChangeSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// AddToViewport() 이후 RemoveFromParent()로 뗐다가 다시 AddToViewport()하면
	// 슬레이트 위젯이 재생성되며 NativeConstruct가 다시 호출될 수 있다.
	// RemoveDynamic으로 먼저 정리해서 델리게이트가 중복 바인딩되지 않게 한다.
	if (SingleMap1Button)
	{
		SingleMap1Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap1);
		SingleMap1Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap1);
	}
	if (SingleMap2Button)
	{
		SingleMap2Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap2);
		SingleMap2Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickSingleMap2);
	}
	if (MultiMap1Button)
	{
		MultiMap1Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap1);
		MultiMap1Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap1);
	}
	if (MultiMap2Button)
	{
		MultiMap2Button->OnClicked.RemoveDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap2);
		MultiMap2Button->OnClicked.AddDynamic(this, &ULevelChangeSelectWidget::OnClickMultiMap2);
	}
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
