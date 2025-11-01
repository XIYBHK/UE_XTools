// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ObjectPool : ModuleRules
{
    public ObjectPool(ReadOnlyTargetRules Target) : base(Target)
    {
        //  遵循UE最佳实践的编译配置
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        IWYUSupport = IWYUSupport.Full;
        
        //  开发时禁用Unity构建，确保代码质量
        bUseUnity = false;
        
        //  遵循UE标准设置
        bEnableExceptions = false;
        bUseRTTI = false;
        
        //  核心运行时依赖 - 最小化原则
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "XTools"  // UE版本兼容性支持
        });
        
        //  私有依赖 - 内部实现需要
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DeveloperSettings"  // 配置系统需要
        });

        //  测试支持 - 暂时禁用以简化编译
        PublicDefinitions.Add("WITH_OBJECTPOOL_TESTS=0");
        
        //  编辑器功能作为可选依赖
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "ToolMenus",
                "EditorStyle"
            });
        }
        
        //  公共包含路径
        PublicIncludePaths.AddRange(new string[]
        {
            // 使用相对路径，遵循UE约定
        });
        
        //  私有包含路径
        PrivateIncludePaths.AddRange(new string[]
        {
            // 使用相对路径，遵循UE约定
        });
        
        //  不再手动定义 OBJECTPOOL_API，避免与 UBT 生成的宏冲突
        
        //  优化设置
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
