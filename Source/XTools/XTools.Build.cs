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

		// UE 5.2 兼容性修复：禁用将警告视为错误
		// 解决 VS 2022 新版本编译器与 UE 5.2 的兼容性问题
		bTreatWarningsAsErrors = false;

		// 添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_XTOOLS=1",
			"DLLEXPORT=__declspec(dllexport)",
			"DLLIMPORT=__declspec(dllimport)"
		});

		// UE版本宏定义（用于跨版本兼容性系统）
		// 
		// UE最佳实践说明：
		// - UE引擎通常会自动定义 ENGINE_MAJOR_VERSION 和 ENGINE_MINOR_VERSION
		// - 但为了确保在所有构建环境中都能正确检测版本，我们显式定义它们
		// - 使用 Target.Version 自动获取当前编译的引擎版本，无需手动设置
		// - 这种方式确保了插件在不同UE版本中都能正确编译和运行
		//
		// 参考：EnhancedCodeFlow模块在Build.cs中直接使用Target.Version进行比较
		// 但为了在头文件中使用条件编译，我们需要将这些值定义为宏
		//
		// 注意：先检查是否已定义（避免重复定义）
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

		//  UE5.3+ C++20 标准配置
		CppStandard = CppStandardVersion.Default;

		//  IWYU 强制执行 - 提升编译速度和代码质量 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		//  开发时配置 - 确保代码质量
		bUseUnity = false;

	//  UE 标准设置 - 符合引擎最佳实践
	bEnableExceptions = false;
	bUseRTTI = false;

		// 编译优化设置
		bUsePrecompiled = false;
		bEnableUndefinedIdentifierWarnings = false;

	//  简化的公共包含路径 - 移除不必要的引擎内部路径
	PublicIncludePaths.AddRange(new string[] {
		ModuleDirectory + "/Public"
	});

	//  简化的私有包含路径
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

			PrivateDependencyModuleNames.AddRange(new string[] {
				"MessageLog"
			});
		}

		// Dynamically loaded modules
		DynamicallyLoadedModuleNames.AddRange(new string[] {
		});

		//  移除重复的定义 - WITH_XTOOLS=1 已在第17行定义
	}
}
