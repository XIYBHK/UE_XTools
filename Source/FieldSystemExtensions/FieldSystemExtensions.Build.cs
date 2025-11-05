// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * FieldSystemExtensions 模块（Runtime）
 *
 * 提供增强的Field System功能
 * 包含：
 * - AXFieldSystemActor：支持高级筛选的Field Actor
 *   - 作用类型筛选
 *   - 排除类型筛选
 *   - Actor Tag筛选
 * 
 * 完全兼容原生AFieldSystemActor，可直接替换蓝图父类
 */
public class FieldSystemExtensions : ModuleRules
{
	public FieldSystemExtensions(ReadOnlyTargetRules Target) : base(Target)
	{
		// UE5.3+ 标准配置
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		IWYUSupport = IWYUSupport.Full;
		bUseUnity = false;
		
		PublicDefinitions.Add("WITH_FIELDSYSTEMEXTENSIONS=1");
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"FieldSystemEngine",  // Field System核心模块
				"Chaos",  // Chaos物理系统
				"GeometryCollectionEngine",  // GeometryCollection组件
				"ChaosSolverEngine"  // Chaos Solver模块
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// 根据需要添加私有依赖
			}
		);
	}
}

