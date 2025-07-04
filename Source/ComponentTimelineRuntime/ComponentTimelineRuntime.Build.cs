// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 组件时间轴运行时模块构建规则
 * 定义了模块的编译和链接设置
 */

using UnrealBuildTool;

public class ComponentTimelineRuntime : ModuleRules
{
	public ComponentTimelineRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"DeveloperSettings"
			}
		);
	}
}
