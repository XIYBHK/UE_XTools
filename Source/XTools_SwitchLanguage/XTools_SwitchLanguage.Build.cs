// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * XTools_SwitchLanguage Module
 * 
 * 提供语言切换功能，支持运行时动态切换本地化语言
 */
public class XTools_SwitchLanguage : ModuleRules
{
	public XTools_SwitchLanguage(ReadOnlyTargetRules Target) : base(Target)
	{
		//  UE5.3+ 标准配置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		//  C++20 标准配置 - 与 UE5.3+ 引擎保持一致
		CppStandard = CppStandardVersion.Default;

		//  IWYU 强制执行 - 提升编译速度和代码质量
		IWYUSupport = IWYUSupport.Full;

		//  开发时配置 - 确保代码质量
		bUseUnity = false;

		//  UE 标准设置 - 符合引擎最佳实践
		bEnableExceptions = false;
		bUseRTTI = false;

		//  包含路径配置
		PublicIncludePaths.Add(ModuleDirectory + "/Public");
		PrivateIncludePaths.Add(ModuleDirectory + "/Private");

		//  公共依赖模块
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		//  私有依赖模块 - 编辑器工具栏功能
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			"InputCore",
			"EditorFramework",
			"UnrealEd",
			"ToolMenus",
			"Slate",
			"SlateCore",
			"Kismet"  // 蓝图刷新功能
		});

		//  添加模块定义
		PublicDefinitions.AddRange(new string[]
		{
			"WITH_XTOOLS_SWITCHLANGUAGE=1"
		});
	}
}
