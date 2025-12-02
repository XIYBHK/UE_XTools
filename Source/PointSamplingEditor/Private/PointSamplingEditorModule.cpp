/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * PointSamplingEditor模块类
 *
 * 为PointSampling提供编辑器专用功能，包括自定义K2Node节点
 */
class FPointSamplingEditorModule : public IModuleInterface
{
public:
	/** 模块启动 */
	virtual void StartupModule() override
	{
		// 编辑器模块启动时的初始化
	}

	/** 模块关闭 */
	virtual void ShutdownModule() override
	{
		// 编辑器模块关闭时的清理
	}
};

IMPLEMENT_MODULE(FPointSamplingEditorModule, PointSamplingEditor)
