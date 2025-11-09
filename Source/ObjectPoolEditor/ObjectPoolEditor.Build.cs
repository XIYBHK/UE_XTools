using UnrealBuildTool;

/**
 * ObjectPoolEditor 插件模块
 * 
 * 提供对象池的编辑器工具和蓝图节点
 */
public class ObjectPoolEditor : ModuleRules
{
    public ObjectPoolEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        // UE5.3+ 标准配置
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // C++20 标准与引擎保持一致
        CppStandard = CppStandardVersion.Default;
        
        // 强制执行 IWYU 原则 (UE5.2+)
        IWYUSupport = IWYUSupport.Full;
        
        // 开发时禁用 Unity Build，确保代码质量
        bUseUnity = false;
        
        // UE 标准设置
        bEnableExceptions = false;
        bUseRTTI = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject", 
            "Engine",
            "ObjectPool"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "BlueprintGraph",
            "KismetCompiler",
            "Slate",
            "SlateCore"
        });
    }
}
