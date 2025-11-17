// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "EdGraph/EdGraphNode.h"
#include "Framework/Commands/InputChord.h"
#include "BlueprintAssistSettings.generated.h"

class UEdGraph;

UENUM(meta = (ToolTip = "节点格式化样式"))
enum class EBANodeFormattingStyle : uint8
{
	Expanded UMETA(DisplayName = "展开"),
	Compact UMETA(DisplayName = "紧凑"),
};

UENUM(meta = (ToolTip = "参数格式化样式"))
enum class EBAParameterFormattingStyle : uint8
{
	Helixing UMETA(DisplayName = "螺旋"),
	LeftSide UMETA(DisplayName = "左侧"),
};

UENUM(meta = (ToolTip = "连线样式"))
enum class EBAWiringStyle : uint8
{
	AlwaysMerge UMETA(DisplayName = "总是合并"),
	MergeWhenNear UMETA(DisplayName = "靠近时合并"),
	SingleWire UMETA(DisplayName = "单根连线"),
};

UENUM(meta = (ToolTip = "自动格式化设置"))
enum class EBAAutoFormatting : uint8
{
	Never UMETA(DisplayName = "从不"),
	FormatAllConnected UMETA(DisplayName = "格式化所有连接的节点"),
	FormatSingleConnected UMETA(DisplayName = "相对于连接的节点格式化"),
};

UENUM(meta = (ToolTip = "全部格式化样式"))
enum class EBAFormatAllStyle : uint8
{
	Simple UMETA(DisplayName = "简单(单列)"),
	Smart UMETA(DisplayName = "智能(根据节点位置创建列)"),
	NodeType UMETA(DisplayName = "节点类型(按节点类型分列)"),
};

UENUM(meta = (ToolTip = "全部格式化水平对齐方式"))
enum class EBAFormatAllHorizontalAlignment : uint8
{
	RootNode UMETA(DisplayName = "根节点(对齐节点树根节点的左侧)"),
	Comment UMETA(DisplayName = "注释(对齐任何包含注释的左侧)"),
};

UENUM(meta = (ToolTip = "格式化器类型"))
enum class EBAFormatterType : uint8
{
	Blueprint UMETA(DisplayName = "蓝图"),
	BehaviorTree UMETA(DisplayName = "行为树"),
	Simple UMETA(DisplayName = "简单格式化器"),
};

USTRUCT()
struct FBAKnotTrackSettings
{
	GENERATED_BODY()

	/* Knot nodes x-offset for regular execution wires */
	UPROPERTY(EditAnywhere, config, Category = "BlueprintFormatting")
	int32 KnotXOffset = 0;

	/* Knot node offset for wires that flow backwards in execution */
	UPROPERTY(EditAnywhere, config, Category = "BlueprintFormatting")
	FIntPoint LoopingOffset = FIntPoint(0, 0);
};

USTRUCT()
struct FBAFormatterSettings
{
	GENERATED_BODY()

	FBAFormatterSettings();

	FBAFormatterSettings(FIntPoint InPadding, EBAAutoFormatting InAutoFormatting, EEdGraphPinDirection InFormatterDirection, TArray<FName> InRootNodes = TArray<FName>())
		: Padding(InPadding)
		, AutoFormatting(InAutoFormatting)
		, FormatterDirection(InFormatterDirection)
		, RootNodes(InRootNodes)
	{
	}

	/* Setting to enable / disable all behaviour for this graph type */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings)
	bool bEnabled = true;

	/* Formatter to use */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	EBAFormatterType FormatterType = EBAFormatterType::Simple;

	/* Padding used when formatting nodes */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	FIntPoint Padding = FIntPoint(100, 100);

	/* Auto formatting method to be used for this graph */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	EBAAutoFormatting AutoFormatting = EBAAutoFormatting::Never;

	/* Direction of execution flow in this graph */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	TEnumAsByte<EEdGraphPinDirection> FormatterDirection;

	/* Names of any root nodes that this graph uses */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	TArray<FName> RootNodes;

	/* Name of the execution pin for this graph type */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	FName ExecPinName;

	FString ToString() const
	{
		return FString::Printf(TEXT("FormatterType %d | ExecPinName %s"), FormatterType, *ExecPinName.ToString());
	}

	EBAAutoFormatting GetAutoFormatting() const;
};

