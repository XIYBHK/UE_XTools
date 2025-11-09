/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

using UnrealBuildTool;

/**
 * BlueprintExtensionsRuntime 模块
 *
 * 运行时蓝图功能库
 * 包含所有可在打包游戏中使用的BlueprintFunctionLibrary
 */
public class BlueprintExtensionsRuntime : ModuleRules
{
	public BlueprintExtensionsRuntime(ReadOnlyTargetRules Target) : base(Target)
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
			"WITH_BLUEPRINT_EXTENSIONS_RUNTIME=1"
		});

		// 公共依赖 - Runtime模块只需要基础依赖
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"XToolsCore"  // 需要 XToolsVersionCompat.h
			}
		);

		// 私有依赖
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GameplayTags"
			}
		);
	}
}

