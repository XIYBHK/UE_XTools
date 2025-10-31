using UnrealBuildTool;

public class ObjectPoolEditor : ModuleRules
{
    public ObjectPoolEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        // UE 5.2 兼容性修复
        bTreatWarningsAsErrors = false;

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject", 
            "Engine",
            "ObjectPool"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "BlueprintGraph",
            "KismetCompiler",
            "Slate",
            "SlateCore"
        });
    }
}
