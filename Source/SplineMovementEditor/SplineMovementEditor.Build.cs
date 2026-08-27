using UnrealBuildTool;

/**
 * SplineMovementEditor 模块
 *
 * 提供样条线移动的编辑器工具和自定义蓝图节点
 */
public class SplineMovementEditor : ModuleRules
{
	public SplineMovementEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		// UE5.3+ 标准配置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// C++20 标准与引擎保持一致
		CppStandard = CppStandardVersion.Default;

		// 强制执行 IWYU 原则 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		// 开发时禁用 Unity Build，确保代码质量
		bUseUnity = false;

		// UE 标准设置
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SplineMovement"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"BlueprintGraph",
			"KismetCompiler",
			"Slate",
			"SlateCore"
		});
	}
}
