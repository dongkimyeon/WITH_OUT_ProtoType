// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelChangeSelectWidget.h"
#include "Components/Button.h"

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
	RequestLevelChange(ELevelChangeMode::Single, TEXT("L_Stage1"));
}

void ULevelChangeSelectWidget::OnClickSingleMap2()
{
	RequestLevelChange(ELevelChangeMode::Single, TEXT("L_Stage2"));
}

void ULevelChangeSelectWidget::OnClickMultiMap1()
{
	RequestLevelChange(ELevelChangeMode::Multi, TEXT("L_Stage1"));
}

void ULevelChangeSelectWidget::OnClickMultiMap2()
{
	RequestLevelChange(ELevelChangeMode::Multi, TEXT("L_Stage2"));
}

void ULevelChangeSelectWidget::RequestLevelChange(ELevelChangeMode Mode, FName MapName)
{
	const FString ModeText = (Mode == ELevelChangeMode::Single) ? TEXT("Single") : TEXT("Multi");
	const FString Msg = FString::Printf(TEXT("레벨 변경 요청: %s / %s "),
		*ModeText, *MapName.ToString());

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, Msg);
	UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
}
