// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * PointSamplingEditor Module
 * 
 * 为PointSampling模块提供编辑器专用功能，包括自定义K2Node节点
 */
public class PointSamplingEditor : ModuleRules
{
	public PointSamplingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		//  UE5.3+ 标准配置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		//  C++20 标准配置
		CppStandard = CppStandardVersion.Default;

		//  IWYU 强制执行
		IWYUSupport = IWYUSupport.Full;

		//  开发时配置
		bUseUnity = false;

		//  UE 标准设置
		bEnableExceptions = false;
		bUseRTTI = false;

		//  公共依赖模块
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PointSampling",  // 依赖运行时模块
			"XToolsCore"      // XTools版本兼容层
		});

		//  私有依赖模块 - 编辑器专用
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"BlueprintGraph",
			"KismetCompiler",
			"GraphEditor",
			"Slate",
			"SlateCore",
			"ToolMenus"
		});
	}
}
