/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// ObjectPoolEditor 模块日志分类
DECLARE_LOG_CATEGORY_EXTERN(LogObjectPoolEditor, Log, All);

/**
 * ObjectPoolEditor 模块
 * 
 * 提供对象池的编辑器工具和蓝图节点
 */
class FObjectPoolEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
