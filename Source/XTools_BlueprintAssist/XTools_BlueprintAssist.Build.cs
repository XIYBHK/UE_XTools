// Copyright fpwong. All Rights Reserved.

using UnrealBuildTool;

public class XTools_BlueprintAssist : ModuleRules
{
	public XTools_BlueprintAssist(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
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
				// ... add private dependencies that you statically link with here ...
				"GraphEditor",
				"Kismet",
				"KismetWidgets",
				"InputCore",
				"BlueprintGraph",
				"AssetTools",
				"EditorStyle",
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
				"CoreUObject",
				"Blutility", 
				"UMGEditor",
				"PropertyEditor",
				"ApplicationCore",
				"AudioEditor", 
				"HTTP",
				"XmlParser", 
				"ContentBrowserData",
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("MessageLog");
		}

		if (Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}

		if (Target.Version.MajorVersion >= 5)
		{
			PrivateDependencyModuleNames.Add("ContentBrowserData");
			PrivateDependencyModuleNames.Add("SubobjectEditor");
			PrivateDependencyModuleNames.Add("SubobjectDataInterface");
			PrivateDependencyModuleNames.Add("EditorFramework");
			PrivateDependencyModuleNames.Add("ContentBrowser");

			if (Target.Version.MinorVersion >= 4)
			{
				PrivateDependencyModuleNames.Add("ToolWidgets");
				PrivateDependencyModuleNames.Add("AssetDefinition");
				PrivateDependencyModuleNames.Add("MaterialEditor");
			}
			
			// AssetSearch 模块在 UE 5.4+ 可用
			if (Target.Version.MinorVersion >= 4)
			{
				PrivateDependencyModuleNames.Add("AssetSearch");
			}
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
