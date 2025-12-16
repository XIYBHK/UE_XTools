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
		//  UE5.3+ 标准配置
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		//  添加模块定义
		PublicDefinitions.Add("WITH_FORMATIONSYSTEM=1");

		// 兼容旧代码中的 DLLEXPORT/DLLIMPORT 宏（非 UE 最佳实践，但为避免破坏性改动保留）
		// 注意：在非 Windows 平台上 __declspec 不可用，必须定义为空以保证可编译（本模块允许 Win64/Mac/Linux）。
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PublicDefinitions.Add("DLLEXPORT=__declspec(dllexport)");
			PublicDefinitions.Add("DLLIMPORT=__declspec(dllimport)");
		}
		else
		{
			PublicDefinitions.Add("DLLEXPORT=");
			PublicDefinitions.Add("DLLIMPORT=");
		}

		//  UE5.3+ C++20 标准配置
		CppStandard = CppStandardVersion.Default;

		//  IWYU 强制执行 - 提升编译速度和代码质量 (UE5.2+)
		IWYUSupport = IWYUSupport.Full;

		//  开发时配置 - 确保代码质量
		bUseUnity = false;

		//  UE 标准设置 - 符合引擎最佳实践
		bEnableExceptions = false;
		bUseRTTI = false;

		// 编译优化设置
		bUsePrecompiled = false;

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
	}
} 
