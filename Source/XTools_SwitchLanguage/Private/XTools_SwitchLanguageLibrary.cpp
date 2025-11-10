/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XTools_SwitchLanguageLibrary.h"
#include "Internationalization/Culture.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Engine/Engine.h"
#include "XToolsErrorReporter.h"

// 静态成员定义
TArray<ESupportedLanguage> UXTools_SwitchLanguageLibrary::LanguageHistory;
const int32 UXTools_SwitchLanguageLibrary::MaxHistorySize = 10;

// 定义本地化文本命名空间
#define LOCTEXT_NAMESPACE "XTools_SwitchLanguage"

FLanguageSwitchResult UXTools_SwitchLanguageLibrary::SwitchLanguage(ESupportedLanguage Language)
{
	FLanguageSwitchResult Result;
	
	// 获取当前语言
	ESupportedLanguage PreviousLanguage = GetCurrentLanguage();
	Result.PreviousLanguage = PreviousLanguage;

	// 处理自动语言
	if (Language == ESupportedLanguage::Auto)
	{
		Language = GetSystemPreferredLanguage();
	}

	// 检查语言可用性
	if (!IsLanguageAvailable(Language))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = LOCTEXT("LanguageNotAvailable", "指定的语言不可用或未安装本地化资源");
		return Result;
	}

	// 获取文化代码
	FString CultureName = GetCultureNameForLanguage(Language);
	if (CultureName.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = LOCTEXT("InvalidCulture", "无效的文化代码");
		return Result;
	}

	// 执行语言切换
	FInternationalization& I18n = FInternationalization::Get();
	if (I18n.SetCurrentCulture(CultureName))
	{
		Result.bSuccess = true;
		Result.CurrentLanguage = Language;
		Result.CultureName = CultureName;

		// 添加到历史记录
		AddToHistory(Language);

		// 广播语言切换事件
		BroadcastLanguageChanged(Language, Result);

		// 显示成功消息
		if (GEngine)
		{
			FText SuccessMessage = FText::Format(
				LOCTEXT("LanguageSwitchSuccess", "语言已切换到: {LanguageName}"),
				FFormatNamedArguments{{TEXT("LanguageName"), FText::FromString(GetEnumValueAsString(Language))}}
			);
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, SuccessMessage.ToString());
		}
	}
	else
	{
		Result.bSuccess = false;
		Result.ErrorMessage = LOCTEXT("SwitchFailed", "语言切换失败，可能是本地化资源未加载");
	}

	return Result;
}

FLanguageSwitchResult UXTools_SwitchLanguageLibrary::SwitchLanguageByCulture(const FString& CultureName)
{
	FLanguageSwitchResult Result;
	
	// 获取当前语言
	ESupportedLanguage PreviousLanguage = GetCurrentLanguage();
	Result.PreviousLanguage = PreviousLanguage;

	// 验证文化代码
	if (CultureName.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = LOCTEXT("EmptyCultureName", "文化代码不能为空");
		return Result;
	}

	// 检查文化可用性
	if (!IsCultureAvailable(CultureName))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FText::Format(
			LOCTEXT("CultureNotAvailable", "文化代码 '{CultureName}' 不可用"),
			FFormatNamedArguments{{TEXT("CultureName"), FText::FromString(CultureName)}}
		);
		return Result;
	}

	// 执行语言切换
	FInternationalization& I18n = FInternationalization::Get();
	if (I18n.SetCurrentCulture(CultureName))
	{
		Result.bSuccess = true;
		Result.CultureName = CultureName;
		Result.CurrentLanguage = GetLanguageForCultureName(CultureName);

		// 添加到历史记录
		AddToHistory(Result.CurrentLanguage);

		// 广播语言切换事件
		BroadcastLanguageChanged(Result.CurrentLanguage, Result);

		// 显示成功消息
		if (GEngine)
		{
			FText SuccessMessage = FText::Format(
				LOCTEXT("CultureSwitchSuccess", "文化代码已切换到: {CultureName}"),
				FFormatNamedArguments{{TEXT("CultureName"), FText::FromString(CultureName)}}
			);
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, SuccessMessage.ToString());
		}
	}
	else
	{
		Result.bSuccess = false;
		Result.ErrorMessage = LOCTEXT("CultureSwitchFailed", "文化代码切换失败");
	}

	return Result;
}

ESupportedLanguage UXTools_SwitchLanguageLibrary::GetCurrentLanguage()
{
	FInternationalization& I18n = FInternationalization::Get();
	FString CurrentCulture = I18n.GetCurrentCulture()->GetName();
	return GetLanguageForCultureName(CurrentCulture);
}

FString UXTools_SwitchLanguageLibrary::GetCurrentCultureName()
{
	FInternationalization& I18n = FInternationalization::Get();
	return I18n.GetCurrentCulture()->GetName();
}

