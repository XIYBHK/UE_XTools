// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "InputCoreTypes.h"
#include "EdGraph/EdGraphNode.h"
#include "Framework/Commands/InputChord.h"
#include "BlueprintAssistSettings.generated.h"

#define BA_DEBUG_EARLY_EXIT(string) do { if (UBASettings::HasDebugSetting(string)) return; } while(0)
#define BA_DEBUG(string) GetDefault<UBASettings>()->BlueprintAssistDebug.Contains(string)

class UEdGraph;

UENUM()
enum class EBACacheSaveLocation : uint8
{
	/** Save to PluginFolder/NodeSizeCache/PROJECT_ID.json */
	Plugin UMETA(DisplayName = "插件目录"),

	/** Save to ProjectFolder/Saved/BlueprintAssist/BlueprintAssistCache.json */
	Project UMETA(DisplayName = "项目目录"),
};

UENUM()
enum class EBANodeFormattingStyle : uint8
{
	Expanded UMETA(DisplayName = "展开式"),
	Compact UMETA(DisplayName = "紧凑式"),
};

UENUM()
enum class EBAParameterFormattingStyle : uint8
{
	Helixing UMETA(DisplayName = "螺旋排列"),
	LeftSide UMETA(DisplayName = "左侧排列"),
};

UENUM()
enum class EBAWiringStyle : uint8
{
	AlwaysMerge UMETA(DisplayName = "始终合并"),
	MergeWhenNear UMETA(DisplayName = "靠近时合并"),
	SingleWire UMETA(DisplayName = "单独连线"),
};

UENUM()
enum class EBAAutoFormatting : uint8
{
	Never UMETA(DisplayName = "从不"),
	FormatAllConnected UMETA(DisplayName = "格式化所有连接的节点"),
	FormatSingleConnected UMETA(DisplayName = "相对单个连接节点格式化"),
};

UENUM()
enum class EBAFormatAllStyle : uint8
{
	Simple UMETA(DisplayName = "简单模式（单列）"),
	Smart UMETA(DisplayName = "智能模式（根据节点位置创建列）"),
	NodeType UMETA(DisplayName = "节点类型（按节点类型分列）"),
};

UENUM()
enum class EBAFormatAllHorizontalAlignment : uint8
{
	RootNode UMETA(DisplayName = "根节点（对齐节点树根节点的左侧）"),
	Comment UMETA(DisplayName = "注释框（对齐包含的注释框左侧）"),
};

UENUM()
enum class EBAFormatterType : uint8
{
	Blueprint UMETA(DisplayName = "蓝图"),
	BehaviorTree UMETA(DisplayName = "行为树"),
	Simple UMETA(DisplayName = "简单格式化器"),
};

UENUM()
enum class EBAAutoZoomToNode : uint8
{
	Never UMETA(DisplayName = "从不"),
	Always UMETA(DisplayName = "始终"),
	Outside_Viewport UMETA(DisplayName = "视口外时"),
};

UENUM()
enum class EBAFunctionAccessSpecifier : uint8
{
	Public UMETA(DisplayName = "公有"),
	Protected UMETA(DisplayName = "保护"),
	Private UMETA(DisplayName = "私有"),
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
	FIntPoint Padding = FIntPoint(96, 96);

	/* Auto formatting method to be used for this graph */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	EBAAutoFormatting AutoFormatting = EBAAutoFormatting::FormatAllConnected;

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

UCLASS(config = EditorPerProjectUserSettings)
class XTOOLS_BLUEPRINTASSIST_API UBASettings final : public UObject
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

