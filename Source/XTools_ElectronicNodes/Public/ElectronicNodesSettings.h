/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the Unreal Engine Marketplace
*/

#pragma once

#include "Engine/DeveloperSettings.h"
#include "ElectronicNodesSettings.generated.h"

UENUM(BlueprintType)
enum class EWireStyle : uint8
{
	Default UMETA(DisplayName = "默认"),
	Manhattan UMETA(DisplayName = "曼哈顿（直角）"),
	Subway UMETA(DisplayName = "地铁（45度角）")
};

UENUM(BlueprintType)
enum class EWireAlignment : uint8
{
	Right UMETA(DisplayName = "右对齐"),
	Left UMETA(DisplayName = "左对齐")
};

UENUM(BlueprintType)
enum class EWirePriority : uint8
{
	None UMETA(DisplayName = "无"),
	Node UMETA(DisplayName = "节点优先"),
	Pin UMETA(DisplayName = "引脚优先")
};

UENUM(BlueprintType)
enum class EMinDistanceStyle : uint8
{
	Line UMETA(DisplayName = "直线"),
	Spline UMETA(DisplayName = "曲线")
};

UENUM(BlueprintType)
enum class EBubbleDisplayRule : uint8
{
	Always UMETA(DisplayName = "始终显示"),
	DisplayOnSelection UMETA(DisplayName = "选中时显示"),
	MoveOnSelection UMETA(DisplayName = "选中时移动")
};

