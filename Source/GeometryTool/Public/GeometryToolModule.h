/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGeometryToolModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};