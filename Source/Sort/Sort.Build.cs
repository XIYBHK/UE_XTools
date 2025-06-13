using UnrealBuildTool;

public class Sort : ModuleRules
{
	public Sort(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ...
			}
		);

		// --- 为编辑器添加必要的模块依赖 ---
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"BlueprintGraph",
					"GraphEditor",
					"KismetCompiler",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"EditorWidgets",
					"PropertyEditor"
				}
			);
		}
	}
}