// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class X_AssetEditor : ModuleRules
{
	public X_AssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		// ✅ UE5.3+ 标准配置
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// ✅ C++20 标准配置 - 与 UE5.3+ 引擎保持一致
		CppStandard = CppStandardVersion.Default;

		// ✅ IWYU 强制执行 - 提升编译速度和代码质量 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		// ✅ 开发时配置 - 确保代码质量
		bUseUnity = false;

		// ✅ UE 标准设置 - 符合引擎最佳实践
		bEnableExceptions = false;
		bUseRTTI = false;
		
		// ✅ 简化的包含路径 - 移除不必要的引擎内部路径
		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "Public")
		});

		PrivateIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "Private")
		});
			
		// ✅ 核心公共依赖 - 包含必需的模块
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"EditorStyle",
			"ContentBrowser",
			"PhysicsCore"  // ✅ 修复：X_CollisionBlueprintLibrary 需要 PhysicsCore
		});
			
		// ✅ 编辑器私有依赖 - 清理重复依赖，按功能分组
		PrivateDependencyModuleNames.AddRange(new string[] {
			// 核心编辑器模块
			"UnrealEd",
			"EditorFramework",
			"LevelEditor",
			"ToolMenus",

			// UI 模块
			"Slate",
			"SlateCore",
			"PropertyEditor",

			// 资产管理模块
			"AssetRegistry",
			"AssetTools",
			"EditorScriptingUtilities",
			"Blutility",

			// 材质和网格模块
			"MaterialEditor",
			"MeshDescription",
			"StaticMeshDescription",

			// 其他工具模块
			"Projects",
			"InputCore",
			"ApplicationCore",
			"DeveloperSettings"
		});
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
		
		// ✅ 编译器优化设置
		OptimizeCode = CodeOptimization.InShippingBuildsOnly;

		// ✅ 添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_X_ASSETEDITOR=1",
			"WITH_EDITOR=1",
			"UE_PLUGIN=1"
		});
	}
} 