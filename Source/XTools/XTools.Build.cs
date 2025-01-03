using UnrealBuildTool;
using System.IO;

/**
 * XTools Plugin Module
 * 
 * Provides utility functions and tools for Unreal Engine projects
 */
public class XTools : ModuleRules
{
	public XTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Compilation optimization
		bUsePrecompiled = false;
		bEnableUndefinedIdentifierWarnings = false;
		bEnableExceptions = true;
		bUseRTTI = true;

		// Public include paths
		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Public",
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes"),
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Public"),
			Path.Combine(EngineDirectory, "Source/Runtime/Core/Public"),
			Path.Combine(EngineDirectory, "Source/Runtime/CoreUObject/Public"),
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes/Components"),
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes/GameFramework")
		});

		// Private include paths  
		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Private",
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Private")
		});

		// Public dependencies
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore", 
			"Slate",
			"SlateCore",
			"UMG"
		});

		// Private dependencies
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects",
			"ApplicationCore",
			"Json",
			"JsonUtilities"
		});

		// Dynamically loaded modules
		DynamicallyLoadedModuleNames.AddRange(new string[] {
		});

		// Additional compiler flags
		PublicDefinitions.Add("WITH_XTOOLS=1");
	}
}
