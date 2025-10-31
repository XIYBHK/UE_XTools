#pragma once

#include "CoreMinimal.h"

/**
 * XTools 统一错误/日志处理入口。
 * 调用者通过此类集中记录日志，并可选触发屏幕提示或编辑器 Message Log。
 */
struct XTOOLS_API FXToolsErrorReporter
{
    static void Report(FLogCategoryBase& Category,
                       ELogVerbosity::Type Verbosity,
                       const FString& Message,
                       const FName Context = NAME_None,
                       bool bNotifyOnScreen = false,
                       float DisplayTime = 5.0f);

    static void Error(FLogCategoryBase& Category,
                      const FString& Message,
                      const FName Context = NAME_None,
                      bool bNotifyOnScreen = false,
                      float DisplayTime = 5.0f);

    static void Warning(FLogCategoryBase& Category,
                        const FString& Message,
                        const FName Context = NAME_None,
                        bool bNotifyOnScreen = false,
                        float DisplayTime = 5.0f);

    static void Info(FLogCategoryBase& Category,
                     const FString& Message,
                     const FName Context = NAME_None,
                     bool bNotifyOnScreen = false,
                     float DisplayTime = 5.0f);
};


