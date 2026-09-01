// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidManager.generated.h"

class UPlayerStatusComponent;
class URaidTimerWidget;
class UUserWidget;

// 스테이지(레이드) 맵에 배치하는 컨트롤러. 존재 자체가 "여기는 레이드다"를 뜻한다:
//  - 로컬 플레이어를 찾으면 생존 시뮬레이션(감염/배고픔/목마름)을 켠다
//  - 15분 제한시간을 카운트다운하고, 넘기면 감염을 폭주시킨다
//  - 레이드 HUD 타이머 위젯을 붙이고 갱신한다
//  - 플레이어 사망 시 사망 UI를 띄우고 5초 후 SafePlace로 돌려보낸다
//    (지니고 있던 아이템 소실은 AProtoCharacter::HandleDeath가 처리 - 스태시는 별개)
// 멀티에서도 GetPlayerPawn(0)으로 자기 클라이언트의 로컬 플레이어만 다룬다(사망/감염 로컬 처리).
UCLASS()
class PROTOPROJECT_API ARaidManager : public AActor
{
	GENERATED_BODY()

public:
	ARaidManager();

	// 레이드 제한시간(초). 이 시간이 지나면 감염이 폭주한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid")
	float RaidDuration = 900.0f;

	// 사망 후 SafePlace로 돌아가기까지의 딜레이(사망 화면 노출 시간).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid")
	float DeathReturnDelay = 5.0f;

	// 사망/탈출 실패 시 이동할 레벨(SafePlace). 디자이너가 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid")
	TSoftObjectPtr<UWorld> ExtractionFailLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid")
	TSubclassOf<URaidTimerWidget> RaidTimerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid")
	TSubclassOf<UUserWidget> DeathScreenWidgetClass;

	UFUNCTION(BlueprintPure, Category = "Raid")
	float GetRemainingRaidTime() const { return FMath::Max(0.0f, RaidDuration - RaidElapsed); }

	UFUNCTION(BlueprintPure, Category = "Raid")
	bool IsOverdue() const { return bOverdueTriggered; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandlePlayerDied();

	void ReturnToHub();

	// 로컬 플레이어 폰 + 상태 컴포넌트를 잡을 때까지 매 틱 시도한다.
	bool TryAcquireLocalPlayer();

	float RaidElapsed = 0.0f;
	bool bRaidActive = false;
	bool bOverdueTriggered = false;

	TWeakObjectPtr<UPlayerStatusComponent> LocalStatus;

	UPROPERTY()
	URaidTimerWidget* TimerWidgetInstance = nullptr;

	UPROPERTY()
	UUserWidget* DeathScreenInstance = nullptr;

	FTimerHandle DeathReturnTimerHandle;
};
