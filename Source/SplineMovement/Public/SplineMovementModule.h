#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * SplineMovement 模块接口
 * 提供样条线路径移动异步蓝图节点
 */
class FSplineMovementModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("SplineMovement"));
	}
};
