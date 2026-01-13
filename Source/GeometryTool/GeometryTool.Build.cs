/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 */

using UnrealBuildTool;

public class GeometryTool : ModuleRules
{
    public GeometryTool(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "XToolsCore"
        });

        // KismetMathLibrary 在 Engine 模块中，无需额外依赖 Kismet
        // PrivateDependencyModuleNames.AddRange(new string[] { });

        // 支持的平台（与 PointSampling 保持一致）
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Windows 平台完整功能
        }
        else if (Target.Platform == UnrealTargetPlatform.Android || 
                 Target.Platform == UnrealTargetPlatform.IOS)
        {
            // 移动平台支持（可选，取决于功能需求）
        }
    }
}