TArray<FLanguageInfo> UXTools_SwitchLanguageLibrary::GetSupportedLanguages()
{
	TArray<FLanguageInfo> SupportedLanguages;

	// 定义支持的语言列表
	TArray<TTuple<ESupportedLanguage, FString, FText>> LanguageDefinitions = {
		MakeTuple(ESupportedLanguage::English, TEXT("en"), LOCTEXT("English", "English")),
		MakeTuple(ESupportedLanguage::Chinese, TEXT("zh-Hans"), LOCTEXT("Chinese", "中文")),
		MakeTuple(ESupportedLanguage::Japanese, TEXT("ja"), LOCTEXT("Japanese", "日本語")),
		MakeTuple(ESupportedLanguage::Korean, TEXT("ko"), LOCTEXT("Korean", "한국어")),
		MakeTuple(ESupportedLanguage::French, TEXT("fr"), LOCTEXT("French", "Français")),
		MakeTuple(ESupportedLanguage::German, TEXT("de"), LOCTEXT("German", "Deutsch")),
		MakeTuple(ESupportedLanguage::Spanish, TEXT("es"), LOCTEXT("Spanish", "Español")),
		MakeTuple(ESupportedLanguage::Russian, TEXT("ru"), LOCTEXT("Russian", "Русский")),
		MakeTuple(ESupportedLanguage::Auto, TEXT(""), LOCTEXT("Auto", "Auto"))
	};

	for (const auto& LangDef : LanguageDefinitions)
	{
		ESupportedLanguage Language = LangDef.Get<0>();
		FString CultureCode = LangDef.Get<1>();
		FText DisplayName = LangDef.Get<2>();
		bool bIsAvailable = (Language == ESupportedLanguage::Auto) || IsCultureAvailable(CultureCode);

		SupportedLanguages.Add(FLanguageInfo(Language, DisplayName, CultureCode, bIsAvailable));
	}

	return SupportedLanguages;
}

bool UXTools_SwitchLanguageLibrary::IsLanguageAvailable(ESupportedLanguage Language)
{
	if (Language == ESupportedLanguage::Auto)
	{
		return true; // 自动语言总是可用的
	}

	FString CultureName = GetCultureNameForLanguage(Language);
	return IsCultureAvailable(CultureName);
}

FString UXTools_SwitchLanguageLibrary::LanguageToCultureName(ESupportedLanguage Language)
{
	return GetCultureNameForLanguage(Language);
}

ESupportedLanguage UXTools_SwitchLanguageLibrary::CultureNameToLanguage(const FString& CultureName)
{
	return GetLanguageForCultureName(CultureName);
}

ESupportedLanguage UXTools_SwitchLanguageLibrary::GetSystemPreferredLanguage()
{
	FInternationalization& I18n = FInternationalization::Get();
	FString SystemCulture = I18n.GetDefaultCulture()->GetName();
	return GetLanguageForCultureName(SystemCulture);
}

FString UXTools_SwitchLanguageLibrary::GetSystemPreferredCultureName()
{
	FInternationalization& I18n = FInternationalization::Get();
	return I18n.GetDefaultCulture()->GetName();
}

FLanguageSwitchResult UXTools_SwitchLanguageLibrary::ResetToSystemLanguage()
{
	ESupportedLanguage SystemLanguage = GetSystemPreferredLanguage();
	return SwitchLanguage(SystemLanguage);
}

TArray<ESupportedLanguage> UXTools_SwitchLanguageLibrary::GetLanguageSwitchHistory()
{
	return LanguageHistory;
}

void UXTools_SwitchLanguageLibrary::ClearLanguageSwitchHistory()
{
	LanguageHistory.Empty();
}

FString UXTools_SwitchLanguageLibrary::GetLocalizationDiagnostics()
{
	FInternationalization& I18n = FInternationalization::Get();
	FString Diagnostics;

	Diagnostics += TEXT("=== 本地化系统诊断信息 ===\n");
	Diagnostics += FString::Printf(TEXT("当前文化: %s\n"), *I18n.GetCurrentCulture()->GetName());
	Diagnostics += FString::Printf(TEXT("默认文化: %s\n"), *I18n.GetDefaultCulture()->GetName());
	
	TArray<FString> AllCultureNames;
	I18n.GetCultureNames(AllCultureNames);
	Diagnostics += FString::Printf(TEXT("可用文化数量: %d\n"), AllCultureNames.Num());

	// 列出一些可用文化
	Diagnostics += TEXT("\n=== 可用文化列表 ===\n");
	for (const FString& CultureName : AllCultureNames)
	{
		FCulturePtr Culture = I18n.GetCulture(CultureName);
		if (Culture.IsValid())
		{
			Diagnostics += FString::Printf(TEXT("- %s (%s)\n"), 
				*Culture->GetName(), *Culture->GetNativeName());
		}
	}

	return Diagnostics;
}