UCLASS(config = EditorPerProjectUserSettings, DisplayName = "BA设置")
class XTOOLS_BLUEPRINTASSIST_API UBASettings final : public UBASettingsBase
{
	GENERATED_BODY()

public:
	UBASettings(const FObjectInitializer& ObjectInitializer);

	static FORCEINLINE const UBASettings& Get()
	{
		return *GetDefault<UBASettings>();
	}
	static FORCEINLINE UBASettings& GetMutable()
	{
		return *GetMutableDefault<UBASettings>();
	}

	////////////////////////////////////////////////////////////
	// General
	////////////////////////////////////////////////////////////

	/* Cache node sizes of any newly detected nodes. Checks upon opening a blueprint or when a new node is added to the graph. */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "检测新节点并缓存大小", Tooltip = "缓存任何新检测到的节点大小。在打开蓝图或向图表添加新节点时检查"))
	bool bDetectNewNodesAndCacheNodeSizes;

	/* Supported asset editors by name */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = General, meta = (DisplayName = "支持的资产编辑器", Tooltip = "按名称列出支持的资产编辑器"))
	TArray<FName> SupportedAssetEditors;

	/* Supported graph editors by name */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = General, meta = (DisplayName = "支持的图表编辑器", Tooltip = "按名称列出支持的图表编辑器"))
	TArray<FName> SupportedGraphEditors;

	/* Enable shake node to break all connections feature */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "启用晃动断开连接", Tooltip = "按住节点晃动几下使其断开所有连接"))
	bool bEnableShakeNodeOffWire;

	/* Time window to detect shake movements (in seconds) */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "晃动检测时间窗口", Tooltip = "检测晃动移动的时间窗口(秒),在指定时间内晃动达到次数时触发断开连接"))
	float ShakeNodeOffWireTimeWindow;

	////////////////////////////////////////////////////////////
	// Formatting options
	////////////////////////////////////////////////////////////

	/* Enabling this is the same as setting auto formatting to Never for all graphs */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "全局禁用自动格式化", Tooltip = "启用此选项相当于为所有图表将自动格式化设置为从不"))
	bool bGloballyDisableAutoFormatting;

	/* Determines how execution nodes are positioned */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "节点格式化样式", Tooltip = "确定执行节点的定位方式"))
	EBANodeFormattingStyle FormattingStyle;

	/* Determines how parameters are positioned */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "参数样式", Tooltip = "确定参数的定位方式"))
	EBAParameterFormattingStyle ParameterStyle;

	/* Determines how execution wires are created */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "执行连线样式", Tooltip = "确定执行连线的创建方式"))
	EBAWiringStyle ExecutionWiringStyle;

	/* Determines how parameter wires are created */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "参数连线样式", Tooltip = "确定参数连线的创建方式"))
	EBAWiringStyle ParameterWiringStyle;

	/* Reuse knot nodes instead of creating new ones every time */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "重用结点节点池", Tooltip = "重用结点节点而不是每次都创建新的"))
	bool bUseKnotNodePool;

	/* Should helixing be disabled if there are multiple linked pins */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (InlineEditConditionToggle, DisplayName = "多引脚时禁用螺旋", Tooltip = "如果有多个链接的引脚则禁用螺旋排列"))
	bool bDisableHelixingWithMultiplePins;

	/* Disable helixing if the number of linked parameter pins is >= than this number */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bDisableHelixingWithMultiplePins", DisplayName = "禁用螺旋的引脚数量", Tooltip = "当链接的参数引脚数量大于等于此数值时禁用螺旋排列"))
	int32 DisableHelixingPinCount;

	/* Whether to use HelixingHeightMax and SingleNodeMaxHeight */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "限制螺旋高度", Tooltip = "是否使用螺旋最大高度和单节点最大高度限制"))
	bool bLimitHelixingHeight;

	/* Helixing is disabled if the total height of the parameter nodes is larger than this value */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bLimitHelixingHeight", DisplayName = "螺旋最大高度", Tooltip = "如果参数节点的总高度大于此值则禁用螺旋排列"))
	int HelixingHeightMax;

	/* Helixing is disabled if a single node is taller than this value */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bLimitHelixingHeight", DisplayName = "单节点最大高度", Tooltip = "如果单个节点高于此值则禁用螺旋排列"))
	int SingleNodeMaxHeight;

	/* Create knot nodes */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "创建结点节点", Tooltip = "在格式化时创建结点节点"))
	bool bCreateKnotNodes;

	/* Add spacing to nodes so they are always in front of their input parameters */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "在参数前扩展节点", Tooltip = "添加节点间距使它们始终位于其输入参数前面"))
	bool bExpandNodesAheadOfParameters;

	/* Add horizontal spacing depending on how vertically far from they are from the linked node */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "按高度扩展节点", Tooltip = "根据与链接节点的垂直距离添加水平间距"))
	bool bExpandNodesByHeight;

	/* The maximum horizontal distance allowed to be expanded */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "节点扩展最大距离", Tooltip = "允许扩展的最大水平距离"))
	float ExpandNodesMaxDist;

	/* Add horizontal spacing depending on how vertically far from they are from the linked node (for parameter nodes) */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "按高度扩展参数", Tooltip = "根据与链接节点的垂直距离添加水平间距(用于参数节点)"))
	bool bExpandParametersByHeight;

	/* The maximum horizontal distance allowed to be expanded (for parameter nodes) */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "参数扩展最大距离", Tooltip = "允许扩展的最大水平距离(用于参数节点)"))
	float ExpandParametersMaxDist;

	/* Snap nodes to grid (in the x-axis) after formatting */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "对齐到网格", Tooltip = "格式化后将节点对齐到网格(X轴)"))
	bool bSnapToGrid;

	/* Skip auto formatting if the new node caused any pins to disconnect */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = FormattingOptions, meta = (DisplayName = "断开引脚后跳过自动格式化", Tooltip = "如果新节点导致任何引脚断开则跳过自动格式化"))
	bool bSkipAutoFormattingAfterBreakingPins;

	////////////////////////////////////////////////////////////
	/// Format All
	////////////////////////////////////////////////////////////

	/* Determines how nodes are positioned into columns when running formatting all nodes */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "全部格式化样式", Tooltip = "确定运行全部格式化时节点如何排列到列中"))
	EBAFormatAllStyle FormatAllStyle;

	/* Determines how nodes are aligned horizontally */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "水平对齐方式", Tooltip = "确定节点如何水平对齐"))
	EBAFormatAllHorizontalAlignment FormatAllHorizontalAlignment;

	/* x values defines padding between columns, y value defines horizontal padding between node trees */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "全部格式化间距", Tooltip = "X值定义列之间的间距，Y值定义节点树之间的水平间距"))
	FIntPoint FormatAllPadding;

	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta=(InlineEditConditionToggle, DisplayName = "使用注释内间距", Tooltip = "在注释内使用自定义的格式化间距"))
	bool bUseFormatAllPaddingInComment;

	/* Determines the vertical spacing for the Format All command when event nodes are in the same comment */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta=(EditCondition="bUseFormatAllPaddingInComment", DisplayName = "注释内格式化间距", Tooltip = "当事件节点在同一注释中时的垂直间距"))
	int32 FormatAllPaddingInComment;

	/* Call the format all function when a new event node is added to the graph */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "自动定位事件节点", Tooltip = "向图表添加新事件节点时调用全部格式化功能"))
	bool bAutoPositionEventNodes;

	/* Call the format all function when ANY new node is added to the graph. Useful for when the 'UseColumnsForFormatAll' setting is on. */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "始终全部格式化", Tooltip = "向图表添加任何新节点时调用全部格式化功能"))
	bool bAlwaysFormatAll;

	////////////////////////////////////////////////////////////
	// Blueprint formatting
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "蓝图格式化设置", Tooltip = "蓝图图表的格式化设置"))
	FBAFormatterSettings BlueprintFormatterSettings;

	/* Padding used between parameter nodes */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "蓝图参数内边距", Tooltip = "参数节点之间使用的内边距"))
	FIntPoint BlueprintParameterPadding;

	/* Offsets for execution knot tracks */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "执行结点轨道设置", Tooltip = "执行结点轨道的偏移设置"))
	FBAKnotTrackSettings BlueprintExecutionKnotSettings;

	/* Offsets for parameter knot tracks */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "参数结点轨道设置", Tooltip = "参数结点轨道的偏移设置"))
	FBAKnotTrackSettings BlueprintParameterKnotSettings;

	/* Blueprint formatting will be used for these types of graphs (you can see the type of a graph with the PrintGraphInfo command, default: unbound) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "使用蓝图格式化的图表", Tooltip = "这些类型的图表将使用蓝图格式化(使用PrintGraphInfo命令查看图表类型)"))
	TArray<FName> UseBlueprintFormattingForTheseGraphs;

	/* When formatting treat delegate pins as execution pins, recommended to turn this option off and use the 'CreateEvent' node */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "将委托视为执行引脚", Tooltip = "格式化时将委托引脚视为执行引脚,建议关闭此选项并使用CreateEvent节点"))
	bool bTreatDelegatesAsExecutionPins;

	/* Center node execution branches (Default: center nodes with 3 or more branches) */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "居中执行分支", Tooltip = "将节点执行分支居中对齐(默认:3个或更多分支时居中)"))
	bool bCenterBranches;

	/* Only center branches if we have this (or more) number of branches */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (EditCondition = "bCenterBranches", DisplayName = "所需分支数量", Tooltip = "只有达到此数量或更多的分支才会居中"))
	int NumRequiredBranches;

	/* Center parameters nodes with multiple links */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "居中参数分支", Tooltip = "将具有多个链接的参数节点居中"))
	bool bCenterBranchesForParameters;

	/* Only center parameters which have this many (or more) number of links */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (EditCondition = "bCenterBranchesForParameters", DisplayName = "参数所需分支数量", Tooltip = "只有达到此数量或更多链接的参数才会居中"))
	int NumRequiredBranchesForParameters;

	/* Vertical spacing from the last linked pin */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "垂直引脚间距", Tooltip = "从最后链接的引脚开始的垂直间距"))
	int VerticalPinSpacing;

	/* Vertical spacing from the last linked pin for parameters */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "参数垂直引脚间距", Tooltip = "参数的垂直引脚间距"))
	int ParameterVerticalPinSpacing;

	/* Spacing used between wire tracks */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "结点轨道间距", Tooltip = "连线轨道之间使用的间距"))
	int BlueprintKnotTrackSpacing;

	/* If the knot's vertical dist to the linked pin is less than this value, it won't be created */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "剔除结点垂直阈值", Tooltip = "如果结点到链接引脚的垂直距离小于此值则不会创建"))
	int CullKnotVerticalThreshold;

	/* The width between pins required for a knot node to be created */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "结点节点距离阈值", Tooltip = "创建结点节点所需的引脚之间的宽度"))
	int KnotNodeDistanceThreshold;

	////////////////////////////////////////////////////////////
	// Other Graphs
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = OtherGraphs, meta = (DisplayName = "非蓝图格式化设置", Tooltip = "其他类型图表的格式化设置"))
	TMap<FName, FBAFormatterSettings> NonBlueprintFormatterSettings;

	/* Extra padding to add between branches */
	UPROPERTY(EditAnywhere, config, Category = BehaviorTree, meta = (DisplayName = "行为树分支额外间距", Tooltip = "在分支之间添加的额外内边距"))
	float BehaviorTreeBranchExtraPadding;

	////////////////////////////////////////////////////////////
	// Comment Settings
	////////////////////////////////////////////////////////////

	/* Apply comment padding when formatting */
	UPROPERTY(EditAnywhere, config, Category = Comments, meta = (DisplayName = "应用注释内边距", Tooltip = "格式化时应用注释内边距"))
	bool bApplyCommentPadding;

	/* Add knot nodes to comments after formatting */
	UPROPERTY(EditAnywhere, config, Category = Comments, meta = (DisplayName = "添加结点到注释", Tooltip = "格式化后将结点节点添加到注释框"))
	bool bAddKnotNodesToComments;

	/* Padding around the comment box. Make sure this is the same as in the AutoSizeComments setting */
	UPROPERTY(EditAnywhere, config, Category = Comments, meta = (DisplayName = "注释节点内边距", Tooltip = "注释框周围的内边距,确保与AutoSizeComments设置中的值相同"))
	FIntPoint CommentNodePadding;

	/* Watch the size of comment nodes and refresh the comment bar title size */
	UPROPERTY(EditAnywhere, config, Category = Comments, meta = (DisplayName = "刷新注释标题栏大小", Tooltip = "监视注释节点大小并刷新注释标题栏大小"))
	bool bRefreshCommentTitleBarSize;

	////////////////////////////////////////////////////////////
	// Accessibility
	////////////////////////////////////////////////////////////

	/**
	 * When caching nodes, the viewport will jump to each node and this can cause discomfort for photosensitive users.
	 * This setting displays an overlay to prevent this.
	 */
	UPROPERTY(EditAnywhere, config, Category = Accessibility, meta = (DisplayName = "缓存节点时显示遮罩", Tooltip = "缓存节点时视口会跳转到每个节点,此设置显示遮罩以防止对光敏感用户造成不适"))
	bool bShowOverlayWhenCachingNodes;

	/* Number of pending caching nodes required to show the progress bar in the center of the overlay */
	UPROPERTY(EditAnywhere, config, Category = Accessibility, meta = (EditCondition = "bShowOverlayWhenCachingNodes", DisplayName = "显示进度条所需节点数", Tooltip = "在遮罩中心显示进度条所需的待缓存节点数量"))
	int RequiredNodesToShowOverlayProgressBar;

	////////////////////////////////////////////////////////////
	// Experimental
	////////////////////////////////////////////////////////////

	/* Faster formatting will only format chains of nodes have been moved or had connections changed. Greatly increases speed of the format all command. */
	UPROPERTY(EditAnywhere, config, Category = Experimental, meta = (DisplayName = "启用快速格式化", Tooltip = "快速格式化只会格式化已移动或连接已更改的节点链,大大提高全部格式化命令的速度"))
	bool bEnableFasterFormatting;

	/* Align execution nodes to the 8x8 grid when formatting */
	UPROPERTY(EditAnywhere, config, Category = Experimental, meta = (DisplayName = "将执行节点对齐到8x8网格", Tooltip = "格式化时将执行节点对齐到8x8网格"))
	bool bAlignExecNodesTo8x8Grid;

	/* Save all before formatting */
	UPROPERTY(EditAnywhere, config, Category = Experimental, meta = (DisplayName = "格式化前保存全部", Tooltip = "格式化前保存所有文件"))
	bool bSaveAllBeforeFormatting;

	/* Run the format all command after saving a graph */
	UPROPERTY(EditAnywhere, config, Category = Experimental, meta = (DisplayName = "保存后全部格式化", Tooltip = "保存图表后运行全部格式化命令"))
	bool bFormatAllAfterSaving;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	static FBAFormatterSettings GetFormatterSettings(UEdGraph* Graph);
	static FBAFormatterSettings* FindFormatterSettings(UEdGraph* Graph);
};

class FBASettingsDetails final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
