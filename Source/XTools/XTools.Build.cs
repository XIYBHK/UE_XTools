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

		// 添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_XTOOLS=1",
			"DLLEXPORT=__declspec(dllexport)",
			"DLLIMPORT=__declspec(dllimport)"
		});

		// Compilation optimization
		bUsePrecompiled = false;
		bEnableUndefinedIdentifierWarnings = false;
		bEnableExceptions = true;
		bUseRTTI = true;

		// Public include paths
		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Public",
			ModuleDirectory + "/AsyncTools",
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes"),
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Public"),
			Path.Combine(EngineDirectory, "Source/Runtime/Core/Public"),
			Path.Combine(EngineDirectory, "Source/Runtime/CoreUObject/Public"),
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes/Components"),
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes/GameFramework"),
			Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes/Engine")
		});

		// Private include paths  
		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Private",
			ModuleDirectory + "/AsyncTools",
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
			"UMG",
			"ComponentTimelineRuntime",
			"RandomShuffles"
		});

		// Private dependencies
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects",
			"ApplicationCore", 
			"Json",
			"JsonUtilities",
			"DeveloperSettings"
		});

		// Editor-only dependencies
		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"Kismet",
				"UnrealEd",
				"BlueprintGraph",
				"GraphEditor",
				"ComponentTimelineUncooked"
			});
		}

		// Dynamically loaded modules
		DynamicallyLoadedModuleNames.AddRange(new string[] {
		});

		// Additional compiler flags
		PublicDefinitions.Add("WITH_XTOOLS=1");
	}
}
