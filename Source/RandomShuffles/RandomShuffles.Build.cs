/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 
using UnrealBuildTool;

public class RandomShuffles : ModuleRules
{
	public RandomShuffles(ReadOnlyTargetRules Target) : base(Target)
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
	}
}
