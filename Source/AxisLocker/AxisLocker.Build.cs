/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

using UnrealBuildTool;

/**
 * AxisLocker 插件模块
 *
 * 提供运行时物理 Actor 轴向锁定（DOF）功能：硬锁/解锁、查询、预设、临时恢复
 */
public class AxisLocker : ModuleRules
{
	public AxisLocker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Default;
		IWYUSupport = IWYUSupport.Full;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDefinitions.AddRange(new string[] {
			"WITH_AXISLOCKER=1"
		});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"XToolsCore"  // XTools版本兼容层 + 统一错误处理
			}
		);
	}
}
