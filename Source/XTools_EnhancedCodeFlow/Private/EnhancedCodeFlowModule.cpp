// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#include "Modules/ModuleManager.h"
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectGlobals.h"
#include "ECFSettings.h"
#include "ECFLogs.h"

DEFINE_LOG_CATEGORY(LogECF);

#if WITH_EDITOR

/**
 * 一次性迁移：旧 X_AssetEditorSettings（Editor ini）中的 bEnableEnhancedCodeFlowSubsystem
 * 显式配置值到本模块的 UECFSettings（Game ini）。
 *
 * 背景：旧开关存放在 Editor-only 设置类中，打包后读取不到，导致同一项目在编辑器与
 * 打包成品行为反转；迁移只发生在编辑器环境（Editor ini 天然不随打包分发）。
 *
 * 幂等保证（三条全满足才执行）：
 *  1. GIsEditor（打包成品无 Editor ini，守卫仅防异常场景）
 *  2. 新键尚未被用户在项目设置中显式管理（否则尊重现有配置）
 *  3. 旧键仍存在
 */
static void MigrateLegacyECFSubsystemSwitch()
{
	if (!GIsEditor || !GConfig)
	{
		return;
	}

	const TCHAR* SwitchKeyName = TEXT("bEnableEnhancedCodeFlowSubsystem");
	const TCHAR* NewSection = TEXT("/Script/XTools_EnhancedCodeFlow.ECFSettings");
	const TCHAR* LegacySection = TEXT("/Script/X_AssetEditor.X_AssetEditorSettings");

	// 已在新项目设置中显式管理过：不覆盖用户当前选择
	FString NewManagedValue;
	if (GConfig->GetString(NewSection, SwitchKeyName, NewManagedValue, GGameIni))
	{
		return;
	}

	FString LegacyValue;
	if (GConfig->GetString(LegacySection, SwitchKeyName, LegacyValue, GEditorIni))
	{
		UECFSettings* Settings = GetMutableDefault<UECFSettings>();
		const bool bLegacyEnabled = LegacyValue.ToBool();
		Settings->bEnableEnhancedCodeFlowSubsystem = bLegacyEnabled;
		if (Settings->TryUpdateDefaultConfigFile())
		{
			GConfig->RemoveKey(LegacySection, SwitchKeyName, GEditorIni);
			GConfig->Flush(false, GEditorIni);
			UE_LOG(LogECF, Log, TEXT("已完成子系统开关迁移: bEnableEnhancedCodeFlowSubsystem=%s -> DefaultGame.ini，旧 Editor.ini 配置键已清除"),
				bLegacyEnabled ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogECF, Warning, TEXT("无法将子系统开关迁移到 DefaultGame.ini，已保留旧 Editor.ini 配置键"));
		}
	}
}

#endif

/**
 * 增强代码流模块类
 * 提供异步操作、延迟执行、时间轴等功能的模块实现
 */
class XTOOLS_ENHANCEDCODEFLOW_API FEnhancedCodeFlowModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		MigrateLegacyECFSubsystemSwitch();
#endif
	}
};

IMPLEMENT_MODULE(FEnhancedCodeFlowModule, XTools_EnhancedCodeFlow)
