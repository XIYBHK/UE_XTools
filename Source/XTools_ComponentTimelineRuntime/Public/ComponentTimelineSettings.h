/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

/**
 * 组件时间轴设置
 * 提供组件时间轴功能的全局配置选项
 */

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ComponentTimelineSettings.generated.h"

/**
 * 组件时间轴设置类
 * 用于管理组件时间轴的全局配置参数
 * 可在项目设置中进行配置
 */
UCLASS(config = Editor, defaultconfig, 
	meta = (DisplayName = "Component Timeline", 
	ToolTip = "组件时间轴的全局配置选项\n用于控制时间轴功能的行为和特性"))
class XTOOLS_COMPONENTTIMELINERUNTIME_API UComponentTimelineSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("XTools"); }

	/** 
	 * 是否在所有蓝图中启用时间轴功能
	 * 启用后可以在任意蓝图中使用时间轴功能
	 * 注意：修改此选项需要重启引擎
	 */
	UPROPERTY(EditAnywhere, config, Category = "时间轴", 
		meta = (DisplayName = "在所有蓝图中启用", 
		ToolTip = "启用后可以在任意蓝图中使用时间轴功能\n需要重启引擎生效", 
		ConfigRestartRequired = true))
	bool bEnableObjectTimeline = false;
};