void UXTools_SwitchLanguageLibrary::RefreshLocalizationResources()
{
	FTextLocalizationManager::Get().RefreshResources();
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, 
			LOCTEXT("LocalizationRefreshed", "本地化资源已刷新").ToString());
	}
}

// 私有辅助函数实现

FString UXTools_SwitchLanguageLibrary::GetEnumValueAsString(ESupportedLanguage Language)
{
	switch (Language)
	{
	case ESupportedLanguage::English:
		return TEXT("English");
	case ESupportedLanguage::Chinese:
		return TEXT("中文");
	case ESupportedLanguage::Japanese:
		return TEXT("日本語");
	case ESupportedLanguage::Korean:
		return TEXT("한국어");
	case ESupportedLanguage::French:
		return TEXT("Français");
	case ESupportedLanguage::German:
		return TEXT("Deutsch");
	case ESupportedLanguage::Spanish:
		return TEXT("Español");
	case ESupportedLanguage::Russian:
		return TEXT("Русский");
	case ESupportedLanguage::Auto:
		return TEXT("Auto");
	default:
		return TEXT("Unknown");
	}
}

FString UXTools_SwitchLanguageLibrary::GetCultureNameForLanguage(ESupportedLanguage Language)
{
	switch (Language)
	{
	case ESupportedLanguage::English:
		return TEXT("en");
	case ESupportedLanguage::Chinese:
		return TEXT("zh-Hans");
	case ESupportedLanguage::Japanese:
		return TEXT("ja");
	case ESupportedLanguage::Korean:
		return TEXT("ko");
	case ESupportedLanguage::French:
		return TEXT("fr");
	case ESupportedLanguage::German:
		return TEXT("de");
	case ESupportedLanguage::Spanish:
		return TEXT("es");
	case ESupportedLanguage::Russian:
		return TEXT("ru");
	case ESupportedLanguage::Auto:
	default:
		return TEXT("");
	}
}

ESupportedLanguage UXTools_SwitchLanguageLibrary::GetLanguageForCultureName(const FString& CultureName)
{
	if (CultureName.StartsWith(TEXT("zh")) || CultureName.StartsWith(TEXT("ZH")))
	{
		return ESupportedLanguage::Chinese;
	}
	else if (CultureName.StartsWith(TEXT("ja")) || CultureName.StartsWith(TEXT("JA")))
	{
		return ESupportedLanguage::Japanese;
	}
	else if (CultureName.StartsWith(TEXT("ko")) || CultureName.StartsWith(TEXT("KO")))
	{
		return ESupportedLanguage::Korean;
	}
	else if (CultureName.StartsWith(TEXT("fr")) || CultureName.StartsWith(TEXT("FR")))
	{
		return ESupportedLanguage::French;
	}
	else if (CultureName.StartsWith(TEXT("de")) || CultureName.StartsWith(TEXT("DE")))
	{
		return ESupportedLanguage::German;
	}
	else if (CultureName.StartsWith(TEXT("es")) || CultureName.StartsWith(TEXT("ES")))
	{
		return ESupportedLanguage::Spanish;
	}
	else if (CultureName.StartsWith(TEXT("ru")) || CultureName.StartsWith(TEXT("RU")))
	{
		return ESupportedLanguage::Russian;
	}
	else if (CultureName.StartsWith(TEXT("en")) || CultureName.StartsWith(TEXT("EN")))
	{
		return ESupportedLanguage::English;
	}
	else
	{
		return ESupportedLanguage::Auto; // 未知文化代码返回自动
	}
}

bool UXTools_SwitchLanguageLibrary::IsCultureAvailable(const FString& CultureName)
{
	if (CultureName.IsEmpty())
	{
		return false;
	}

	FInternationalization& I18n = FInternationalization::Get();
	
	// 尝试获取文化，如果成功则说明可用
	FCulturePtr Culture = I18n.GetCulture(CultureName);
	return Culture.IsValid();
}

void UXTools_SwitchLanguageLibrary::AddToHistory(ESupportedLanguage Language)
{
	// 移除重复项
	LanguageHistory.Remove(Language);

	// 添加到开头
	LanguageHistory.Insert(Language, 0);

	// 限制历史记录大小
	while (LanguageHistory.Num() > MaxHistorySize)
	{
		LanguageHistory.RemoveAt(LanguageHistory.Num() - 1);
	}
}

void UXTools_SwitchLanguageLibrary::BroadcastLanguageChanged(ESupportedLanguage NewLanguage, const FLanguageSwitchResult& Result)
{
	// 这里可以添加全局事件广播逻辑
	// 例如通过游戏实例子系统或全局事件管理器
	// 目前作为占位符实现
}

// 取消本地化文本命名空间定义
#undef LOCTEXT_NAMESPACE
