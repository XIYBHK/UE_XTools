// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 对象时间轴节点实现文件
 * 提供了对象专用的时间轴节点实现
 */

#include "K2Node_ObjectTimeline.h"
#include "ComponentTimelineSettings.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "K2Node_ObjectTimeline"

/////////////////////////////////////////////////////
// UK2Node_ObjectTimeline

/**
 * 构造函数
 */
UK2Node_ObjectTimeline::UK2Node_ObjectTimeline(const FObjectInitializer& ObjectInitializer)
	: UK2Node_BaseTimeline(ObjectInitializer)
{
}

/**
 * 获取节点标题的颜色
 * @return 节点标题的颜色（粉色）
 */
FLinearColor UK2Node_ObjectTimeline::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.1f, 1.0f);
}

/**
 * 获取节点标题文本
 * @param TitleType - 标题类型
 * @return 节点标题文本
 */
FText UK2Node_ObjectTimeline::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText Title = FText::FromName(TimelineName);

	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != nullptr);

	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineName);
	// 如果该节点还没有生成时间轴，则使用描述性标题
	if (Timeline == nullptr)
	{
		Title = LOCTEXT("NoTimelineTitle", "添加对象时间轴（实验性功能）...");
	}
	return Title;
}

/**
 * 获取工具提示文本
 * @return 工具提示文本，说明这是一个实验性功能
 */
FText UK2Node_ObjectTimeline::GetTooltipText() const
{
	return LOCTEXT("TimelineTooltip", "这是一个实验性功能！\n时间轴节点允许随时间设置关键帧值。\n双击打开时间轴编辑器。");
}

/**
 * 获取节点的菜单操作
 * 只有在启用对象时间轴功能时才显示菜单项
 */
void UK2Node_ObjectTimeline::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	if (GetDefault<UComponentTimelineSettings>()->bEnableObjectTimeline)
	{
		Super::GetMenuActions(ActionRegistrar);
	}
}

/**
 * 检查蓝图是否支持对象时间轴功能
 * @param Blueprint - 要检查的蓝图
 * @return 如果蓝图支持事件图表且不是基于组件或Actor的，则返回true
 */
bool UK2Node_ObjectTimeline::DoesSupportTimelines(const UBlueprint* Blueprint) const
{
	return Super::DoesSupportTimelines(Blueprint) && !FBlueprintEditorUtils::IsComponentBased(Blueprint) && !FBlueprintEditorUtils::IsActorBased(Blueprint);
}

/**
 * 获取对象时间轴所需的节点名称
 * @return 初始化时间轴的函数名称
 */
FName UK2Node_ObjectTimeline::GetRequiredNodeInBlueprint() const
{
	static FName NODE_NAME = "InitializeTimelines";
	return NODE_NAME;
}

#undef LOCTEXT_NAMESPACE