UENUM(BlueprintType)
enum class ESelectionRule : uint8
{
	Near UMETA(DisplayName = "仅相邻节点"),
	Far UMETA(DisplayName = "所有关联节点")
};

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Electronic Nodes Plugin"))
class XTOOLS_ELECTRONICNODES_API UElectronicNodesSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UElectronicNodesSettings()
	{
		CategoryName = TEXT("Plugins");
		SectionName = TEXT("Electronic Nodes Plugin");
	}

	/* -----[ Activation ] ----- */

	/* Activate or deactivate the whole plugin. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation", meta = (DisplayName = "启用插件", Tooltip = "启用或禁用整个插件功能。默认: true"))
	bool MasterActivate = true;

	/* Use global settings across all your projects. When activated, it will load the global settings (overwriting this one).
	If no global settings exists, it will create it based on this one. Future updates will then be saved to global settings. */
	UPROPERTY(config, EditAnywhere, Category = "Activation", meta = (DisplayName = "使用全局设置", Tooltip = "跨所有项目使用全局设置。启用后将加载全局设置（覆盖当前设置）。如果全局设置不存在，将基于当前设置创建。未来的更新将保存到全局设置"))
	bool UseGlobalSettings = false;

	/* Force reload the global settings (if it was modified outside this instance for example). */
	UPROPERTY(config, EditAnywhere, Category = "Activation", meta = (EditCondition = "UseGlobalSettings", DisplayName = "重新加载全局设置", Tooltip = "强制重新加载全局设置（例如在外部修改后）"))
	bool LoadGlobalSettings = false;

	/* Display a popup with changelog on update. Default: false */
	UPROPERTY(config, EditAnywhere, Category = "Activation", meta = (DisplayName = "更新时显示弹窗", Tooltip = "插件更新时显示更新日志弹窗。默认: false"))
	bool ActivatePopupOnUpdate = false;

	/* Activate Electronic Nodes on Blueprint graphs. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate", DisplayName = "蓝图图表", Tooltip = "在蓝图图表上启用 Electronic Nodes。默认: true"))
	bool ActivateOnBlueprint = true;

	/* Activate Electronic Nodes on Material graphs. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate", DisplayName = "材质图表", Tooltip = "在材质图表上启用 Electronic Nodes。默认: true"))
	bool ActivateOnMaterial = true;

	/* Activate Electronic Nodes on Animation graphs. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate", DisplayName = "动画图表", Tooltip = "在动画图表上启用 Electronic Nodes。默认: true"))
	bool ActivateOnAnimation = true;

	/* Activate Electronic Nodes on VoxelPlugin (available on the marketplace). Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate", DisplayName = "VoxelPlugin", Tooltip = "在 VoxelPlugin 上启用 Electronic Nodes（可在市场获取）。默认: true"))
	bool ActivateOnVoxelPlugin = true;

	/* Hot patch hardcoded Unreal functions (only available on Windows) to make some more features available. NEED A RESTART OF THE ENGINE! Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate", DisplayName = "使用热补丁", Tooltip = "热补丁 Unreal 硬编码函数（仅Windows）以启用更多功能。需要重启引擎！默认: true"))
	bool UseHotPatch = true;

	/* Activate Electronic Nodes on Niagara. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate && UseHotPatch", DisplayName = "Niagara", Tooltip = "在 Niagara 上启用 Electronic Nodes。默认: true"))
	bool ActivateOnNiagara = true;

	/* Activate Electronic Nodes on Behavior Tree. Default: false */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate && UseHotPatch", DisplayName = "行为树", Tooltip = "在行为树上启用 Electronic Nodes。默认: true"))
	bool ActivateOnBehaviorTree = true;

	/* Activate Electronic Nodes on Control Rig. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate && UseHotPatch", DisplayName = "Control Rig", Tooltip = "在 Control Rig 上启用 Electronic Nodes。默认: true"))
	bool ActivateOnControlRig = true;

	/* Activate Electronic Nodes on Metasound. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate && UseHotPatch", DisplayName = "Metasound", Tooltip = "在 Metasound 上启用 Electronic Nodes。默认: true"))
	bool ActivateOnMetasound = true;

	/* Activate Electronic Nodes on Reference Viewer. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate && UseHotPatch", DisplayName = "引用查看器", Tooltip = "在引用查看器上启用 Electronic Nodes。默认: true"))
	bool ActivateOnReferenceViewer = true;

	/* Activate Electronic Nodes on custom graphs. WARNING: some graphs might need Hot Patch, and some graphs might not work at all */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Schema", meta = (EditCondition = "MasterActivate", DisplayName = "自定义图表架构", Tooltip = "在自定义图表上启用 Electronic Nodes。警告: 某些图表可能需要热补丁，某些图表可能完全不工作"))
	TArray<TSubclassOf<UEdGraphSchema>> CustomGraphSchemas;

	/* Activate Electronic Nodes everywhere, for debugging purpose only. Default: false */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Debug", meta = (EditCondition = "MasterActivate", DisplayName = "全局启用（调试）", Tooltip = "在所有地方启用 Electronic Nodes，仅用于调试。默认: false"))
	bool ActivateFallback = false;

	/* Display schema name in log. Default: false */
	UPROPERTY(config, EditAnywhere, Category = "Activation|Debug", meta = (EditCondition = "MasterActivate", DisplayName = "日志显示架构名", Tooltip = "在日志中显示图表架构名称。默认: false"))
	bool DisplaySchemaName = false;

	/* -----[ Wire Style ] ----- */

	/* Wire style of graph. "Manhattan" is for 90deg angles, "Subway" is for 45deg angles. */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "连线样式", Tooltip = "图表连线样式。曼哈顿为90度直角，地铁为45度角"))
	EWireStyle WireStyle = EWireStyle::Subway;

	/* Specify wire alignment. Default: right. */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "连线对齐", Tooltip = "指定连线对齐方式。默认: 右对齐"))
	EWireAlignment WireAlignment = EWireAlignment::Right;

	/* Specify wire alignment priority (when a Node is connected to a Pin). Default: none. */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "连线优先级", Tooltip = "指定连线对齐优先级（当节点连接到引脚时）。默认: 无"))
	EWirePriority WirePriority = EWirePriority::None;

	/* Round radius of the wires. Default: 10 */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "转角圆角半径", Tooltip = "连线转角的圆角半径。默认: 10"))
	uint32 RoundRadius = 10;

	/* Thickness of the wire (multiplier). Default: 1.2 */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (ClampMin = "0.0", DisplayName = "连线粗细", Tooltip = "连线粗细（倍数）。默认: 1.2，在高分辨率显示器上更清晰"))
	float WireThickness = 1.2f;

	/* Bellow this distance, wires will be drawn as straight. Default: 24 */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (ClampMin = "0.0", DisplayName = "直线最小距离", Tooltip = "低于此距离时，连线将绘制为直线。默认: 24"))
	float MinDistanceToStyle = 24.0f;

	/* Style for wires bellow MinDistanceToStyle. Default: Line */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "短距离连线样式", Tooltip = "低于最小距离时的连线样式。默认: 直线"))
	EMinDistanceStyle MinDistanceStyle = EMinDistanceStyle::Line;

	/* Horizontal offset of wires from nodes. Default: 16 */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "水平偏移", Tooltip = "连线与节点的水平偏移距离。默认: 16"))
	uint32 HorizontalOffset = 16;

	/* Disable the offset for pins. Default: false */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "禁用引脚偏移", Tooltip = "禁用引脚的偏移。默认: false"))
	bool DisablePinOffset = false;

	/* Fix default zoomed-out wire displacement. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Wire Style", meta = (DisplayName = "修复缩放位移", Tooltip = "修复默认缩小时的连线位移。默认: true"))
	bool FixZoomDisplacement = true;

	/* -----[ Exec Wire Style ] ----- */

	/* Use a specific draw style for exec wires. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Exec Wire Style", meta = (DisplayName = "单独设置执行线样式", Tooltip = "为执行线使用单独的绘制样式。默认: true，执行流用90°直角更醒目"))
	bool OverwriteExecWireStyle = true;

	/* Specific wire style for exec wires. Default: Manhattan */
	UPROPERTY(config, EditAnywhere, Category = "Exec Wire Style", meta = (EditCondition = "OverwriteExecWireStyle", DisplayName = "执行线样式", Tooltip = "执行线的专用连线样式。默认: 曼哈顿"))
	EWireStyle WireStyleForExec = EWireStyle::Manhattan;

	/* Specify wire alignment for exe wires. Default: right. */
	UPROPERTY(config, EditAnywhere, Category = "Exec Wire Style", meta = (EditCondition = "OverwriteExecWireStyle", DisplayName = "执行线对齐", Tooltip = "指定执行线的对齐方式。默认: 右对齐"))
	EWireAlignment WireAlignmentForExec = EWireAlignment::Right;

	/* Specify wire alignment priority (when a Node is connected to a Pin) for exe wires. Default: node. */
	UPROPERTY(config, EditAnywhere, Category = "Exec Wire Style", meta = (EditCondition = "OverwriteExecWireStyle", DisplayName = "执行线优先级", Tooltip = "指定执行线的对齐优先级（当节点连接到引脚时）。默认: 节点优先"))
	EWirePriority WirePriorityForExec = EWirePriority::Node;

	/* -----[ Ribbon Style ] ----- */

	/* Activate ribbon cables for overlapping wires. */
	UPROPERTY(config, EditAnywhere, Category = "Ribbon Style (experimental)", meta = (DisplayName = "启用带状线缆", Tooltip = "为重叠连线启用带状线缆效果（实验性功能）"))
	bool ActivateRibbon = true;

	/* Offset between ribbon wires. Default: 2 */
	UPROPERTY(config, EditAnywhere, Category = "Ribbon Style (experimental)", meta = (EditCondition = "ActivateRibbon", DisplayName = "带状线间距", Tooltip = "带状连线之间的偏移距离。默认: 2"))
	uint32 RibbonOffset = 2;

	/* Offset of wires when merge into ribbon. Default: 20 */
	UPROPERTY(config, EditAnywhere, Category = "Ribbon Style (experimental)", meta = (EditCondition = "ActivateRibbon", DisplayName = "合并偏移", Tooltip = "连线合并为带状时的偏移距离。默认: 20"))
	uint32 RibbonMergeOffset = 20;

	/* Push the offset outside the node (instead of going for the middle). Default: false */
	UPROPERTY(config, EditAnywhere, Category = "Ribbon Style (experimental)", meta = (EditCondition = "ActivateRibbon", DisplayName = "向外推", Tooltip = "将偏移推到节点外部（而不是向中间）。默认: false"))
	bool RibbonPushOutside = false;

	/* -----[ Bubble Style ] ----- */

	/* Show moving bubbles on the wires. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (DisplayName = "显示移动气泡", Tooltip = "在连线上显示移动的气泡动画。默认: true"))
	bool ForceDrawBubbles = true;

	/* Draw bubbles only on exec wires. Default: true */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (EditCondition = "ForceDrawBubbles", DisplayName = "仅执行线显示", Tooltip = "仅在执行连线上绘制气泡。默认: true"))
	bool DrawBubblesOnlyOnExec = true;

	/* Display rules to show/move bubbles only near selected nodes. Default: DisplayOnSelection */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (EditCondition = "ForceDrawBubbles", DisplayName = "气泡显示规则", Tooltip = "控制仅在选中节点附近显示/移动气泡的规则。默认: 选中时显示"))
	EBubbleDisplayRule BubbleDisplayRule = EBubbleDisplayRule::DisplayOnSelection;

	/* If selection only consider close nodes (near) or every related nodes (far). Default: Near */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (EditCondition = "ForceDrawBubbles", DisplayName = "选择范围", Tooltip = "选择时仅考虑相邻节点还是所有相关节点。默认: 仅相邻节点"))
	ESelectionRule SelectionRule = ESelectionRule::Near;

	/* Disable bubbles above a certain zoom level. Default: -2 */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (EditCondition = "ForceDrawBubbles", ClampMin = "-12", ClampMax = "7", DisplayName = "缩放阈值", Tooltip = "在特定缩放级别以上禁用气泡。默认: -2"))
	int32 BubbleZoomThreshold = -2;

	/* Size of bubbles on the wires. Default: 1.5 */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (EditCondition = "ForceDrawBubbles", ClampMin = "1.0", DisplayName = "气泡大小", Tooltip = "连线上气泡的大小。默认: 1.5"))
	float BubbleSize = 1.5f;

	/* Speed of bubbles on the wires. Default: 1.0 */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (EditCondition = "ForceDrawBubbles", ClampMin = "0.0", DisplayName = "气泡速度", Tooltip = "连线上气泡的移动速度。默认: 1.0"))
	float BubbleSpeed = 1.0f;

	/* Space between bubbles on the wires. Default: 50.0 */
	UPROPERTY(config, EditAnywhere, Category = "Bubbles Style", meta = (EditCondition = "ForceDrawBubbles", ClampMin = "10.0", DisplayName = "气泡间距", Tooltip = "连线上气泡之间的间距。默认: 50.0"))
	float BubbleSpace = 50.0f;

	bool Debug = false;

	/* Internal value to fix elements on plugin update. */
	UPROPERTY(config)
	FString PluginVersionUpdate = "";

	virtual FName GetContainerName() const override
	{
		return "Editor";
	}

	void ToggleMasterActivation()
	{
		MasterActivate = !MasterActivate;
	}
};
