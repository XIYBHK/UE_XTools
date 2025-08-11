// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

using UnrealBuildTool;

/**
 * EnhancedCodeFlow 插件模块
 *
 * 提供增强的代码流控制功能，包括异步操作、延迟执行、协程等
 * 支持高性能的时间轴控制和复杂的异步任务管理
 */
public class EnhancedCodeFlow : ModuleRules
{
	public EnhancedCodeFlow(ReadOnlyTargetRules Target) : base(Target)
	{
        // ✅ UE5.3+ 标准配置
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // ✅ C++20 标准与引擎保持一致
        CppStandard = CppStandardVersion.Default;

        // ✅ 强制执行 IWYU 原则 (UE5.2+)
        IWYUSupport = IWYUSupport.Full;

        // ✅ 开发时禁用 Unity Build，确保代码质量
        bUseUnity = false;

        // ✅ UE 标准设置
        bEnableExceptions = false;
        bEnableExceptions = true;
        bUseRTTI = false;

        // ✅ 添加模块定义
        PublicDefinitions.AddRange(new string[] {
            "WITH_ENHANCEDCODEFLOW=1"
        });

        // 必需的模块依赖
        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

        // 确保没有重复的定义
        PublicDefinitions.RemoveAll(ECFDefinition => ECFDefinition.StartsWith("ECF_"));

        // 允许在非发布版本中禁用优化（便于调试）
        bool bDisableOptimization = false;
        if (bDisableOptimization && (Target.Configuration != UnrealTargetConfiguration.Shipping))
        {
            if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 2)
            {
                PublicDefinitions.Add("ECF_PRAGMA_DISABLE_OPTIMIZATION=UE_DISABLE_OPTIMIZATION");
                PublicDefinitions.Add("ECF_PRAGMA_ENABLE_OPTIMIZATION=UE_ENABLE_OPTIMIZATION");
            }
            else
            {
                PublicDefinitions.Add("ECF_PRAGMA_DISABLE_OPTIMIZATION=PRAGMA_DISABLE_OPTIMIZATION");
                PublicDefinitions.Add("ECF_PRAGMA_ENABLE_OPTIMIZATION=PRAGMA_ENABLE_OPTIMIZATION");
            }
        }
        else
        {
            PublicDefinitions.Add("ECF_PRAGMA_DISABLE_OPTIMIZATION=");
            PublicDefinitions.Add("ECF_PRAGMA_ENABLE_OPTIMIZATION=");
        }

        // 启用或禁用 Unreal Insight 分析器的额外跟踪
        bool bEnableInsightProfiling = true;
        if (bEnableInsightProfiling)
        {
            PublicDefinitions.Add("ECF_INSIGHT_PROFILING=1");
        }
        else
        {
            PublicDefinitions.Add("ECF_INSIGHT_PROFILING=0");
        }
    }
}
