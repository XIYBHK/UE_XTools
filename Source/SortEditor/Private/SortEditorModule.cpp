#include "SortEditorModule.h"
#include "SortGraphPinFactory.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "FSortEditorModule"

DEFINE_LOG_CATEGORY(LogSortEditor);

void FSortEditorModule::StartupModule()
{
    PinFactory = MakeShareable(new FSortGraphPinFactory());
    FEdGraphUtilities::RegisterVisualPinFactory(PinFactory);
    UE_LOG(LogSortEditor, Log, TEXT("SortEditor module started"));
}

void FSortEditorModule::ShutdownModule()
{
    if (PinFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualPinFactory(PinFactory);
        PinFactory.Reset();
    }
    UE_LOG(LogSortEditor, Log, TEXT("SortEditor module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSortEditorModule, SortEditor)