using UnrealBuildTool;
using System.IO;

/**
 * XTools Plugin Module
 * 
 * Provides utility functions and tools for Unreal Engine projects
 */
public class XTools : ModuleRules
{
	public XTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_XTOOLS=1",
			"DLLEXPORT=__declspec(dllexport)",
			"DLLIMPORT=__declspec(dllimport)"
		});

		// ✅ UE5.3+ C++20 标准配置
		CppStandard = CppStandardVersion.Default;

		// ✅ IWYU 强制执行 - 提升编译速度和代码质量 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		// ✅ 开发时配置 - 确保代码质量
		bUseUnity = false;

		// ✅ UE 标准设置 - 符合引擎最佳实践
		bEnableExceptions = false;
		        bEnableExceptions = true;
        bUseRTTI = false;

		// 编译优化设置
		bUsePrecompiled = false;
		bEnableUndefinedIdentifierWarnings = false;

	// ✅ 简化的公共包含路径 - 移除不必要的引擎内部路径
	PublicIncludePaths.AddRange(new string[] {
		ModuleDirectory + "/Public"
	});

	// ✅ 简化的私有包含路径
	PrivateIncludePaths.AddRange(new string[] {
		ModuleDirectory + "/Private"
	});

		// Public dependencies
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG",
			"ComponentTimelineRuntime",
			"RandomShuffles",
			"FormationSystem"
		});

		// Private dependencies
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects",
			"ApplicationCore", 
			"Json",
			"JsonUtilities",
			"DeveloperSettings"
		});

		// Editor-only dependencies
		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"Kismet",
				"UnrealEd",
				"BlueprintGraph",
				"GraphEditor",
				"ComponentTimelineUncooked",
				"AssetRegistry",
				"KismetCompiler",
				"EditorStyle",
				"EditorWidgets"
			});
		}

		// Dynamically loaded modules
		DynamicallyLoadedModuleNames.AddRange(new string[] {
		});

		// ✅ 移除重复的定义 - WITH_XTOOLS=1 已在第17行定义
	}
}
