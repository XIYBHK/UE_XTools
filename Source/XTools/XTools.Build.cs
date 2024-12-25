using UnrealBuildTool;

public class XTools : ModuleRules
{
	public XTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
		PublicIncludePaths.AddRange(
			new string[] {
				// ... 如果需要添加公共包含路径
			}
		);
        
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
				// ... 私有依赖项
			}
		);
	}
}