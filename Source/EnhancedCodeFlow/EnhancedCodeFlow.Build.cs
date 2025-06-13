// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

using UnrealBuildTool;

public class EnhancedCodeFlow : ModuleRules
{
	public EnhancedCodeFlow(ReadOnlyTargetRules Target) : base(Target)
	{
        // 确保 IWYU（Include What You Use）
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // 确保支持默认的 C++ 标准（UE5.3 及以上版本为 CPP20）
        CppStandard = CppStandardVersion.Default;

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
