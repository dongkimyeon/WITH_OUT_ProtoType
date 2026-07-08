// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProtoProject : ModuleRules
{
	public ProtoProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// 기존 코드 아래에 이 구문을 추가합니다.
		PublicIncludePaths.AddRange(new string[] {
			"ProtoProject/PlayerContent/Inventory",
			"ProtoProject/PlayerContent/Item"
		});
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
