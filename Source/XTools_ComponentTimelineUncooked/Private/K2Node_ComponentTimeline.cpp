// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 组件时间轴节点实现文件
 * 提供了组件专用的时间轴节点实现
 */

#include "K2Node_ComponentTimeline.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "K2Node_ComponentTimeline"

/////////////////////////////////////////////////////
// UK2Node_ComponentTimeline

/**
 * 构造函数
 */
UK2Node_ComponentTimeline::UK2Node_ComponentTimeline(const FObjectInitializer& ObjectInitializer)
	: UK2Node_BaseTimeline(ObjectInitializer)
{
	NodeTooltip = LOCTEXT("NodeTooltip", "组件时间轴节点\n用于在组件中创建和管理时间轴功能\n可以通过时间轴编辑器设置关键帧动画");
}

/**
 * 获取节点标题的颜色
 * @return 节点标题的颜色（橙色）
 */
FLinearColor UK2Node_ComponentTimeline::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.51f, 0.0f);
}

/**
 * 获取节点标题文本
 * @param TitleType - 标题类型
 * @return 节点标题文本
 */
FText UK2Node_ComponentTimeline::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText Title = FText::FromName(TimelineName);

	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != nullptr);

	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineName);
	// 如果该节点还没有生成时间轴，则使用描述性标题
	if (Timeline == nullptr)
	{
		Title = LOCTEXT("NoTimelineTitle", "添加组件时间轴...");
	}

	return Title;
}

/**
 * 获取工具提示文本
 * @return 工具提示文本
 */
FText UK2Node_ComponentTimeline::GetTooltipText() const
{
	return NodeTooltip;
}

/**
 * 检查蓝图是否支持组件时间轴功能
 * @param Blueprint - 要检查的蓝图
 * @return 如果蓝图支持事件图表且基于组件，则返回true
 */
bool UK2Node_ComponentTimeline::DoesSupportTimelines(const UBlueprint* Blueprint) const
{
	return Super::DoesSupportTimelines(Blueprint) && FBlueprintEditorUtils::IsComponentBased(Blueprint);
}

/**
 * 获取组件时间轴所需的节点名称
 * @return 初始化组件时间轴的函数名称
 */
FName UK2Node_ComponentTimeline::GetRequiredNodeInBlueprint() const
{
	static FName NODE_NAME = "InitializeComponentTimelines";
	return NODE_NAME;
}

#undef LOCTEXT_NAMESPACE
