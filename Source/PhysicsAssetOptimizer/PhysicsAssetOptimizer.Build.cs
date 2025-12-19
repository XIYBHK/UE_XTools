/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

using UnrealBuildTool;

public class PhysicsAssetOptimizer : ModuleRules
{
	public PhysicsAssetOptimizer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",              // FPhysicsAssetUtils
			"PhysicsUtilities",      // CreateCollisionFromBone
			"MeshUtilitiesCommon",   // FBoneVertInfo
			"MeshUtilitiesEngine",   // CalcBoneVertInfos
			"Slate",
			"SlateCore",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EditorStyle",
			"Persona",            // 物理资产编辑器
			"PhysicsAssetEditor", // 物理资产编辑器扩展
			"ToolMenus",          // 工具栏菜单系统
			"GeometryCore",       // FDynamicMesh3（LSV 生成需要）
			"EditorSubsystem"     // UAssetEditorSubsystem
		});

		// ⚠️ 注意：PhysicsUtilities 是 Developer 模块，包含 Private 头文件
		// 需要添加私有包含路径访问 LevelSetHelpers.h
		PrivateIncludePaths.AddRange(new string[]
		{
			"Developer/PhysicsUtilities/Private"  // 访问 LevelSetHelpers.h
		});
	}
}
