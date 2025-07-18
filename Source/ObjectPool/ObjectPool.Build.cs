// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ObjectPool : ModuleRules
{
    public ObjectPool(ReadOnlyTargetRules Target) : base(Target)
    {
        // ✅ 遵循UE最佳实践的编译配置
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnforceIWYU = true;
        
        // ✅ 开发时禁用Unity构建，确保代码质量
        bUseUnity = false;
        
        // ✅ 遵循UE标准设置
        bEnableExceptions = false;
        bUseRTTI = false;
        
        // ✅ 核心运行时依赖 - 最小化原则
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
        
        // ✅ 私有依赖 - 内部实现需要
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DeveloperSettings"  // 配置系统需要
        });

        // ✅ 测试支持 - 基于UE5内置测试框架
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "AutomationController",  // 自动化测试控制器
                "UnrealEd"              // 测试需要编辑器支持
            });

            PublicDefinitions.Add("WITH_OBJECTPOOL_TESTS=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_OBJECTPOOL_TESTS=0");
        }
        
        // ✅ 编辑器功能作为可选依赖
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "ToolMenus",
                "EditorStyle"
            });
        }
        
        // ✅ 公共包含路径
        PublicIncludePaths.AddRange(new string[]
        {
            // 使用相对路径，遵循UE约定
        });
        
        // ✅ 私有包含路径
        PrivateIncludePaths.AddRange(new string[]
        {
            // 使用相对路径，遵循UE约定
        });
        
        // ✅ 定义预处理器宏
        PublicDefinitions.AddRange(new string[]
        {
            "OBJECTPOOL_API=DLLEXPORT"
        });
        
        // ✅ 优化设置
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            PublicDefinitions.Add("OBJECTPOOL_SHIPPING=1");
        }
        else
        {
            PublicDefinitions.Add("OBJECTPOOL_SHIPPING=0");
        }
    }
}
