#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FGraphPanelPinFactory;

class FSortEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TSharedPtr<FGraphPanelPinFactory> PinFactory;
}; 