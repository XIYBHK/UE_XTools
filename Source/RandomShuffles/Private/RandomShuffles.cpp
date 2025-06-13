/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#include "RandomShuffles.h"
#include "Modules/ModuleManager.h"

void FRandomShufflesModule::StartupModule()
{
	// 模块启动时的初始化逻辑
}

void FRandomShufflesModule::ShutdownModule()
{
	// 模块关闭时的清理逻辑
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRandomShufflesModule, RandomShuffles)