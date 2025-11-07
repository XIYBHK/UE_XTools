#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputChord.h"
#include "Layout/Margin.h"
#include "UObject/Object.h"
#include "BlueprintAssistSettings_EditorFeatures.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class XTOOLS_BLUEPRINTASSIST_API UBASettings_EditorFeatures final : public UObject
{
	GENERATED_BODY()

public:
	UBASettings_EditorFeatures(const FObjectInitializer& ObjectInitializer);

	////////////////////////////////////////////////////////////
	/// CustomEventReplication
	////////////////////////////////////////////////////////////

	/* Set the according replication flags after renaming a custom event by matching the prefixes below */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "重命名后设置复制标志", Tooltip = "重命名自定义事件后通过匹配以下前缀设置相应的复制标志"))
	bool bSetReplicationFlagsAfterRenaming;

	/* When enabled, renaming a custom event with no matching prefix will apply "NotReplicated" */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta=(EditCondition="bSetReplicationFlagsAfterRenaming", DisplayName = "无前缀时清除复制标志", Tooltip = "启用时，重命名没有匹配前缀的自定义事件将应用不复制标志"))
	bool bClearReplicationFlagsWhenRenamingWithNoPrefix;

	/* Add the according prefix to the title after changing replication flags */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "添加前缀到标题", Tooltip = "更改复制标志后将相应前缀添加到标题"))
	bool bAddReplicationPrefixToCustomEventTitle;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "多播前缀", Tooltip = "多播事件的前缀"))
	FString MulticastPrefix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "服务器前缀", Tooltip = "服务器事件的前缀"))
	FString ServerPrefix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "客户端前缀", Tooltip = "客户端事件的前缀"))
	FString ClientPrefix;

	////////////////////////////////////////////////////////////
	/// Node group
	////////////////////////////////////////////////////////////

	/* Draw an outline to visualise each node group on the graph */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta = (DisplayName = "绘制节点组轮廓", Tooltip = "在图表上绘制轮廓以可视化每个节点组"))
	bool bDrawNodeGroupOutline;

	/* Only draw the group outline when selected */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "仅选中时绘制轮廓", Tooltip = "仅当选中时绘制节点组轮廓"))
	bool bOnlyDrawGroupOutlineWhenSelected;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓颜色", Tooltip = "节点组轮廓的颜色"))
	FLinearColor NodeGroupOutlineColor;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓宽度", Tooltip = "节点组轮廓的宽度"))
	float NodeGroupOutlineWidth;

	/* Outline margin around each node */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓边距", Tooltip = "每个节点周围的轮廓边距"))
	FMargin NodeGroupOutlineMargin;

	/* Draw a fill to show the node groups for selected nodes */
	UPROPERTY(EditAnywhere, Category = NodeGroup, meta = (DisplayName = "绘制节点组填充", Tooltip = "绘制填充以显示选中节点的节点组"))
	bool bDrawNodeGroupFill;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupFill", EditConditionHides, DisplayName = "节点组填充颜色", Tooltip = "节点组填充的颜色"))
	FLinearColor NodeGroupFillColor;

	////////////////////////////////////////////////////////////
	//// Mouse Features
	////////////////////////////////////////////////////////////

	/** Extra input chords to for dragging selected nodes with cursor (same as left-click-dragging) */
	UPROPERTY(EditAnywhere, config, Category = "Mouse Features", meta = (DisplayName = "额外拖动节点快捷键", Tooltip = "用光标拖动选中节点的额外输入组合键（与左键拖动相同）"))
	TArray<FInputChord> AdditionalDragNodesChords;

	/** Input chords for group dragging (move all linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Mouse Features", meta = (DisplayName = "组移动快捷键", Tooltip = "组拖动的输入组合键（移动所有链接的节点）"))
	TArray<FInputChord> GroupMovementChords;

	/** Input chords for group dragging (move left linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Mouse Features", meta = (DisplayName = "左子树移动快捷键", Tooltip = "拖动左侧链接节点的输入组合键"))
	TArray<FInputChord> LeftSubTreeMovementChords;

	/** Input chords for group dragging (move right linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Mouse Features", meta = (DisplayName = "右子树移动快捷键", Tooltip = "拖动右侧链接节点的输入组合键"))
	TArray<FInputChord> RightSubTreeMovementChords;

	////////////////////////////////////////////////////////////
	/// General
	////////////////////////////////////////////////////////////

	/* Try to insert the node between any current wires when holding down this key */
	UPROPERTY(EditAnywhere, config, Category = "General | New Node Behaviour", meta = (DisplayName = "插入节点快捷键", Tooltip = "按住此键时尝试将节点插入到现有连线之间"))
	FInputChord InsertNewNodeKeyChord;

	/* When creating a new node from a parameter pin, always try to connect the execution. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "General | New Node Behaviour", meta = (DisplayName = "从参数引脚始终连接执行", Tooltip = "从参数引脚创建新节点时始终尝试连接执行，按住插入节点键将禁用此功能"))
	bool bAlwaysConnectExecutionFromParameter;

	/* When creating a new node from a parameter pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "General | New Node Behaviour", meta = (DisplayName = "从参数引脚始终插入", Tooltip = "从参数引脚创建新节点时始终尝试插入到连线之间，按住插入节点键将禁用此功能"))
	bool bAlwaysInsertFromParameter;

	/* When creating a new node from an execution pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "General | New Node Behaviour", meta = (DisplayName = "从执行引脚始终插入", Tooltip = "从执行引脚创建新节点时始终尝试插入到连线之间，按住插入节点键将禁用此功能"))
	bool bAlwaysInsertFromExecution;

	/* Copy the pin value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "复制引脚值快捷键", Tooltip = "将引脚值复制到剪贴板"))
	FInputChord CopyPinValueChord;

	/* Paste the hovered value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "粘贴引脚值快捷键", Tooltip = "将剪贴板内容粘贴到悬停的引脚"))
	FInputChord PastePinValueChord;

	/* Select the first editable parameter pin when a node is created */
	UPROPERTY(EditAnywhere, config, Category = "Experimental", meta = (DisplayName = "创建节点时选中参数引脚", Tooltip = "创建节点时选中第一个可编辑的参数引脚"))
	bool bSelectValuePinWhenCreatingNewNodes;

	FORCEINLINE static const UBASettings_EditorFeatures& Get() { return *GetDefault<UBASettings_EditorFeatures>(); }
	FORCEINLINE static UBASettings_EditorFeatures& GetMutable() { return *GetMutableDefault<UBASettings_EditorFeatures>(); }
};
