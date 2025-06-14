#include "SortModule.h"

#define LOCTEXT_NAMESPACE "FSortModule"

void FSortModule::StartupModule()
{
    // Runtime-only module, no editor-specific startup.
}

void FSortModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FSortModule, Sort) 