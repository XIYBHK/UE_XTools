#pragma once

#include "CoreMinimal.h"

/**
 * XTools 统一错误/日志处理入口。
 * 调用者通过此类集中记录日志，并可选触发屏幕提示或编辑器 Message Log。
 * 
 * 使用范围与复杂度规范：
 * - 适用场景：
 *   - 插件/工具代码中需要统一风格的错误与警告输出
 *   - Blueprint Library / 对外 API 需要在不中断执行的前提下报告问题
 * - 不推荐的用法：
 *   - 不要用它实现复杂的异常系统或全局状态机
 *   - 不要在简单的、本地调试代码中强行替代所有 UE_LOG（直接 UE_LOG 更直观）
 * - 组合方式：
 *   - 对于“理论不会发生”的逻辑错误，可以结合 ensure/ensureMsgf 使用，捕获调用栈
 *   - 对于可预期的运行时失败，仅使用 FXToolsErrorReporter::Warning/Error + 合理的返回值
 * 
 * 支持所有 UE 日志分类类型，包括 Shipping 构建中的 FNoLoggingCategory。
 */
struct XTOOLSCORE_API FXToolsErrorReporter
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

/**
 * 结合 ensureMsgf 与 FXToolsErrorReporter 的统一宏，用于函数返回值为 bool 的场景。
 *
 * 使用方式：
 *   XTOOLS_ENSURE_OR_ERROR_RETURN(Ptr != nullptr, LogX_AssetNaming, "Ptr is null in %s", TEXT("MyFunc"));
 *
 * 行为：
 *   - 在 Debug/Development 构建中触发 ensureMsgf（输出调用栈，便于调试）
 *   - 始终通过 FXToolsErrorReporter::Error 记录统一格式的错误日志
 *   - 返回 false 终止当前函数
 */
#define XTOOLS_ENSURE_OR_ERROR_RETURN(Condition, Category, Message, ...) \
    do \
    { \
        if (!ensureMsgf((Condition), TEXT(Message), ##__VA_ARGS__)) \
        { \
            FXToolsErrorReporter::Error((Category), FString::Printf(TEXT(Message), ##__VA_ARGS__)); \
            return false; \
        } \
    } while (0)

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
