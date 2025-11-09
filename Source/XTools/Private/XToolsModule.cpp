#include "XToolsModule.h"

#define LOCTEXT_NAMESPACE "FXToolsModule"

DEFINE_LOG_CATEGORY(LogXTools);

void FXToolsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FXToolsModule::ShutdownModule()
{
	// 清理代码（如果需要）
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FXToolsModule, XTools)
