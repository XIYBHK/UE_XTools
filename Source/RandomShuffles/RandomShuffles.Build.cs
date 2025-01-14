/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 
using UnrealBuildTool;

public class RandomShuffles : ModuleRules
{
	public RandomShuffles(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// 添加公共包含路径
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// 添加其他私有包含路径
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
