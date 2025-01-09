// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

/**
 * 对象时间轴节点头文件
 * 提供了对象专用的时间轴节点实现
 */

#include "K2Node_BaseTimeline.h"

#include "K2Node_ObjectTimeline.generated.h"

/**
 * 对象时间轴节点类
 * 专门用于处理普通对象（非组件）中的时间轴功能
 */
UCLASS()
class COMPONENTTIMELINEUNCOOKED_API UK2Node_ObjectTimeline : public UK2Node_BaseTimeline
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

	/** 获取节点的菜单操作 */
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

protected:
	/** 检查蓝图是否支持对象时间轴功能 */
	virtual bool DoesSupportTimelines(const UBlueprint* Blueprint) const override;
	/** 获取对象时间轴所需的节点名称 */
	virtual FName GetRequiredNodeInBlueprint() const override;
};

