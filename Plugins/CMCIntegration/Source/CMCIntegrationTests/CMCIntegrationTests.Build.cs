using UnrealBuildTool;
public class CMCIntegrationTests : ModuleRules
{
    public CMCIntegrationTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "ApplicationCore", "UnrealEd", "BlueprintGraph", "KismetCompiler" });
    }
}
