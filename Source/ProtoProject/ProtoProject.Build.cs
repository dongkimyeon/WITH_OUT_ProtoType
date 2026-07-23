// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProtoProject : ModuleRules
{
	public ProtoProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Sockets", "Networking" });

		// 기존 코드 아래에 이 구문을 추가합니다.
		PublicIncludePaths.AddRange(new string[] {
			"ProtoProject/PlayerContent/Inventory",
			"ProtoProject/PlayerContent/Item"
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

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
