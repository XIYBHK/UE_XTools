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
		PublicDefinitions.Add("WITH_XTOOLS=1");

		//  UE5.3+ C++20 标准配置
		CppStandard = CppStandardVersion.Default;

		// 禁用未定义标识符警告
		// UE 5.5+ 废弃了 bEnableUndefinedIdentifierWarnings，改用 UndefinedIdentifierWarningLevel
		// 使用反射动态设置，避免编译时类型不存在的问题
		SetUndefinedIdentifierWarning(false);

		//  IWYU 强制执行 - 提升编译速度和代码质量 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		//  开发时配置 - 确保代码质量
		bUseUnity = false;

		//  UE 标准设置 - 符合引擎最佳实践
		bEnableExceptions = false;
		bUseRTTI = false;

		// 编译优化设置
		bUsePrecompiled = false;

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
		"XToolsCore",  // 核心工具和版本兼容性
		"XTools_ComponentTimelineRuntime",
		"RandomShuffles",
		// UE Geometry modules for native surface sampling
		"GeometryCore",           // FMeshSurfacePointSampling, FDynamicMesh3
		"MeshConversion",         // StaticMesh <-> DynamicMesh conversion
		"GeometryFramework"       // Geometry processing framework
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
		// M-19 依赖卫生：已移除零使用的 Kismet/GraphEditor/EditorStyle/EditorWidgets/AppFramework/ToolWidgets。
		// 勿凭印象重新引入：UE5.3 中 FKismetEditorUtilities(Kismet2/*.h) 与 CompileBlueprint 实现归属 UnrealEd；
		// 蓝图节点头(K2Node_*/EdGraphSchema_K2) 需 BlueprintGraph，其公开头引用的 KismetCompilerMisc.h 需显式声明 KismetCompiler。
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"BlueprintGraph",
				"XTools_ComponentTimelineUncooked",
				"AssetRegistry",
				"KismetCompiler"
			});

		}

		// Dynamically loaded modules
		DynamicallyLoadedModuleNames.AddRange(new string[] {
		});

	//  移除重复的定义 - WITH_XTOOLS=1 已在第17行定义
	}

	/// <summary>
	/// 设置未定义标识符警告（兼容 UE 5.3-5.7）
	/// UE 5.5+ 使用 UndefinedIdentifierWarningLevel，旧版本使用 bEnableUndefinedIdentifierWarnings
	/// </summary>
	private void SetUndefinedIdentifierWarning(bool bEnable)
	{
		// 尝试新 API (UE 5.5+)
		var newProp = GetType().GetProperty("UndefinedIdentifierWarningLevel");
		if (newProp != null)
		{
			// WarningLevel.Off = 0, WarningLevel.Warning = 1, WarningLevel.Error = 2
			var warningLevelType = newProp.PropertyType;
			var offValue = System.Enum.Parse(warningLevelType, bEnable ? "Warning" : "Off");
			newProp.SetValue(this, offValue);
			return;
		}

		// 回退到旧 API (UE 5.3/5.4)
		var oldProp = GetType().GetProperty("bEnableUndefinedIdentifierWarnings");
		if (oldProp != null)
		{
			oldProp.SetValue(this, bEnable);
		}
	}
}
