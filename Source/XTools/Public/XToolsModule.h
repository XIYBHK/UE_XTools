#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// XTools 模块日志分类
DECLARE_LOG_CATEGORY_EXTERN(LogXTools, Log, All);

class FXToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};