	/** Enable or disable the BlueprintAssist plugin */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (ConfigRestartRequired = true, DisplayName = "启用插件", Tooltip = "启用或禁用 Blueprint Assist 插件（需要重启编辑器生效）"))
	bool bEnablePlugin = true;

	/* Add the BlueprintAssist widget to the toolbar */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "添加工具栏控件", Tooltip = "将 Blueprint Assist 控件添加到工具栏"))
	bool bAddToolbarWidget;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "选中引脚高亮颜色", Tooltip = "更改选中引脚周围边框的颜色"))
	FLinearColor SelectedPinHighlightColor;

	/* Sets the 'Comment Bubble Pinned' bool for all nodes on the graph (Auto Size Comment plugin handles this value for comments) */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "启用全局注释气泡固定", Tooltip = "为图表上的所有节点设置注释气泡固定状态（Auto Size Comment 插件会处理注释的此值）"))
	bool bEnableGlobalCommentBubblePinned;

	/* The global 'Comment Bubble Pinned' value */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (EditCondition = "bEnableGlobalCommentBubblePinned", DisplayName = "全局注释气泡固定值", Tooltip = "全局注释气泡固定的默认值"))
	bool bGlobalCommentBubblePinnedValue;

	/* Automatically add parent nodes to event nodes */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "自动添加父节点", Tooltip = "自动为事件节点添加父节点"))
	bool bAutoAddParentNode;

	/* Enable shake node to break all connections feature */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "启用晃动断开连接", Tooltip = "按住节点晃动几下使其断开所有连接"))
	bool bEnableShakeNodeOffWire;

	/* Time window to detect shake movements (in seconds) */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "晃动检测时间窗口", Tooltip = "检测晃动移动的时间窗口（秒），在指定时间内晃动达到次数时触发断开连接"))
	float ShakeNodeOffWireTimeWindow;

	/* Automatically rename Function getters and setters when the Function is renamed */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "自动重命名访问器", Tooltip = "当函数重命名时自动重命名 Getter 和 Setter 函数"))
	bool bAutoRenameGettersAndSetters;

	/* Merge the generate getter and setter into one button */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "合并生成访问器按钮", Tooltip = "将生成 Getter 和 Setter 的按钮合并为一个"))
	bool bMergeGenerateGetterAndSetterButton;

	/* Distance the viewport moves when running the Shift Camera command. Scaled by zoom distance. */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "移动相机距离", Tooltip = "执行移动相机命令时视口移动的距离，按缩放距离缩放"))
	int ShiftCameraDistance;

	/* Enable more slower but more accurate node size caching */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "精确节点大小缓存", Tooltip = "启用较慢但更准确的节点大小缓存"))
	bool bSlowButAccurateSizeCaching;

	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "缓存保存位置", Tooltip = "选择节点大小缓存的保存位置"))
	EBACacheSaveLocation CacheSaveLocation;

	/* Save the node size cache to a file (located in the the plugin folder) */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "保存缓存到文件", Tooltip = "将节点大小缓存保存到文件（位于插件目录）"))
	bool bSaveBlueprintAssistCacheToFile;

	/* Determines if we should auto zoom to a newly created node */
	UPROPERTY(EditAnywhere, config, Category = General, meta = (DisplayName = "自动缩放到节点", Tooltip = "确定是否自动缩放到新创建的节点"))
	EBAAutoZoomToNode AutoZoomToNodeBehavior = EBAAutoZoomToNode::Outside_Viewport;

	/* Supported asset editors by name */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = General, meta = (DisplayName = "支持的资源编辑器", Tooltip = "按名称列出支持的资源编辑器"))
	TArray<FName> SupportedAssetEditors;

	/* Supported graph editors by name */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = General, meta = (DisplayName = "支持的图表编辑器", Tooltip = "按名称列出支持的图表编辑器"))
	TArray<FName> SupportedGraphEditors;

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
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "参数格式化样式", Tooltip = "确定参数节点的定位方式"))
	EBAParameterFormattingStyle ParameterStyle;

	/* Determines how execution wires are created */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "执行连线样式", Tooltip = "确定执行连线的创建方式"))
	EBAWiringStyle ExecutionWiringStyle;

	/* Determines how parameter wires are created */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "参数连线样式", Tooltip = "确定参数连线的创建方式"))
	EBAWiringStyle ParameterWiringStyle;

	/* Reuse knot nodes instead of creating new ones every time */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "使用连接点节点池", Tooltip = "重用连接点节点而不是每次都创建新的"))
	bool bUseKnotNodePool;

	/* Should helixing be disabled if there are multiple linked pins */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (InlineEditConditionToggle, DisplayName = "多引脚时禁用螺旋排列", Tooltip = "当存在多个链接引脚时是否禁用螺旋排列"))
	bool bDisableHelixingWithMultiplePins;

	/* Disable helixing if the number of linked parameter pins is >= than this number */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bDisableHelixingWithMultiplePins", DisplayName = "禁用螺旋排列引脚数量", Tooltip = "当链接的参数引脚数量大于等于此数值时禁用螺旋排列"))
	int32 DisableHelixingPinCount;

	/* Whether to use HelixingHeightMax and SingleNodeMaxHeight */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "限制螺旋排列高度", Tooltip = "是否使用螺旋排列最大高度和单节点最大高度限制"))
	bool bLimitHelixingHeight;

	/* Helixing is disabled if the total height of the parameter nodes is larger than this value */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bLimitHelixingHeight", DisplayName = "螺旋排列最大高度", Tooltip = "当参数节点的总高度大于此值时禁用螺旋排列"))
	int HelixingHeightMax;

	/* Helixing is disabled if a single node is taller than this value */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bLimitHelixingHeight", DisplayName = "单节点最大高度", Tooltip = "当单个节点高度大于此值时禁用螺旋排列"))
	int SingleNodeMaxHeight;

	/* Cache node sizes of any newly detected nodes. Checks upon opening a blueprint or when a new node is added to the graph. */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "检测新节点并缓存大小", Tooltip = "缓存任何新检测到的节点大小。在打开蓝图或向图表添加新节点时检查"))
	bool bDetectNewNodesAndCacheNodeSizes;

	/* Refresh node sizes before formatting */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "格式化前刷新节点大小", Tooltip = "在格式化之前刷新节点大小"))
	bool bRefreshNodeSizeBeforeFormatting;

	/* Create knot nodes */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "创建连接点节点", Tooltip = "自动创建连接点节点以优化连线"))
	bool bCreateKnotNodes;

	/* Add spacing to nodes so they are always in front of their input parameters */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "节点位于参数前方", Tooltip = "为节点添加间距使其始终位于输入参数的前方"))
	bool bExpandNodesAheadOfParameters;

	/* Add spacing to nodes which have many connections, fixing hard to read wires */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "按高度展开节点", Tooltip = "为具有多个连接的节点添加间距，修复难以阅读的连线"))
	bool bExpandNodesByHeight;

	/* Add spacing to parameter nodes which have many connections, fixing hard to read wires */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "按高度展开参数", Tooltip = "为具有多个连接的参数节点添加间距，修复难以阅读的连线"))
	bool bExpandParametersByHeight;

	/* Snap nodes to grid (in the x-axis) after formatting */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (DisplayName = "吸附到网格", Tooltip = "格式化后将节点吸附到网格（在 X 轴上）"))
	bool bSnapToGrid;

	////////////////////////////////////////////////////////////
	/// Format All
	////////////////////////////////////////////////////////////

	/* Determines how nodes are positioned into columns when running formatting all nodes */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "全部格式化样式", Tooltip = "确定运行全部格式化时节点如何排列成列"))
	EBAFormatAllStyle FormatAllStyle;

	/* Determines how nodes are aligned horizontally */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "水平对齐方式", Tooltip = "确定节点的水平对齐方式"))
	EBAFormatAllHorizontalAlignment FormatAllHorizontalAlignment;

	/* x values defines padding between columns, y value defines horizontal padding between node trees */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "全部格式化内边距", Tooltip = "X 值定义列之间的内边距，Y 值定义节点树之间的水平内边距"))
	FIntPoint FormatAllPadding;

	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta=(InlineEditConditionToggle, DisplayName = "注释内使用特定内边距", Tooltip = "在注释内使用特定的垂直间距"))
	bool bUseFormatAllPaddingInComment;

	/* Determines the vertical spacing for the Format All command when event nodes are in the same comment */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta=(EditCondition="bUseFormatAllPaddingInComment", DisplayName = "注释内垂直间距", Tooltip = "当事件节点在同一注释内时全部格式化命令的垂直间距"))
	int32 FormatAllPaddingInComment;

	/* Call the format all function when a new event node is added to the graph */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "自动定位事件节点", Tooltip = "将新事件节点添加到图表时调用全部格式化功能"))
	bool bAutoPositionEventNodes;

	/* Call the format all function when ANY new node is added to the graph. Useful for when the 'UseColumnsForFormatAll' setting is on. */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta = (DisplayName = "始终全部格式化", Tooltip = "将任何新节点添加到图表时调用全部格式化功能，在启用列式布局时很有用"))
	bool bAlwaysFormatAll;

	////////////////////////////////////////////////////////////
	// Blueprint formatting
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "蓝图格式化器设置", Tooltip = "蓝图格式化器的详细设置"))
	FBAFormatterSettings BlueprintFormatterSettings;

	/* Padding used between parameter nodes */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "参数节点间距", Tooltip = "参数节点之间使用的内边距"))
	FIntPoint BlueprintParameterPadding;

	/* Offsets for execution knot tracks */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "执行连接点轨道设置", Tooltip = "执行连接点轨道的偏移设置"))
	FBAKnotTrackSettings BlueprintExecutionKnotSettings;

	/* Offsets for parameter knot tracks */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "参数连接点轨道设置", Tooltip = "参数连接点轨道的偏移设置"))
	FBAKnotTrackSettings BlueprintParameterKnotSettings;

	/* Blueprint formatting will be used for these types of graphs (you can see the type of a graph with the PrintGraphInfo command, default: unbound) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "使用蓝图格式化的图表类型", Tooltip = "这些类型的图表将使用蓝图格式化（可使用 PrintGraphInfo 命令查看图表类型，默认: unbound）"))
	TArray<FName> UseBlueprintFormattingForTheseGraphs;

	/* When formatting treat delegate pins as execution pins, recommended to turn this option off and use the 'CreateEvent' node */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "将委托引脚视为执行引脚", Tooltip = "格式化时将委托引脚视为执行引脚，建议关闭此选项并使用 CreateEvent 节点"))
	bool bTreatDelegatesAsExecutionPins;

	/* Center node execution branches (Default: center nodes with 3 or more branches) */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "居中节点执行分支", Tooltip = "居中节点执行分支（默认：居中具有 3 个或更多分支的节点）"))
	bool bCenterBranches;

	/* Only center branches if we have this (or more) number of branches */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (EditCondition = "bCenterBranches", DisplayName = "最少分支数量", Tooltip = "仅当分支数量达到此值或更多时才居中"))
	int NumRequiredBranches;

	/* Center parameters nodes with multiple links */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "居中参数分支", Tooltip = "居中具有多个链接的参数节点"))
	bool bCenterBranchesForParameters;

	/* Only center parameters which have this many (or more) number of links */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (EditCondition = "bCenterBranchesForParameters", DisplayName = "参数最少链接数量", Tooltip = "仅当参数链接数量达到此值或更多时才居中"))
	int NumRequiredBranchesForParameters;

	/* Vertical spacing from the last linked pin */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "垂直引脚间距", Tooltip = "从最后链接的引脚开始的垂直间距"))
	int VerticalPinSpacing;

	/* Vertical spacing from the last linked pin for parameters */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "参数垂直引脚间距", Tooltip = "参数从最后链接的引脚开始的垂直间距"))
	int ParameterVerticalPinSpacing;

	/* Spacing used between wire tracks */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "连接点轨道间距", Tooltip = "连线轨道之间使用的间距"))
	int BlueprintKnotTrackSpacing;

	/* If the knot's vertical dist to the linked pin is less than this value, it won't be created */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting, meta = (DisplayName = "剔除连接点垂直阈值", Tooltip = "如果连接点到链接引脚的垂直距离小于此值，则不会创建"))
	int CullKnotVerticalThreshold;

	/* The width between pins required for a knot node to be created */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (DisplayName = "连接点节点距离阈值", Tooltip = "创建连接点节点所需的引脚之间的宽度"))
	int KnotNodeDistanceThreshold;

	////////////////////////////////////////////////////////////
	// Other Graphs
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = OtherGraphs, meta = (DisplayName = "非蓝图图表格式化器设置", Tooltip = "为其他图表类型（如行为树、材质图表等）配置格式化设置"))
	TMap<FName, FBAFormatterSettings> NonBlueprintFormatterSettings;

	////////////////////////////////////////////////////////////
	// Comment Settings
	////////////////////////////////////////////////////////////

	/* Apply comment padding when formatting */
	UPROPERTY(EditAnywhere, config, Experimental, Category = CommentSettings, meta = (DisplayName = "应用注释内边距", Tooltip = "格式化时应用注释内边距"))
	bool bApplyCommentPadding;

	/* Add knot nodes to comments after formatting */
	UPROPERTY(EditAnywhere, config, Category = CommentSettings, meta = (DisplayName = "将连接点添加到注释", Tooltip = "格式化后将连接点节点添加到注释中"))
	bool bAddKnotNodesToComments;

	/* Padding around the comment box. Make sure this is the same as in the AutoSizeComments setting */
	UPROPERTY(EditAnywhere, config, Category = CommentSettings, meta = (DisplayName = "注释节点内边距", Tooltip = "注释框周围的内边距，确保与 Auto Size Comments 设置中的值相同"))
	FIntPoint CommentNodePadding;

	////////////////////////////////////////////////////////////
	// Create Variable defaults
	////////////////////////////////////////////////////////////

	/* Enable Variable defaults */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (DisplayName = "启用变量默认值", Tooltip = "为新创建的变量启用默认值设置"))
	bool bEnableVariableDefaults;

	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (DisplayName = "应用到事件调度器", Tooltip = "将变量默认值应用到事件调度器"))
	bool bApplyVariableDefaultsToEventDispatchers;

	/* Variable default Instance Editable */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "实例可编辑", Tooltip = "变量默认实例可编辑"))
	bool bDefaultVariableInstanceEditable;

	/* Variable default Blueprint Read Only */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "蓝图只读", Tooltip = "变量默认蓝图只读"))
	bool bDefaultVariableBlueprintReadOnly;

	/* Variable default Expose on Spawn */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "生成时公开", Tooltip = "变量默认在生成时公开"))
	bool bDefaultVariableExposeOnSpawn;

	/* Variable default Private */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "私有", Tooltip = "变量默认为私有"))
	bool bDefaultVariablePrivate;

	/* Variable default Expose to Cinematics */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "公开到过场动画", Tooltip = "变量默认公开到过场动画"))
	bool bDefaultVariableExposeToCinematics;

	/* Variable default name */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量名", Tooltip = "新变量的默认名称"))
	FString DefaultVariableName;

	/* Variable default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认工具提示", Tooltip = "新变量的默认工具提示"))
	FText DefaultVariableTooltip;

	/* Variable default Category */
	UPROPERTY(EditAnywhere, config, Category = NewVariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认分类", Tooltip = "新变量的默认分类"))
	FText DefaultVariableCategory;

	////////////////////////////////////////////////////////////
	// Create function defaults
	////////////////////////////////////////////////////////////

	/* Enable Function defaults */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (DisplayName = "启用函数默认值", Tooltip = "为新创建的函数启用默认值设置"))
	bool bEnableFunctionDefaults;

	/* Function default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认访问修饰符", Tooltip = "新函数的默认访问修饰符"))
	EBAFunctionAccessSpecifier DefaultFunctionAccessSpecifier;

	/* Function default Pure */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "纯函数", Tooltip = "函数默认为纯函数"))
	bool bDefaultFunctionPure;
	
	/* Function default Const */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "常量函数", Tooltip = "函数默认为常量函数"))
	bool bDefaultFunctionConst;

	/* Function default Exec */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "可执行函数", Tooltip = "函数默认为可执行函数"))
	bool bDefaultFunctionExec;

	/* Function default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认工具提示", Tooltip = "新函数的默认工具提示"))
	FText DefaultFunctionTooltip;

	/* Function default Keywords */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认关键词", Tooltip = "新函数的默认关键词"))
	FText DefaultFunctionKeywords;

	/* Function default Category */
	UPROPERTY(EditAnywhere, config, Category = NewFunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认分类", Tooltip = "新函数的默认分类"))
	FText DefaultFunctionCategory;

	////////////////////////////////////////////////////////////
	// Misc
	////////////////////////////////////////////////////////////

	/* Disable the plugin (requires restarting engine) */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta=(ConfigRestartRequired = "true", DisplayName = "禁用插件", Tooltip = "禁用 Blueprint Assist 插件（需要重启引擎）"))
	bool bDisableBlueprintAssistPlugin;

	/* What category to assign to generated getter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "生成的 Getter 函数分类", Tooltip = "为生成的 Getter 函数分配的分类，覆盖默认函数分类"))
	FText DefaultGeneratedGettersCategory;

	/* What category to assign to generated setter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "生成的 Setter 函数分类", Tooltip = "为生成的 Setter 函数分配的分类，覆盖默认函数分类"))
	FText DefaultGeneratedSettersCategory;

	/* Double click on a node to go to definition. Currently only implemented for Cast blueprint node. */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "双击跳转到定义", Tooltip = "双击节点跳转到定义，目前仅支持 Cast 蓝图节点"))
	bool bEnableDoubleClickGoToDefinition;

	/* Enable invisible knot nodes (re-open any open graphs) */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "启用隐形连接点", Tooltip = "启用隐形连接点节点（需要重新打开已打开的图表）"))
	bool bEnableInvisibleKnotNodes;

	/* Play compile sound on *successful* live compile (may need to restart editor) */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "播放实时编译音效", Tooltip = "在实时编译成功时播放音效（可能需要重启编辑器）"))
	bool bPlayLiveCompileSound;

	/** Input for folder bookmarks */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "文件夹书签快捷键", Tooltip = "文件夹书签的快捷键输入"))
	TArray<FKey> FolderBookmarks;

	/** Duration to differentiate between a click and a drag */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "点击时长", Tooltip = "区分点击和拖动的持续时间"))
	float ClickTime;

	/** Draw a red border around bad comment nodes after formatting */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "高亮异常注释", Tooltip = "格式化后在异常注释节点周围绘制红色边框"))
	bool bHighlightBadComments;

	/** Ignore this (setting for custom debugging) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = Misc, meta = (DisplayName = "调试设置", Tooltip = "忽略此项（用于自定义调试的设置）"))
	TArray<FString> BlueprintAssistDebug;

	////////////////////////////////////////////////////////////
	// Accessibility
	////////////////////////////////////////////////////////////

	/**
	 * When caching nodes, the viewport will jump to each node and this can cause discomfort for photosensitive users.
	 * This setting displays an overlay to prevent this.
	 */
	UPROPERTY(EditAnywhere, config, Category = Accessibility, meta = (DisplayName = "缓存节点时显示遮罩", Tooltip = "缓存节点时视口会跳转到每个节点，这可能会导致光敏用户不适，此设置显示遮罩以防止此问题"))
	bool bShowOverlayWhenCachingNodes;

	/* Number of pending caching nodes required to show the progress bar in the center of the overlay */
	UPROPERTY(EditAnywhere, config, Category = Accessibility, meta = (EditCondition = "bShowOverlayWhenCachingNodes", DisplayName = "显示进度条所需节点数", Tooltip = "在遮罩中央显示进度条所需的待缓存节点数量"))
	int RequiredNodesToShowOverlayProgressBar;

	////////////////////////////////////////////////////////////
	// Experimental
	////////////////////////////////////////////////////////////

	/* Faster formatting will only format chains of nodes have been moved or had connections changed. Greatly increases speed of the format all command. */
	UPROPERTY(EditAnywhere, config, Category = Experimental, meta = (DisplayName = "启用快速格式化", Tooltip = "快速格式化仅格式化已移动或连接已更改的节点链，大幅提高全部格式化命令的速度"))
	bool bEnableFasterFormatting;

	/* Align execution nodes to the 8x8 grid when formatting */
	UPROPERTY(EditAnywhere, config, Category = Experimental, meta = (DisplayName = "对齐到 8x8 网格", Tooltip = "格式化时将执行节点对齐到 8x8 网格"))
	bool bAlignExecNodesTo8x8Grid;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	static FBAFormatterSettings GetFormatterSettings(UEdGraph* Graph);
	static FBAFormatterSettings* FindFormatterSettings(UEdGraph* Graph);
	
	static FORCEINLINE bool HasDebugSetting(const FString& Setting)
	{
		return Get().BlueprintAssistDebug.Contains(Setting);
	}
};

class FBASettingsDetails final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
