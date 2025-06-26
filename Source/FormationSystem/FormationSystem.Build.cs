using UnrealBuildTool;

/**
 * FormationSystem Plugin Module
 * 
 * 专业的阵型变换和群集移动系统模块
 * 提供完整的RTS阵型管理、智能变换算法和Character移动支持
 */
public class FormationSystem : ModuleRules
{
	public FormationSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		// 预编译头文件设置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 添加模块定义
		PublicDefinitions.AddRange(new string[] {
			"WITH_FORMATIONSYSTEM=1",
			"DLLEXPORT=__declspec(dllexport)",
			"DLLIMPORT=__declspec(dllimport)"
		});

		// 编译优化设置
		bUsePrecompiled = false;
		bEnableUndefinedIdentifierWarnings = false;
		
		// 启用C++异常处理和RTTI（仅在确实需要时启用）
		bEnableExceptions = true;
		bUseRTTI = true;

		// Public包含路径
		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Public"
		});

		// Private包含路径  
		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Private"
		});

		// 公共依赖模块
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"AIModule",
			"Slate",
			"SlateCore",
			"UMG"
		});

		// 私有依赖模块
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Projects",
			"ApplicationCore", 
			"Json",
			"JsonUtilities",
			"DeveloperSettings"
		});

		// 编辑器专用依赖
		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"Kismet",
				"UnrealEd",
				"BlueprintGraph",
				"GraphEditor"
			});
		}

		// 模块定义
		PublicDefinitions.Add("WITH_FORMATIONSYSTEM=1");
	}
} 