// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

/**
 * 基础时间轴节点头文件
 * 提供了时间轴节点的基础功能实现
 */

#include "K2Node_HackTimeline.h"

#include "K2Node_BaseTimeline.generated.h"

/**
 * 基础时间轴节点类
 * 作为所有时间轴节点类型的基类，提供共同的基础功能
 */
UCLASS(abstract, meta=(ToolTip="时间轴节点基类"))
class XTOOLS_COMPONENTTIMELINEUNCOOKED_API UK2Node_BaseTimeline : public UK2Node_HackTimeline
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraphNode Interface.
	/** 节点粘贴后的处理 */
	virtual void PostPasteNode() override;
	/** 检查节点是否与目标图表兼容 */
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	/** 在编译期间验证节点 */
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface.
	/** 获取节点的菜单操作 */
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	/** 获取节点分类 */
	virtual FText GetMenuCategory() const override { return NSLOCTEXT("K2Node", "XToolsCategory", "XTools"); }
	//~ End UK2Node Interface.

protected:
	/** 
	 * 在蓝图中添加新的时间轴
	 * @param Blueprint - 目标蓝图
	 * @param TimelineVarName - 时间轴变量名称
	 * @return 新创建的时间轴模板
	 */
	static UTimelineTemplate* AddNewTimeline(UBlueprint* Blueprint, const FName& TimelineVarName);

	/** 
	 * 检查蓝图是否支持时间轴功能
	 * @param Blueprint - 要检查的蓝图
	 * @return 是否支持时间轴
	 */
	virtual bool DoesSupportTimelines(const UBlueprint* Blueprint) const;

	/** 
	 * 获取蓝图中所需的节点名称
	 * @return 节点名称
	 */
	virtual FName GetRequiredNodeInBlueprint() const;

protected:
	/** 节点工具提示文本 */
	FText NodeTooltip;
};
