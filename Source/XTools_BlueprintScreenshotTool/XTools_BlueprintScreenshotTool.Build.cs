/*
* Copyright (c) 2024 Gradess Games
* Copyright (c) 2025 XIYBHK (XTools Integration)
* Licensed under UE_XTools License
*
* 蓝图截图工具模块
* Blueprint Screenshot Tool - 快速截取蓝图图表的屏幕截图工具
*
* 原始来源: Gradess Games
* 集成到 XTools 插件 - XIYBHK
* 支持版本: UE 5.3-5.6
*/

using UnrealBuildTool;

public class XTools_BlueprintScreenshotTool : ModuleRules
{
	public XTools_BlueprintScreenshotTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Projects",
				"UnrealEd",
				"ImageWriteQueue",
				"ApplicationCore",
				"InputCore",
				"RenderCore",
				"UMG",
			}
		);
	}
}