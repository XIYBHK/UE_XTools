#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class XTOOLS_API IXToolsModule : public IModuleInterface
{
public:
	/**
	 * 单例访问器
	 */
	static inline IXToolsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IXToolsModule>("XTools");
	}

	/**
	 * 检查模块是否已加载
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("XTools");
	}
};