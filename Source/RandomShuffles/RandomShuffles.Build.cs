/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */

using UnrealBuildTool;

/**
 * RandomShuffles 插件模块
 *
 * 提供PRD算法和随机采样功能的运行时模块
 * 包含线程安全的伪随机分布算法和高效的数组采样工具
 */
public class RandomShuffles : ModuleRules
{
	public RandomShuffles(ReadOnlyTargetRules Target) : base(Target)
	{
		//  UE5.3+ 标准配置
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//  C++20 标准与引擎保持一致
		CppStandard = CppStandardVersion.Default;

		//  强制执行 IWYU 原则 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		//  开发时禁用 Unity Build，确保代码质量
		bUseUnity = false;

	//  UE 标准设置
	bEnableExceptions = false;
	bUseRTTI = false;

		//  添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_RANDOMSHUFFLES=1"
		});

		// 跨版本兼容：定义引擎版本宏（用于条件编译）
		PublicDefinitions.Add("ENGINE_MAJOR_VERSION=" + Target.Version.MajorVersion);
		PublicDefinitions.Add("ENGINE_MINOR_VERSION=" + Target.Version.MinorVersion);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"XToolsCore"  // XTools版本兼容层
			}
		);
	}
}
