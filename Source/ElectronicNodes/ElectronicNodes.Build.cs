/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the UE4 Marketplace
*
* Integration Note: Integrated into XTools plugin for local use and customization
* Source: https://www.unrealengine.com/marketplace/electronic-nodes
*/

using System.IO;
using UnrealBuildTool;

public class ElectronicNodes : ModuleRules
{
	public ElectronicNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		string enginePath = Path.GetFullPath(Target.RelativeEnginePath);

		PublicIncludePaths.AddRange(
			new string[] { }
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(enginePath, "Source/Editor/AnimationBlueprintEditor/Private/"),
				Path.Combine(enginePath, "Source/Editor/BehaviorTreeEditor/Private/"),
				Path.Combine(enginePath, "Source/Editor/GraphEditor/Private/")
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"RenderCore",
				"InputCore",
				"Projects",
				"UnrealEd",
				"GraphEditor",
				"BlueprintGraph",
				"AnimGraph",
				"AnimationBlueprintEditor",
				"AIGraph",
				"BehaviorTreeEditor",
				"DeveloperSettings",
				"WebBrowser",
				"SettingsEditor"
			}
		);
		
		// UE 5.0+ EditorStyle 模块已废弃，不再需要
		// UE 4.x 仍需要此模块以支持旧版样式系统
		if (Target.Version.MajorVersion < 5)
		{
			PrivateDependencyModuleNames.Add("EditorStyle");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
				{ }
		);
	}
}