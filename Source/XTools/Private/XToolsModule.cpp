#include "XToolsModule.h"
#include "Modules/ModuleManager.h"

class FXToolsModule : public IXToolsModule
{
public:
	virtual void StartupModule() override
	{
		// 模块启动时的初始化代码
		UE_LOG(LogTemp, Log, TEXT("XTools Module Startup"));
	}

	virtual void ShutdownModule() override
	{
		// 模块关闭时的清理代码
		UE_LOG(LogTemp, Log, TEXT("XTools Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FXToolsModule, XTools)