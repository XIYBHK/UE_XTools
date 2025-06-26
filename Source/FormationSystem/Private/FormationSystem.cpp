#include "FormationSystem.h"
#include "FormationLog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FFormationSystemModule"

void FFormationSystemModule::StartupModule()
{
	// 模块启动时的初始化逻辑
	UE_LOG(LogFormationSystem, Log, TEXT("FormationSystem模块已启动"));
}

void FFormationSystemModule::ShutdownModule()
{
	// 模块关闭时的清理逻辑
	UE_LOG(LogFormationSystem, Log, TEXT("FormationSystem模块已关闭"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFormationSystemModule, FormationSystem) 