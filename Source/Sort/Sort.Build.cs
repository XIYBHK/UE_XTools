using UnrealBuildTool;

/**
 * Sort 插件模块
 *
 * 提供高性能的排序算法和数组操作功能
 * 支持多种排序算法和类型安全的排序操作
 */
public class Sort : ModuleRules
{
	public Sort(ReadOnlyTargetRules Target) : base(Target)
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
			"WITH_SORT=1"
		});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		// --- 为编辑器添加必要的模块依赖 ---
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"BlueprintGraph",
					"GraphEditor",
					"KismetCompiler",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"InputCore"
				}
			);
		}
	}
}