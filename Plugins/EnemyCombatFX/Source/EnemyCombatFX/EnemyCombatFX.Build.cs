using UnrealBuildTool;

public class EnemyCombatFX : ModuleRules
{
    public EnemyCombatFX(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent" });
        PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI" });
    }
}
