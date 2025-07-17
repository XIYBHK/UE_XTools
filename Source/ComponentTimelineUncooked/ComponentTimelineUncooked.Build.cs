// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * ComponentTimelineUncooked 插件模块
 *
 * 提供组件时间轴的编辑器功能
 * 支持时间轴的可视化编辑和调试工具
 */

using UnrealBuildTool;

public class ComponentTimelineUncooked : ModuleRules
{
	public ComponentTimelineUncooked(ReadOnlyTargetRules Target) : base(Target)
	{
		// ✅ UE5.3+ 标准配置
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// ✅ C++20 标准与引擎保持一致
		CppStandard = CppStandardVersion.Default;

		// ✅ 强制执行 IWYU 原则
		bEnforceIWYU = true;

		// ✅ 开发时禁用 Unity Build，确保代码质量
		bUseUnity = false;

		// ✅ UE 标准设置
		bEnableExceptions = false;
		bUseRTTI = false;

		// ✅ 添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_COMPONENTTIMELINEUNCOOKED=1"
		});

		// 公共依赖模块
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		// 私有依赖模块
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"BlueprintGraph",
				"UnrealEd",
				"KismetCompiler",
				"ComponentTimelineRuntime"
			}
		);
	}
}
