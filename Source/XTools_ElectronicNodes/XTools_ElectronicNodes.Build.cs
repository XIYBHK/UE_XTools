/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the UE4 Marketplace
*
* Integration Note: Integrated into XTools plugin for local use and customization
* Source: https://www.unrealengine.com/marketplace/electronic-nodes
*/

using System.IO;
using UnrealBuildTool;

public class XTools_ElectronicNodes : ModuleRules
{
	public XTools_ElectronicNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		// UE5.3+ 标准配置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// C++20 标准与引擎保持一致
		CppStandard = CppStandardVersion.Default;
		
		// 强制执行 IWYU 原则 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;
		
		// 开发时禁用 Unity Build，确保代码质量
		bUseUnity = false;
		
		// UE 标准设置
		bEnableExceptions = false;
		bUseRTTI = false;
		
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