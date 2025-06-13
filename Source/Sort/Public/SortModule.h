#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FGraphPanelPinFactory;

class FSortModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    /** 自定义引脚工厂 */
    TSharedPtr<FGraphPanelPinFactory> SortGraphPinFactory;
};