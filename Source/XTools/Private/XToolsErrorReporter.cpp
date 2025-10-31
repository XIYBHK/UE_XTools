#include "XToolsErrorReporter.h"

#include "Engine/Engine.h"
#include "Misc/OutputDevice.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#endif

namespace
{
    FString BuildFullMessage(const FString& Message, const FName Context)
    {
        if (Context.IsNone())
        {
            return Message;
        }

        return FString::Printf(TEXT("[%s] %s"), *Context.ToString(), *Message);
    }

    FColor ResolveColor(const ELogVerbosity::Type Verbosity)
    {
        switch (Verbosity)
        {
        case ELogVerbosity::Error:
            return FColor::Red;
        case ELogVerbosity::Warning:
            return FColor::Yellow;
        default:
            return FColor::White;
        }
    }
}

void FXToolsErrorReporter::ReportInternal(FLogCategoryBase* Category,
                                          ELogVerbosity::Type Verbosity,
                                          const FString& Message,
                                          const FName Context,
                                          bool bNotifyOnScreen,
                                          float DisplayTime)
{
    const FString FullMessage = BuildFullMessage(Message, Context);

    // 只在有有效日志分类时记录日志
    if (Category)
    {
        FMsg::Logf(__FILE__, __LINE__, Category->GetCategoryName(), Verbosity, TEXT("%s"), *FullMessage);
    }

    if (bNotifyOnScreen && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(INDEX_NONE, DisplayTime, ResolveColor(Verbosity), FullMessage);
    }

#if WITH_EDITOR
    {
        FMessageLog EditorLog("XTools");
        switch (Verbosity)
        {
        case ELogVerbosity::Error:
            EditorLog.Error(FText::FromString(FullMessage));
            break;
        case ELogVerbosity::Warning:
            EditorLog.Warning(FText::FromString(FullMessage));
            break;
        default:
            EditorLog.Info(FText::FromString(FullMessage));
            break;
        }
    }
#endif
}


