// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// FieldSystemExtensions 模块日志分类
DECLARE_LOG_CATEGORY_EXTERN(LogFieldSystemExtensions, Log, All);

/**
 * FieldSystemExtensions 模块
 * 提供增强的Field System功能
 */
class FFieldSystemExtensionsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

