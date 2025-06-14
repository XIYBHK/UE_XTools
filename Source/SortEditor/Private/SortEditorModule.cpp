#include "SortEditorModule.h"
#include "SortGraphPinFactory.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "FSortEditorModule"

void FSortEditorModule::StartupModule()
{
    PinFactory = MakeShareable(new FSortGraphPinFactory());
    FEdGraphUtilities::RegisterVisualPinFactory(PinFactory);
}

void FSortEditorModule::ShutdownModule()
{
    if (PinFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualPinFactory(PinFactory);
        PinFactory.Reset();
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSortEditorModule, SortEditor) 