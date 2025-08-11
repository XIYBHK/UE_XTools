using UnrealBuildTool;

/**
 * SortEditor 插件模块
 *
 * 提供排序功能的编辑器工具和测试界面
 * 包含排序算法的可视化测试和性能分析工具
 */
public class SortEditor : ModuleRules
{
    public SortEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        // ✅ UE5.3+ 标准配置
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // ✅ C++20 标准与引擎保持一致
        CppStandard = CppStandardVersion.Default;

        // ✅ 强制执行 IWYU 原则 (UE5.2+)
        IWYUSupport = IWYUSupport.Full;

        // ✅ 开发时禁用 Unity Build，确保代码质量
        bUseUnity = false;

        // ✅ UE 标准设置
        bEnableExceptions = false;
        bEnableExceptions = true;
        bUseRTTI = false;

        // 仅在编辑器中编译此模块
        if (!Target.bBuildEditor)
        {
            return;
        }

        // ✅ 添加模块定义
        PublicDefinitions.AddRange(new string[] {
            "WITH_SORTEDITOR=1"
        });

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Sort",            // 依赖运行时排序模块
                "UnrealEd",
                "BlueprintGraph",
                "GraphEditor",
                "KismetCompiler",
                "InputCore",
                "EditorStyle"
            }
        );
    }
} 