// ============================================
// 本模块源自第三方插件（个人使用集成）
// - 原作者：fpwong
// - 原插件：Blueprint Assist
// - 许可证：MIT License
// - 原始仓库：https://github.com/fpwong/BlueprintAssistPlugin
// - 修改内容：集成到XTools插件，适配UE 5.0+版本
// ============================================
// Copyright 2021 fpwong. All Rights Reserved.

using UnrealBuildTool;

public class BlueprintAssist : ModuleRules
{
	public BlueprintAssist(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		bUseUnity = false;

		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
				"ApplicationCore"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...
				"GraphEditor",
				"Kismet",
				"KismetWidgets",
				"InputCore",
				"BlueprintGraph",
				"AssetTools",
				"EditorWidgets",
				"UnrealEd",
				"Projects",
				"Json",
				"JsonUtilities",
				"EngineSettings",
				"AssetRegistry",
				"Persona",
				"WorkspaceMenuStructure",
				"ToolMenus",
				"UMG",
				"RenderCore",
				"DeveloperSettings",
				"Blutility", 
				"UMGEditor"
			}
		);

		// UE 5.0+ EditorStyle已废弃，但为了兼容性保留（代码中使用条件编译适配）
		if (Target.Version.MajorVersion < 5)
		{
			PrivateDependencyModuleNames.Add("EditorStyle");
		}

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("MessageLog");
		}

		if (Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}

		if (Target.Version.MajorVersion == 5)
		{
			PrivateDependencyModuleNames.Add("ContentBrowserData");
			PrivateDependencyModuleNames.Add("SubobjectEditor");
			PrivateDependencyModuleNames.Add("SubobjectDataInterface");
			PrivateDependencyModuleNames.Add("EditorFramework");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}