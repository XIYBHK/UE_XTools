/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "XTools_SwitchLanguageTypes.generated.h"

/**
 * 支持的语言枚举
 */
UENUM(BlueprintType)
enum class ESupportedLanguage : uint8
{
	English		UMETA(DisplayName = "English"),
	Chinese		UMETA(DisplayName = "中文"),
	Japanese	UMETA(DisplayName = "日本語"),
	Korean		UMETA(DisplayName = "한국어"),
	French		UMETA(DisplayName = "Français"),
	German		UMETA(DisplayName = "Deutsch"),
	Spanish		UMETA(DisplayName = "Español"),
	Russian		UMETA(DisplayName = "Русский"),
	Auto		UMETA(DisplayName = "Auto", ToolTip = "自动检测系统语言")
};

/**
 * 语言切换结果
 */
USTRUCT(BlueprintType)
struct XTOOLS_SWITCHLANGUAGE_API FLanguageSwitchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	FText ErrorMessage = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	ESupportedLanguage PreviousLanguage = ESupportedLanguage::Auto;

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	ESupportedLanguage CurrentLanguage = ESupportedLanguage::Auto;

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	FString CultureName = TEXT("");

	FLanguageSwitchResult()
	{
	}

	FLanguageSwitchResult(bool InSuccess, const FText& InErrorMessage = FText::GetEmpty())
		: bSuccess(InSuccess), ErrorMessage(InErrorMessage)
	{
	}
};

/**
 * 语言信息结构体
 */
USTRUCT(BlueprintType)
struct XTOOLS_SWITCHLANGUAGE_API FLanguageInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	ESupportedLanguage Language = ESupportedLanguage::English;

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	FText DisplayName = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	FString CultureCode = TEXT("");

	UPROPERTY(BlueprintReadOnly, Category="SwitchLanguage")
	bool bIsAvailable = false;

	FLanguageInfo()
	{
	}

	FLanguageInfo(ESupportedLanguage InLanguage, const FText& InDisplayName, const FString& InCultureCode, bool InIsAvailable)
		: Language(InLanguage), DisplayName(InDisplayName), CultureCode(InCultureCode), bIsAvailable(InIsAvailable)
	{
	}
};

/**
 * 语言切换事件委托
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLanguageChanged, ESupportedLanguage, NewLanguage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLanguageChangedWithInfo, ESupportedLanguage, NewLanguage, const FLanguageSwitchResult&, SwitchResult);
