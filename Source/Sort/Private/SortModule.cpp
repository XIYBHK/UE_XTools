#include "SortModule.h"
#include "SortGraphPinFactory.h"
#include "EdGraphUtilities.h"

#define LOCTEXT_NAMESPACE "FSortModule"

void FSortModule::StartupModule()
{
    // 注册自定义引脚工厂
    SortGraphPinFactory = MakeShareable(new FSortGraphPinFactory());
    FEdGraphUtilities::RegisterVisualPinFactory(SortGraphPinFactory);
}

void FSortModule::ShutdownModule()
{
    // 注销引脚工厂
    if (SortGraphPinFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualPinFactory(SortGraphPinFactory);
        SortGraphPinFactory.Reset();
    }
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FSortModule, Sort) 