// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionBridgeSubsystem.h"
#include "CompanionLog.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Sockets.h"

void UCompanionBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!bAutoLaunchEnabled)
	{
		return;
	}

	LaunchSttBridgeIfNeeded();
	LaunchTtsBridgeIfNeeded();
}

void UCompanionBridgeSubsystem::Deinitialize()
{
	// 우리가 직접 띄운 프로세스만 정리한다(이미 떠 있어서 건너뛴 경우는 핸들이 비어있어 여기서 안 건드림).
	if (SttProcHandle.IsValid() && FPlatformProcess::IsProcRunning(SttProcHandle))
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Bridge] STT_Bridge 프로세스 종료"));
		FPlatformProcess::TerminateProc(SttProcHandle, true);
	}
	SttProcHandle.Reset();

	if (TtsProcHandle.IsValid() && FPlatformProcess::IsProcRunning(TtsProcHandle))
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Bridge] TTS_Bridge 프로세스 종료"));
		FPlatformProcess::TerminateProc(TtsProcHandle, true);
	}
	TtsProcHandle.Reset();

	Super::Deinitialize();
}

bool UCompanionBridgeSubsystem::IsPortOpen(int32 Port) const
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("CompanionBridgePortCheck"), false);
	if (!Socket)
	{
		return false;
	}

	bool bIsValidIp = false;
	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	Addr->SetIp(TEXT("127.0.0.1"), bIsValidIp);
	Addr->SetPort(Port);

	// 127.0.0.1은 리스너가 없으면 즉시 RST가 오므로 블로킹 connect를 써도 게임 스레드가
	// 눈에 띄게 멈추지 않는다. 논블로킹 select 방식은 "연결 거부됐는데도 writable로 나옴" 같은
	// 오탐이 있어서 일부러 블로킹으로 확인한다.
	const bool bConnected = Socket->Connect(*Addr);

	SocketSubsystem->DestroySocket(Socket);
	return bConnected;
}

void UCompanionBridgeSubsystem::LaunchSttBridgeIfNeeded()
{
	if (IsPortOpen(SttPort))
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Bridge] STT_Bridge가 이미 실행 중(port %d) - 자동 실행 건너뜀"), SttPort);
		return;
	}

	const FString ExePath = FPaths::ProjectDir() / TEXT("Tools/STT_Bridge/STT_Bridge.exe");
	if (!FPaths::FileExists(ExePath))
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Bridge] STT_Bridge.exe를 찾을 수 없음: %s"), *ExePath);
		return;
	}

	const FString WorkingDir = FPaths::GetPath(ExePath);
	SttProcHandle = FPlatformProcess::CreateProc(*ExePath, TEXT(""), true, false, false, nullptr, 0, *WorkingDir, nullptr);

	if (SttProcHandle.IsValid())
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Bridge] STT_Bridge 자동 실행: %s"), *ExePath);
	}
	else
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Bridge] STT_Bridge 실행 실패: %s"), *ExePath);
	}
}

void UCompanionBridgeSubsystem::LaunchTtsBridgeIfNeeded()
{
	if (IsPortOpen(TtsPort))
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Bridge] TTS_Bridge가 이미 실행 중(port %d) - 자동 실행 건너뜀"), TtsPort);
		return;
	}

	const FString BatPath = FPaths::ProjectDir() / TEXT("Tools/TTS_Bridge/start_tts_server.bat");
	if (!FPaths::FileExists(BatPath))
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Bridge] start_tts_server.bat를 찾을 수 없음: %s"), *BatPath);
		return;
	}

	// .bat는 PE 실행 파일이 아니라 CreateProcess로 직접 못 띄우므로 cmd.exe /c로 감싼다.
	const FString WorkingDir = FPaths::GetPath(BatPath);
	const FString Params = FString::Printf(TEXT("/c \"%s\""), *BatPath);
	TtsProcHandle = FPlatformProcess::CreateProc(TEXT("cmd.exe"), *Params, true, false, false, nullptr, 0, *WorkingDir, nullptr);

	if (TtsProcHandle.IsValid())
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Bridge] TTS_Bridge 자동 실행: %s"), *BatPath);
	}
	else
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Bridge] TTS_Bridge 실행 실패: %s"), *BatPath);
	}
}
