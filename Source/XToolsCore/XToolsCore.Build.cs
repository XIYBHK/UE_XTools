/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

using UnrealBuildTool;

/**
 * XToolsCore 模块 (Runtime)
 * 
 * 提供跨版本兼容性和核心工具函数
 * 所有 Runtime 模块都可以安全依赖此模块
 * 
 * 包含：
 * - XToolsVersionCompat.h: UE 5.3-5.6 版本兼容性宏
 * - XToolsErrorReporter.h: 统一错误/日志处理
 * - XToolsDefines.h: 插件版本和通用宏定义
 */
public class XToolsCore : ModuleRules
{
	public XToolsCore(ReadOnlyTargetRules Target) : base(Target)
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
			"WITH_XTOOLS_CORE=1"
		});

		// UE版本宏定义（用于跨版本兼容性系统）
		// 确保在所有构建环境中都能正确检测版本
		bool bHasMajorVersion = false;
		bool bHasMinorVersion = false;
		
		foreach (string Def in PublicDefinitions)
		{
			if (Def.StartsWith("ENGINE_MAJOR_VERSION="))
			{
				bHasMajorVersion = true;
			}
			if (Def.StartsWith("ENGINE_MINOR_VERSION="))
			{
				bHasMinorVersion = true;
			}
		}
		
		if (!bHasMajorVersion)
		{
			PublicDefinitions.Add("ENGINE_MAJOR_VERSION=" + Target.Version.MajorVersion);
		}
		if (!bHasMinorVersion)
		{
			PublicDefinitions.Add("ENGINE_MINOR_VERSION=" + Target.Version.MinorVersion);
		}

		// 公共依赖 - Runtime 模块只需要最基础的依赖
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		// 私有依赖
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
