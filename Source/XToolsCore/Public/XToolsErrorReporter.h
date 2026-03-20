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
    // File/Line 参数由 XTOOLS_LOG_* 宏自动传入调用者的位置信息
    template<typename LogCategoryType>
    static void Report(const LogCategoryType& Category,
                       ELogVerbosity::Type Verbosity,
                       const FString& Message,
                       const FName Context = NAME_None,
                       bool bNotifyOnScreen = false,
                       float DisplayTime = 5.0f,
                       const ANSICHAR* File = "",
                       int32 Line = 0);

    template<typename LogCategoryType>
    static void Error(const LogCategoryType& Category,
                      const FString& Message,
                      const FName Context = NAME_None,
                      bool bNotifyOnScreen = false,
                      float DisplayTime = 5.0f,
                      const ANSICHAR* File = "",
                      int32 Line = 0);

    template<typename LogCategoryType>
    static void Warning(const LogCategoryType& Category,
                        const FString& Message,
                        const FName Context = NAME_None,
                        bool bNotifyOnScreen = false,
                        float DisplayTime = 5.0f,
                        const ANSICHAR* File = "",
                        int32 Line = 0);

    template<typename LogCategoryType>
    static void Info(const LogCategoryType& Category,
                     const FString& Message,
                     const FName Context = NAME_None,
                     bool bNotifyOnScreen = false,
                     float DisplayTime = 5.0f,
                     const ANSICHAR* File = "",
                     int32 Line = 0);

private:
    // 内部实现方法（File/Line 参数用于输出调用者的实际位置）
    static void ReportInternal(FLogCategoryBase* Category,
                              ELogVerbosity::Type Verbosity,
                              const FString& Message,
                              const FName Context,
                              bool bNotifyOnScreen,
                              float DisplayTime,
                              const ANSICHAR* File = "",
                              int32 Line = 0);
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

/**
 * 便捷日志宏 - 自动传入调用点的文件名和行号。
 * 解决直接调用静态方法时 __FILE__/__LINE__ 始终指向 ErrorReporter 本身的问题。
 *
 * 用法：
 *   XTOOLS_LOG_ERROR(LogMyCategory, TEXT("发生错误"));
 *   XTOOLS_LOG_WARNING(LogMyCategory, TEXT("警告信息"));
 *   XTOOLS_LOG_INFO(LogMyCategory, TEXT("普通日志"));
 *
 * 需要屏幕提示时使用带参数版本：
 *   XTOOLS_LOG_ERROR_EX(LogMyCategory, TEXT("错误"), NAME_None, true, 3.0f);
 */
#define XTOOLS_LOG_ERROR(Category, Message) \
    FXToolsErrorReporter::Error(Category, Message, NAME_None, false, 5.0f, __FILE__, __LINE__)

#define XTOOLS_LOG_WARNING(Category, Message) \
    FXToolsErrorReporter::Warning(Category, Message, NAME_None, false, 5.0f, __FILE__, __LINE__)

#define XTOOLS_LOG_INFO(Category, Message) \
    FXToolsErrorReporter::Info(Category, Message, NAME_None, false, 5.0f, __FILE__, __LINE__)

#define XTOOLS_LOG_ERROR_EX(Category, Message, Context, bNotifyOnScreen, DisplayTime) \
    FXToolsErrorReporter::Error(Category, Message, Context, bNotifyOnScreen, DisplayTime, __FILE__, __LINE__)

#define XTOOLS_LOG_WARNING_EX(Category, Message, Context, bNotifyOnScreen, DisplayTime) \
    FXToolsErrorReporter::Warning(Category, Message, Context, bNotifyOnScreen, DisplayTime, __FILE__, __LINE__)

#define XTOOLS_LOG_INFO_EX(Category, Message, Context, bNotifyOnScreen, DisplayTime) \
    FXToolsErrorReporter::Info(Category, Message, Context, bNotifyOnScreen, DisplayTime, __FILE__, __LINE__)

// 模板方法实现 - 必须在头文件中
template<typename LogCategoryType>
void FXToolsErrorReporter::Report(const LogCategoryType& Category,
                                  ELogVerbosity::Type Verbosity,
                                  const FString& Message,
                                  const FName Context,
                                  bool bNotifyOnScreen,
                                  float DisplayTime,
                                  const ANSICHAR* File,
                                  int32 Line)
{
    // 获取日志分类指针
    // 对于 FLogCategoryBase 派生类，传递指针；对于 FNoLoggingCategory，传递 nullptr
    FLogCategoryBase* CategoryPtr = nullptr;

    // 使用 UE 的 TIsDerivedFrom 检查类型，兼容所有构建配置
    if constexpr (TIsDerivedFrom<LogCategoryType, FLogCategoryBase>::Value)
    {
        CategoryPtr = const_cast<FLogCategoryBase*>(static_cast<const FLogCategoryBase*>(&Category));
    }

    ReportInternal(CategoryPtr, Verbosity, Message, Context, bNotifyOnScreen, DisplayTime, File, Line);
}

template<typename LogCategoryType>
void FXToolsErrorReporter::Error(const LogCategoryType& Category,
                                 const FString& Message,
                                 const FName Context,
                                 bool bNotifyOnScreen,
                                 float DisplayTime,
                                 const ANSICHAR* File,
                                 int32 Line)
{
    Report(Category, ELogVerbosity::Error, Message, Context, bNotifyOnScreen, DisplayTime, File, Line);
}

template<typename LogCategoryType>
void FXToolsErrorReporter::Warning(const LogCategoryType& Category,
                                   const FString& Message,
                                   const FName Context,
                                   bool bNotifyOnScreen,
                                   float DisplayTime,
                                   const ANSICHAR* File,
                                   int32 Line)
{
    Report(Category, ELogVerbosity::Warning, Message, Context, bNotifyOnScreen, DisplayTime, File, Line);
}

template<typename LogCategoryType>
void FXToolsErrorReporter::Info(const LogCategoryType& Category,
                                const FString& Message,
                                const FName Context,
                                bool bNotifyOnScreen,
                                float DisplayTime,
                                const ANSICHAR* File,
                                int32 Line)
{
    Report(Category, ELogVerbosity::Log, Message, Context, bNotifyOnScreen, DisplayTime, File, Line);
}
