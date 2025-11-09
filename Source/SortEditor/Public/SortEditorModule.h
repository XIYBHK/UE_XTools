#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// SortEditor 模块日志分类
DECLARE_LOG_CATEGORY_EXTERN(LogSortEditor, Log, All);

struct FGraphPanelPinFactory;

class FSortEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TSharedPtr<FGraphPanelPinFactory> PinFactory;
};