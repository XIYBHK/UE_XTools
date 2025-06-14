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
private:
    /** 自定义引脚工厂 */
    TSharedPtr<class FGraphPanelPinFactory> SortGraphPinFactory;
#endif
};