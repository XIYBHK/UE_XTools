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