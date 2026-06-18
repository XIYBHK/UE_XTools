/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "Modules/ModuleManager.h"
#include "AxisLockerAPI.h"

#define LOCTEXT_NAMESPACE "FAxisLockerModule"

DEFINE_LOG_CATEGORY(LogAxisLocker);

class FAxisLockerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogAxisLocker, Log, TEXT("AxisLocker module started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogAxisLocker, Log, TEXT("AxisLocker module shutdown"));
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAxisLockerModule, AxisLocker)
