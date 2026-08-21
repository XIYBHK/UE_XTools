/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRandomShufflesModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 世界清理委托句柄 - ShutdownModule 时必须移除，兼容 Live Coding/模块卸载 */
	FDelegateHandle PRDWorldCleanupHandle;
};
