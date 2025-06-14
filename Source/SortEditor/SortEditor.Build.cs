using UnrealBuildTool;

public class SortEditor : ModuleRules
{
    public SortEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // 仅在编辑器中编译此模块
        if (!Target.bBuildEditor)
        {
            return;
        }

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Sort",            // 依赖运行时排序模块
                "UnrealEd",
                "BlueprintGraph",
                "GraphEditor",
                "KismetCompiler",
                "InputCore",
                "EditorStyle"
            }
        );
    }
} 