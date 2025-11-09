// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

/**
 * 组件时间轴节点头文件
 * 提供了组件专用的时间轴节点实现
 */

#include "K2Node_BaseTimeline.h"

#include "K2Node_ComponentTimeline.generated.h"

/**
 * 组件时间轴节点类
 * 专门用于处理组件中的时间轴功能
 */
UCLASS(meta=(DisplayName = "组件时间轴 (Component Timeline)", 
	ToolTip="组件时间轴节点\n用于在组件中创建和管理时间轴功能\n可以通过时间轴编辑器设置关键帧动画",
	Keywords="Component Timeline"))
class XTOOLS_COMPONENTTIMELINEUNCOOKED_API UK2Node_ComponentTimeline : public UK2Node_BaseTimeline
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraphNode Interface.
	/** 获取节点标题的颜色 */
	virtual FLinearColor GetNodeTitleColor() const override;
	/** 获取节点标题文本 */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	/** 获取工具提示文本 */
	virtual FText GetTooltipText() const override;
	//~ End UEdGraphNode Interface.

protected:
	/** 检查蓝图是否支持组件时间轴功能 */
	virtual bool DoesSupportTimelines(const UBlueprint* Blueprint) const override;
	/** 获取组件时间轴所需的节点名称 */
	virtual FName GetRequiredNodeInBlueprint() const override;
};


