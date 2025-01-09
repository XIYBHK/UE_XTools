// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 组件时间轴未烘焙模块构建规则
 * 定义了编辑器模块的编译和链接设置
 */

using UnrealBuildTool;

public class ComponentTimelineUncooked : ModuleRules
{
	public ComponentTimelineUncooked(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// 公共包含路径设置
		PublicIncludePaths.AddRange(
			new string[] {
				// 在此处添加所需的公共包含路径
			}
			);
				
		// 私有包含路径设置
		PrivateIncludePaths.AddRange(
			new string[] {
				// 在此处添加所需的其他私有包含路径
			}
			);
			
		// 公共依赖模块设置
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", // 核心模块
				// 在此处添加其他需要静态链接的公共依赖项
			}
			);
			
		// 私有依赖模块设置
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",    // 核心对象系统
				"Engine",         // 引擎功能
				"Slate",         // UI系统
				"SlateCore",     // UI核心系统
				"BlueprintGraph", // 蓝图图表系统
				"UnrealEd",      // 虚幻编辑器
				"KismetCompiler", // 蓝图编译器
				"ComponentTimelineRuntime" // 组件时间轴运行时模块
				// 在此处添加需要静态链接的私有依赖项
			}
			);
		
		// 动态加载模块设置
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// 在此处添加模块需要动态加载的任何模块
			}
			);
	}
}
