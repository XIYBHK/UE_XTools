#pragma once

#include "CoreMinimal.h"

/**
 * XTools 统一错误/日志处理入口。
 * 调用者通过此类集中记录日志，并可选触发屏幕提示或编辑器 Message Log。
 * 
 * 支持所有 UE 日志分类类型，包括 Shipping 构建中的 FNoLoggingCategory。
 */
struct XTOOLS_API FXToolsErrorReporter
{
    // 模板方法 - 支持所有日志分类类型（包括 FNoLoggingCategory）
    template<typename LogCategoryType>
    static void Report(const LogCategoryType& Category,
                       ELogVerbosity::Type Verbosity,
                       const FString& Message,
                       const FName Context = NAME_None,
                       bool bNotifyOnScreen = false,
                       float DisplayTime = 5.0f);

    template<typename LogCategoryType>
    static void Error(const LogCategoryType& Category,
                      const FString& Message,
                      const FName Context = NAME_None,
                      bool bNotifyOnScreen = false,
                      float DisplayTime = 5.0f);

    template<typename LogCategoryType>
    static void Warning(const LogCategoryType& Category,
                        const FString& Message,
                        const FName Context = NAME_None,
                        bool bNotifyOnScreen = false,
                        float DisplayTime = 5.0f);

    template<typename LogCategoryType>
    static void Info(const LogCategoryType& Category,
                     const FString& Message,
                     const FName Context = NAME_None,
                     bool bNotifyOnScreen = false,
                     float DisplayTime = 5.0f);

private:
    // 内部实现方法
    static void ReportInternal(FLogCategoryBase* Category,
                              ELogVerbosity::Type Verbosity,
                              const FString& Message,
                              const FName Context,
                              bool bNotifyOnScreen,
                              float DisplayTime);
};

// 模板方法实现 - 必须在头文件中
template<typename LogCategoryType>
void FXToolsErrorReporter::Report(const LogCategoryType& Category,
                                  ELogVerbosity::Type Verbosity,
                                  const FString& Message,
                                  const FName Context,
                                  bool bNotifyOnScreen,
                                  float DisplayTime)
{
    // 获取日志分类指针
    // 对于 FLogCategoryBase 派生类，传递指针；对于 FNoLoggingCategory，传递 nullptr
    FLogCategoryBase* CategoryPtr = nullptr;
    
    // 使用 UE 的 TIsDerivedFrom 检查类型，兼容所有构建配置
    if constexpr (TIsDerivedFrom<LogCategoryType, FLogCategoryBase>::Value)
    {
        CategoryPtr = const_cast<FLogCategoryBase*>(static_cast<const FLogCategoryBase*>(&Category));
    }
    
    ReportInternal(CategoryPtr, Verbosity, Message, Context, bNotifyOnScreen, DisplayTime);
}

template<typename LogCategoryType>
void FXToolsErrorReporter::Error(const LogCategoryType& Category,
                                 const FString& Message,
                                 const FName Context,
                                 bool bNotifyOnScreen,
                                 float DisplayTime)
{
    Report(Category, ELogVerbosity::Error, Message, Context, bNotifyOnScreen, DisplayTime);
}

template<typename LogCategoryType>
void FXToolsErrorReporter::Warning(const LogCategoryType& Category,
                                   const FString& Message,
                                   const FName Context,
                                   bool bNotifyOnScreen,
                                   float DisplayTime)
{
    Report(Category, ELogVerbosity::Warning, Message, Context, bNotifyOnScreen, DisplayTime);
}

template<typename LogCategoryType>
void FXToolsErrorReporter::Info(const LogCategoryType& Category,
                                const FString& Message,
                                const FName Context,
                                bool bNotifyOnScreen,
                                float DisplayTime)
{
    Report(Category, ELogVerbosity::Log, Message, Context, bNotifyOnScreen, DisplayTime);
}


