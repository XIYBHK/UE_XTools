// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class X_AssetEditor : ModuleRules
{
	public X_AssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "Public"),
				// ... add public include paths required here ...
			}
		);
				
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "Private"),
				Path.Combine(EngineDirectory, "Source", "Runtime", "Engine", "Classes"),
				Path.Combine(EngineDirectory, "Source", "Runtime", "Engine", "Public"),
			}
		);
			
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTasks",
				"DeveloperSettings",
				"InputCore",
				"EditorStyle",
				"RHI",
				"RenderCore",
				"ContentBrowser",
				"PhysicsCore",
			}
		);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"LevelEditor",
				"UnrealEd",
				"ToolMenus",
				"Slate",
				"SlateCore",

				"AssetRegistry",
				"AssetTools",
				"EditorScriptingUtilities",
				"Blutility",
				"GameplayTasks",
				"AnimGraphRuntime",
				"Engine",
				"CoreUObject",
				"DeveloperSettings",
				"EditorStyle",
				"PropertyEditor",
				"ApplicationCore",
				"Projects",
				"MaterialEditor",
				"PhysicsCore",
				"Chaos",
				"MeshDescription",
				"StaticMeshDescription",
			}
		);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
		
		// 确保编译器优化设置
		OptimizeCode = CodeOptimization.InShippingBuildsOnly;
		
		// 启用异常处理
		bEnableExceptions = true;
		
		// 添加预处理器定义
		PublicDefinitions.Add("WITH_EDITOR=1");
		PublicDefinitions.Add("UE_PLUGIN=1");
	}
} 