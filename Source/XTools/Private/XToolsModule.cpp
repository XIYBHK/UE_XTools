#include "XToolsModule.h"
#include "XToolsDefines.h"

#define LOCTEXT_NAMESPACE "FXToolsModule"

DEFINE_LOG_CATEGORY(LogXTools);

void FXToolsModule::StartupModule()
{
}

void FXToolsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FXToolsModule, XTools)