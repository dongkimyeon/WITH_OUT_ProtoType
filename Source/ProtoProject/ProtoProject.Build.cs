// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProtoProject : ModuleRules
{
	public ProtoProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "AIModule", "GameplayTags", "NavigationSystem" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Sockets", "Networking", "AssetRegistry", "HTTP", "Json", "JsonUtilities", "AudioCaptureCore", "ImageWrapper", "MoviePlayer" });

		// 0번 키 디버그 패널(ProtoDebugPanel)을 Shipping 패키징에서도 열 수 있게 하는 스위치.
		// 패널 코드는 원래 !UE_BUILD_SHIPPING으로만 막혀 있어서 Shipping 빌드에선 키 바인딩째로
		// 컴파일에서 빠졌다. 진짜 출시 빌드를 만들 땐 이 값을 0으로 되돌리면 그때 동작으로 복귀한다.
		PublicDefinitions.Add("PROTO_DEBUG_PANEL=1");

		// 기존 코드 아래에 이 구문을 추가합니다.
		PublicIncludePaths.AddRange(new string[] {
			"ProtoProject/PlayerContent/Inventory",
			"ProtoProject/PlayerContent/Item",
			"ProtoProject/Companion"
		});

		// RIO 에코 서버와 통신하기 위한 Protocol(FlatBuffers) 헤더 경로.
		PrivateIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(ModuleDirectory, "../../Protocol/include"),
			System.IO.Path.Combine(ModuleDirectory, "../../Protocol/header/cpp"),
		});
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// UnrealEd는 에디터 전용 모듈이라 Shipping/Game 타겟(패키징 빌드)엔
		// 절대 링크하면 안 된다 -- bBuildEditor로 ProtoProjectEditor 타겟일
		// 때만 추가한다. ExportLevelObstaclesCommandlet이 맵을 "File > Open
		// Level"과 똑같은 방식(UEditorLoadingAndSavingUtils::LoadMap)으로
		// 로드해서 액터 트랜스폼을 제대로 계산시키기 위해 필요.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
