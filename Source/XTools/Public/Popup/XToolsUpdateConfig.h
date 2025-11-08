/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "Engine/DeveloperSettings.h"
#include "XToolsUpdateConfig.generated.h"

/**
 * XTools 更新配置
 * 用于跟踪插件版本，控制首次打开/更新时的弹窗显示
 */
UCLASS(config = EditorPerProjectUserSettings)
class XTOOLS_API UXToolsUpdateConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UXToolsUpdateConfig()
	{
	}

	/** 已显示的插件版本号（用于检测首次打开或更新） */
	UPROPERTY(config)
	FString PluginVersionShown = "";
};



