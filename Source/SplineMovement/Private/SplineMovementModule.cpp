#include "SplineMovementModule.h"
#include "SplineMovementLog.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FSplineMovementModule"

void FSplineMovementModule::StartupModule()
{
	UE_LOG(LogSplineMovement, Log, TEXT("SplineMovement 模块已启动"));
}

void FSplineMovementModule::ShutdownModule()
{
	UE_LOG(LogSplineMovement, Log, TEXT("SplineMovement 模块已关闭"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSplineMovementModule, SplineMovement)
