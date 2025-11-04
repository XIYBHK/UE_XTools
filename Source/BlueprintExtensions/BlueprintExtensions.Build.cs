/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

using UnrealBuildTool;

/**
 * BlueprintExtensions 模块（Editor-Only）
 *
 * 提供蓝图编辑器增强功能
 * 包含：
 * - 自定义K2节点（ForLoop、ForEach系列、Assign、Map嵌套操作等）
 * 
 * 注意：运行时功能库已移至 BlueprintExtensionsRuntime 模块
 */
public class BlueprintExtensions : ModuleRules
{
	public BlueprintExtensions(ReadOnlyTargetRules Target) : base(Target)
	{
		// UE5.3+ 标准配置
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// C++20 标准与引擎保持一致
		CppStandard = CppStandardVersion.Default;

		// 强制执行 IWYU 原则 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		// 开发时禁用 Unity Build，确保代码质量
		bUseUnity = false;

		// UE 标准设置
		bEnableExceptions = false;
		bUseRTTI = false;

		// 添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_BLUEPRINT_EXTENSIONS=1"
		});

	// 公共依赖
	PublicDependencyModuleNames.AddRange(
		new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"BlueprintExtensionsRuntime"  // K2Nodes需要调用Runtime模块中的函数库
		}
	);

		// 私有依赖
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"GameplayTags"
			}
		);

		// K2节点需要的编辑器模块
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"BlueprintGraph",
					"KismetCompiler",
					"GraphEditor",
					"KismetWidgets",
					"EditorStyle"
				}
			);
		}
	}
}

