#include "XToolsModule.h"
#include "XToolsDefines.h"

#define LOCTEXT_NAMESPACE "FXToolsModule"

DEFINE_LOG_CATEGORY(LogXTools);

void FXToolsModule::StartupModule()
{
	// 所有XTools设置已统一迁移到X_AssetEditorSettings
	// 在项目设置 -> 插件 -> X Asset Editor 中配置
}

void FXToolsModule::ShutdownModule()
{
	// 清理代码（如果需要）
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FXToolsModule, XTools)