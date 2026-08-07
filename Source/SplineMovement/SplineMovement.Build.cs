using UnrealBuildTool;

/**
 * SplineMovement 模块 - 样条线路径移动异步节点
 * 支持 Character 移动输入和 AI 寻路两种模式
 */
public class SplineMovement : ModuleRules
{
	public SplineMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		// UE5.3+ 标准配置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 添加模块定义
		PublicDefinitions.Add("WITH_SPLINEMOVEMENT=1");

		// UE5.3+ C++20 标准配置
		CppStandard = CppStandardVersion.Default;

		// IWYU 强制执行 - 提升编译速度和代码质量 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		// 开发时配置 - 确保代码质量
		bUseUnity = false;

		// UE 标准设置 - 符合引擎最佳实践
		bEnableExceptions = false;
		bUseRTTI = false;

		// 编译优化设置
		bUsePrecompiled = false;

		// Public 包含路径
		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Public"
		});

		// Private 包含路径
		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Private"
		});

		// 公共依赖模块
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"XToolsCore"
		});

		// 私有依赖模块
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects"
		});
	}
}
