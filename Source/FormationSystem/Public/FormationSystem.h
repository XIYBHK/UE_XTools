#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** 
 * FormationSystem模块API宏定义
 * 用于导出公共API符号
 */
#ifndef FORMATIONSYSTEM_API
	#ifdef FORMATIONSYSTEM_EXPORTS
		#define FORMATIONSYSTEM_API DLLEXPORT
	#else
		#define FORMATIONSYSTEM_API DLLIMPORT
	#endif
#endif

/**
 * FormationSystem模块接口
 * 阵型变换和群集移动系统的核心模块
 */
class FFormationSystemModule : public IModuleInterface
{
public:
	/** IModuleInterface 实现 */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * 检查模块是否可用
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("FormationSystem");
	}
}; 