/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Internationalization/Internationalization.h"
#include "XTools_SwitchLanguageTypes.h"
#include "XTools_SwitchLanguageLibrary.generated.h"

/**
 * XTools 语言切换蓝图函数库
 * 
 * 提供运行时动态切换本地化语言的功能，支持多种语言和自动检测
 */
UCLASS()
class XTOOLS_SWITCHLANGUAGE_API UXTools_SwitchLanguageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// 核心语言切换功能
	// ============================================================================

	/**
	 * 切换到指定语言
	 * @param Language 目标语言
	 * @return 切换结果，包含成功状态和详细信息
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|语言切换|核心",
		meta = (DisplayName = "切换语言"))
	static FLanguageSwitchResult SwitchLanguage(ESupportedLanguage Language);

	/**
	 * 切换到指定文化代码（如 "zh-Hans", "en", "ja"）
	 * @param CultureName 文化代码
	 * @return 切换结果，包含成功状态和详细信息
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|语言切换|核心",
		meta = (DisplayName = "切换语言（文化代码）"))
	static FLanguageSwitchResult SwitchLanguageByCulture(const FString& CultureName);

	/**
	 * 获取当前语言
	 * @return 当前使用的语言
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|查询",
		meta = (DisplayName = "获取当前语言"))
	static ESupportedLanguage GetCurrentLanguage();

	/**
	 * 获取当前文化代码
	 * @return 当前使用的文化代码（如 "zh-Hans", "en"）
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|查询",
		meta = (DisplayName = "获取当前文化代码"))
	static FString GetCurrentCultureName();

	// ============================================================================
	// 语言信息查询
	// ============================================================================

	/**
	 * 获取所有支持的语言列表
	 * @return 支持的语言信息数组
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|查询",
		meta = (DisplayName = "获取支持的语言列表"))
	static TArray<FLanguageInfo> GetSupportedLanguages();

	/**
	 * 检查指定语言是否可用
	 * @param Language 要检查的语言
	 * @return 该语言是否可用
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|查询",
		meta = (DisplayName = "检查语言是否可用"))
	static bool IsLanguageAvailable(ESupportedLanguage Language);

	/**
	 * 将枚举语言转换为文化代码
	 * @param Language 枚举语言
	 * @return 对应的文化代码
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|转换",
		meta = (DisplayName = "语言枚举转文化代码"))
	static FString LanguageToCultureName(ESupportedLanguage Language);

	/**
	 * 将文化代码转换为枚举语言
	 * @param CultureName 文化代码
	 * @return 对应的枚举语言
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|转换",
		meta = (DisplayName = "文化代码转语言枚举"))
	static ESupportedLanguage CultureNameToLanguage(const FString& CultureName);

	// ============================================================================
	// 系统语言检测
	// ============================================================================

	/**
	 * 获取系统首选语言
	 * @return 系统首选语言
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|系统",
		meta = (DisplayName = "获取系统首选语言"))
	static ESupportedLanguage GetSystemPreferredLanguage();

	/**
	 * 获取系统首选文化代码
	 * @return 系统首选文化代码
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|系统",
		meta = (DisplayName = "获取系统首选文化代码"))
	static FString GetSystemPreferredCultureName();

	// ============================================================================
	// 高级功能
	// ============================================================================

	/**
	 * 重置为系统语言
	 * @return 切换结果
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|语言切换|核心",
		meta = (DisplayName = "重置为系统语言"))
	static FLanguageSwitchResult ResetToSystemLanguage();

	/**
	 * 获取语言切换历史
	 * @return 最近切换的语言列表（最新的在前）
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|历史",
		meta = (DisplayName = "获取语言切换历史"))
	static TArray<ESupportedLanguage> GetLanguageSwitchHistory();

	/**
	 * 清空语言切换历史
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|语言切换|历史",
		meta = (DisplayName = "清空语言切换历史"))
	static void ClearLanguageSwitchHistory();

	// ============================================================================
	// 调试和诊断
	// ============================================================================

	/**
	 * 获取本地化系统诊断信息
	 * @return 诊断信息字符串
	 */
	UFUNCTION(BlueprintPure, Category = "XTools|语言切换|诊断",
		meta = (DisplayName = "获取本地化诊断信息"))
	static FString GetLocalizationDiagnostics();

	/**
	 * 强制刷新本地化资源
	 * 通常在切换语言后调用以确保所有UI更新
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|语言切换|诊断",
		meta = (DisplayName = "刷新本地化资源"))
	static void RefreshLocalizationResources();

private:
	// 内部辅助函数
	static FString GetCultureNameForLanguage(ESupportedLanguage Language);
	static ESupportedLanguage GetLanguageForCultureName(const FString& CultureName);
	static bool IsCultureAvailable(const FString& CultureName);
	static void AddToHistory(ESupportedLanguage Language);
	static void BroadcastLanguageChanged(ESupportedLanguage NewLanguage, const FLanguageSwitchResult& Result);
	static FString GetEnumValueAsString(ESupportedLanguage Language);

	// 静态历史记录
	static TArray<ESupportedLanguage> LanguageHistory;
	static const int32 MaxHistorySize;
};
