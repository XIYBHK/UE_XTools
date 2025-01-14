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
};
