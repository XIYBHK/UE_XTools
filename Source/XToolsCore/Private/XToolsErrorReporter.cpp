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
                                          float DisplayTime,
                                          const ANSICHAR* File,
                                          int32 Line)
{
    const FString FullMessage = BuildFullMessage(Message, Context);

    // 只在有有效日志分类时记录日志
    if (Category)
    {
        // 如果调用者通过宏传入了有效的文件/行号信息，使用调用者的位置；
        // 否则回退到当前文件位置（向后兼容直接调用静态方法的情况）
        const ANSICHAR* LogFile = (File && File[0] != '\0') ? File : __FILE__;
        const int32 LogLine = (Line > 0) ? Line : __LINE__;
        FMsg::Logf(LogFile, LogLine, Category->GetCategoryName(), Verbosity, TEXT("%s"), *FullMessage);
    }

    if (bNotifyOnScreen && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(INDEX_NONE, DisplayTime, ResolveColor(Verbosity), FullMessage);
    }

#if WITH_EDITOR
    {
        FMessageLog EditorLog("XToolsCore");
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
