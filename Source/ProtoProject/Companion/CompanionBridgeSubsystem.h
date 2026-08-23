// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CompanionBridgeSubsystem.generated.h"

// 에디터 PIE 시작 시(또는 패키지된 실행 파일 시작 시) Tools/STT_Bridge, Tools/TTS_Bridge의 로컬
// 사이드카 서버를 자동으로 띄운다. 이미 해당 포트가 열려 있으면(수동으로 미리 켜둔 경우) 건드리지
// 않고, 우리가 직접 띄운 프로세스만 게임 종료 시 같이 정리한다.
UCLASS(config=Game)
class PROTOPROJECT_API UCompanionBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, Category = "Companion|Bridge")
	bool bAutoLaunchEnabled = true;

	UPROPERTY(EditAnywhere, Config, Category = "Companion|Bridge")
	int32 SttPort = 8090;

	UPROPERTY(EditAnywhere, Config, Category = "Companion|Bridge")
	int32 TtsPort = 8080;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	FProcHandle SttProcHandle;
	FProcHandle TtsProcHandle;

	bool IsPortOpen(int32 Port) const;
	void LaunchSttBridgeIfNeeded();
	void LaunchTtsBridgeIfNeeded();
};
