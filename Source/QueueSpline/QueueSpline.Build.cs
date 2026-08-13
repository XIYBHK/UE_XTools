using UnrealBuildTool;

public class QueueSpline : ModuleRules
{
	public QueueSpline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		CppStandard = CppStandardVersion.Default;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDefinitions.Add("WITH_QUEUESPLINE=1");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"XToolsCore"
		});
	}
}
