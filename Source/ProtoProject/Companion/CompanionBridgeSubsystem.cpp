// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionBridgeSubsystem.h"
#include "CompanionLog.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Sockets.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

// 에디터/게임이 크래시 등으로 비정상 종료돼도 자식 프로세스가 좀비로 남지 않도록, 이 Job에
// 묶인 프로세스는 Job 핸들이 닫히는 순간(=우리 프로세스가 사라지는 순간 포함) OS가 강제 종료한다.
static void* CreateKillOnCloseJobObject()
{
	HANDLE Job = CreateJobObjectW(nullptr, nullptr);
	if (!Job)
	{
		return nullptr;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION Info = {};
	Info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Info, sizeof(Info)))
	{
		CloseHandle(Job);
		return nullptr;
	}
	return Job;
}

static void AssignProcessToKillOnCloseJob(void* JobObjectHandle, const FProcHandle& ProcHandle)
{
	if (JobObjectHandle && ProcHandle.IsValid())
	{
		// TTS_Bridge는 cmd.exe를 거쳐 python.exe를 띄우는데, CREATE_BREAKAWAY_FROM_JOB을 안 썼으므로
		// cmd.exe만 Job에 붙여도 그 자식(python.exe)까지 자동으로 같은 Job에 포함된다.
		AssignProcessToJobObject(static_cast<HANDLE>(JobObjectHandle), ProcHandle.Get());
	}
}
#endif

void UCompanionBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!bAutoLaunchEnabled)
	{
		return;
	}

#if PLATFORM_WINDOWS
	JobObjectHandle = CreateKillOnCloseJobObject();
#endif

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

#if PLATFORM_WINDOWS
	if (JobObjectHandle)
	{
		// Job 핸들을 닫는 순간 KILL_ON_JOB_CLOSE로 인해 혹시 남아있는 자식(예: cmd.exe가
		// 남기고 간 손자 프로세스)까지 OS가 정리한다. 위에서 이미 TerminateProc했어도 안전망으로 둔다.
		CloseHandle(static_cast<HANDLE>(JobObjectHandle));
		JobObjectHandle = nullptr;
	}
#endif

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

	// FPaths::ProjectDir()는 에디터에서 절대경로가 아니라 엔진 바이너리 기준 상대경로를 반환한다.
	// STT_Bridge.exe 직접 실행은 상대경로라도 별문제 없지만, TTS_Bridge 쪽은 cmd.exe에 작업
	// 디렉터리(lpCurrentDirectory)로 상대경로를 넘기면 제대로 안 먹어서 절대경로로 변환해둔다.
	const FString ExePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/STT_Bridge/STT_Bridge.exe"));
	if (!FPaths::FileExists(ExePath))
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Bridge] STT_Bridge.exe를 찾을 수 없음: %s"), *ExePath);
		return;
	}

	const FString WorkingDir = FPaths::GetPath(ExePath);
	SttProcHandle = FPlatformProcess::CreateProc(*ExePath, TEXT(""), true, false, false, nullptr, 0, *WorkingDir, nullptr);

	if (SttProcHandle.IsValid())
	{
#if PLATFORM_WINDOWS
		AssignProcessToKillOnCloseJob(JobObjectHandle, SttProcHandle);
#endif
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

	const FString BatPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools/TTS_Bridge/start_tts_server.bat"));
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
#if PLATFORM_WINDOWS
		AssignProcessToKillOnCloseJob(JobObjectHandle, TtsProcHandle);
#endif
		UE_LOG(LogCompanionAI, Log, TEXT("[Bridge] TTS_Bridge 자동 실행: %s"), *BatPath);
	}
	else
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Bridge] TTS_Bridge 실행 실패: %s"), *BatPath);
	}
}
