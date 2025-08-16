#include "Modules/ModuleManager.h"

class FObjectPoolEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FObjectPoolEditorModule, ObjectPoolEditor)

