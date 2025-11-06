// ============================================
// 本模块源自第三方插件（个人使用集成）
// - 原作者：fpwong
// - 原插件：AutoSizeComments
// - 许可证：CC-BY-4.0 (Creative Commons Attribution 4.0 International)
// - 原始仓库：https://github.com/fpwong/AutoSizeComments
// - 修改内容：集成到XTools插件，适配UE 5.0+版本
// ============================================
// Copyright 2021 fpwong. All Rights Reserved.

using UnrealBuildTool;

public class AutoSizeComments : ModuleRules
{
	public AutoSizeComments(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		bUseUnity = false;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "GraphEditor",
                "BlueprintGraph",
                "UnrealEd",
                "InputCore",
                "Projects",
                "Json",
                "JsonUtilities",
                "EngineSettings",
                "AssetRegistry"
            }
            );

		// UE 5.0+ EditorStyle已废弃，但为了兼容性保留（代码中使用宏适配）
		if (Target.Version.MajorVersion < 5)
		{
			PrivateDependencyModuleNames.Add("EditorStyle");
		}
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
