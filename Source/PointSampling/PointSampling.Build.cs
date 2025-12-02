// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * PointSampling Module
 * 
 * 提供各种点阵采样算法，包括泊松圆盘采样、Halton序列等
 */
public class PointSampling : ModuleRules
{
	public PointSampling(ReadOnlyTargetRules Target) : base(Target)
	{
		//  UE5.3+ 标准配置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		//  C++20 标准配置 - 与 UE5.3+ 引擎保持一致
		CppStandard = CppStandardVersion.Default;

		//  IWYU 强制执行 - 提升编译速度和代码质量
		IWYUSupport = IWYUSupport.Full;

		//  开发时配置 - 确保代码质量
		bUseUnity = false;

		//  UE 标准设置 - 符合引擎最佳实践
		bEnableExceptions = false;
		bUseRTTI = false;

		//  包含路径配置
		PublicIncludePaths.Add(ModuleDirectory + "/Public");
		PrivateIncludePaths.Add(ModuleDirectory + "/Private");

		//  公共依赖模块
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore",  // 用于访问静态网格体渲染数据
			"XToolsCore"  // XTools版本兼容层
		});

		//  编辑器专用依赖 - K2Node需要编辑器环境
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"BlueprintGraph",
				"KismetCompiler",
				"GraphEditor",
				"Slate",           // FSlateIcon
				"SlateCore",       // Slate基础类型
				"ToolMenus"        // FToolMenuSection, FUIAction等
			});
		}
		else
		{
			// 非编辑器构建时排除K2Node相关文件
			ExcludeSourceFiles.AddRange(new string[]
			{
				ModuleDirectory + "/Private/K2Node_PointSampling.cpp",
				ModuleDirectory + "/Private/K2Node_PointSamplingPinManager.cpp"
			});
		}

		//  添加模块定义
		PublicDefinitions.AddRange(new string[]
		{
			"WITH_POINTSAMPLING=1"
		});
	}
}

