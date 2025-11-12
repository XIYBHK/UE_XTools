/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSortModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

#if WITH_EDITOR
// 运行时模块不持有编辑器工厂，移除此成员以保持分层清晰
#endif
};
