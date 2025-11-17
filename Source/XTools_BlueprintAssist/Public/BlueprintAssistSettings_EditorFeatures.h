#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "Framework/Commands/InputChord.h"
#include "Layout/Margin.h"
#include "UObject/Object.h"
#include "BlueprintAssistSettings_EditorFeatures.generated.h"

UENUM(meta = (ToolTip = "函数访问修饰符"))
enum class EBAFunctionAccessSpecifier : uint8
{
	Public UMETA(DisplayName = "公有"),
	Protected UMETA(DisplayName = "保护"),
	Private UMETA(DisplayName = "私有"),
};

UENUM(meta = (ToolTip = "自动缩放到节点"))
enum class EBAAutoZoomToNode : uint8
{
	Never UMETA(DisplayName = "从不"),
	Always UMETA(DisplayName = "总是"),
	Outside_Viewport UMETA(DisplayName = "在视口外"),
};

UENUM(meta = (ToolTip = "引脚选择方法"))
enum class EBAPinSelectionMethod : uint8
{
	/* 选择右侧的执行引脚 */
	Execution UMETA(DisplayName = "执行"),

	/* 选择第一个值(未链接的参数)引脚,否则选择右侧的执行引脚 */
	Value UMETA(DisplayName = "值"),
};


UCLASS(Config = EditorPerProjectUserSettings, DisplayName = "BA设置 编辑器功能")
class XTOOLS_BLUEPRINTASSIST_API UBASettings_EditorFeatures final : public UBASettingsBase
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
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta=(EditCondition="bSetReplicationFlagsAfterRenaming", DisplayName = "无前缀时清除复制标志", Tooltip = "启用后,重命名没有匹配前缀的自定义事件将应用'不复制'标志"))
	bool bClearReplicationFlagsWhenRenamingWithNoPrefix;

	/* Add the according prefix to the title after changing replication flags */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "添加复制前缀到标题", Tooltip = "更改复制标志后将相应的前缀添加到标题"))
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
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "仅选中时绘制轮廓", Tooltip = "只在选中时绘制节点组轮廓"))
	bool bOnlyDrawGroupOutlineWhenSelected;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓颜色", Tooltip = "节点组轮廓的颜色"))
	FLinearColor NodeGroupOutlineColor;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓宽度", Tooltip = "节点组轮廓线的宽度"))
	float NodeGroupOutlineWidth;

	/* Outline margin around each node */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓边距", Tooltip = "每个节点周围的轮廓边距"))
	FMargin NodeGroupOutlineMargin;

	/* Draw a fill to show the node groups for selected nodes */
	UPROPERTY(EditAnywhere, Category = NodeGroup, meta = (DisplayName = "绘制节点组填充", Tooltip = "为选中的节点绘制填充以显示节点组"))
	bool bDrawNodeGroupFill;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupFill", EditConditionHides, DisplayName = "节点组填充颜色", Tooltip = "节点组填充的颜色"))
	FLinearColor NodeGroupFillColor;

	////////////////////////////////////////////////////////////
	/// Graph
	////////////////////////////////////////////////////////////

	/* Distance the viewport moves when running the Shift Camera command. Scaled by zoom distance. */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "移动相机距离", Tooltip = "运行移动相机命令时视口移动的距离,根据缩放距离缩放"))
	int ShiftCameraDistance;

	/* Automatically add parent nodes to event nodes */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "自动添加父节点", Tooltip = "自动将父节点添加到事件节点"))
	bool bAutoAddParentNode;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "选中引脚高亮颜色", Tooltip = "选中引脚周围边框的颜色"))
	FLinearColor SelectedPinHighlightColor;

	/* Determines which pin should be selected when you click on an execution node */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "执行节点引脚选择方式", Tooltip = "确定单击执行节点时应选中哪个引脚"))
	EBAPinSelectionMethod PinSelectionMethod_Execution;

	/* Determines which pin should be selected when you click on a parameter node */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "参数节点引脚选择方式", Tooltip = "确定单击参数节点时应选中哪个引脚"))
	EBAPinSelectionMethod PinSelectionMethod_Parameter;

	/* Sets the 'Comment Bubble Pinned' bool for all nodes on the graph (Auto Size Comment plugin handles this value for comments) */
	UPROPERTY(EditAnywhere, config, Category = "Graph|Comments", meta = (DisplayName = "启用全局注释气泡固定", Tooltip = "为图表上的所有节点设置'注释气泡固定'布尔值"))
	bool bEnableGlobalCommentBubblePinned;

	/* The global 'Comment Bubble Pinned' value */
	UPROPERTY(EditAnywhere, config, Category = "Graph|Comments", meta = (EditCondition = "bEnableGlobalCommentBubblePinned", DisplayName = "全局注释气泡固定值", Tooltip = "全局'注释气泡固定'的值"))
	bool bGlobalCommentBubblePinnedValue;

	/* Determines if we should auto zoom to a newly created node */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "自动缩放到节点行为", Tooltip = "确定是否应自动缩放到新创建的节点"))
	EBAAutoZoomToNode AutoZoomToNodeBehavior = EBAAutoZoomToNode::Outside_Viewport;

	/* Try to insert the node between any current wires when holding down this key */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "插入新节点快捷键", Tooltip = "按住此键时尝试将节点插入到任何当前连线之间"))
	FInputChord InsertNewNodeKeyChord;

	/* When creating a new node from a parameter pin, always try to connect the execution. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "从参数总是连接执行", Tooltip = "从参数引脚创建新节点时总是尝试连接执行,按住插入键将禁用此功能"))
	bool bAlwaysConnectExecutionFromParameter;

	/* When creating a new node from a parameter pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "从参数总是插入", Tooltip = "从参数引脚创建新节点时总是尝试插入到连线之间,按住插入键将禁用此功能"))
	bool bAlwaysInsertFromParameter;

	/* When creating a new node from an execution pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "从执行总是插入", Tooltip = "从执行引脚创建新节点时总是尝试插入到连线之间,按住插入键将禁用此功能"))
	bool bAlwaysInsertFromExecution;

	/* Select the first editable parameter pin when a node is created */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "创建新节点时选中值引脚", Tooltip = "创建节点时选中第一个可编辑的参数引脚"))
	bool bSelectValuePinWhenCreatingNewNodes;

	////////////////////////////////////////////////////////////
	/// General
	////////////////////////////////////////////////////////////

	/* Add the BlueprintAssist widget to the toolbar */
	UPROPERTY(EditAnywhere, config, Category = "General", meta = (DisplayName = "添加工具栏控件", Tooltip = "将BlueprintAssist控件添加到工具栏"))
	bool bAddToolbarWidget;

	/* Automatically rename Function getters and setters when the Function is renamed */
	UPROPERTY(EditAnywhere, config, Category = "General|Getters and Setters", meta = (DisplayName = "自动重命名获取器和设置器", Tooltip = "重命名函数时自动重命名函数的获取器和设置器"))
	bool bAutoRenameGettersAndSetters;

	/* Merge the generate getter and setter into one button */
	UPROPERTY(EditAnywhere, config, Category = "General|Getters and Setters", meta = (DisplayName = "合并生成获取器和设置器按钮", Tooltip = "将生成获取器和设置器合并为一个按钮"))
	bool bMergeGenerateGetterAndSetterButton;

	////////////////////////////////////////////////////////////
	// Create Variable defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on variables when they are created */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "启用变量默认值", Tooltip = "创建变量时设置默认属性"))
	bool bEnableVariableDefaults;

	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "应用变量默认值到事件调度器", Tooltip = "将变量默认值应用到事件调度器"))
	bool bApplyVariableDefaultsToEventDispatchers;

	/* Variable default Instance Editable */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量实例可编辑", Tooltip = "变量默认实例可编辑"))
	bool bDefaultVariableInstanceEditable;

	/* Variable default Blueprint Read Only */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量蓝图只读", Tooltip = "变量默认蓝图只读"))
	bool bDefaultVariableBlueprintReadOnly;

	/* Variable default Expose on Spawn */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量生成时公开", Tooltip = "变量默认生成时公开"))
	bool bDefaultVariableExposeOnSpawn;

	/* Variable default Private */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量私有", Tooltip = "变量默认私有"))
	bool bDefaultVariablePrivate;

	/* Variable default Expose to Cinematics */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量公开到过场动画", Tooltip = "变量默认公开到过场动画"))
	bool bDefaultVariableExposeToCinematics;

	/* Variable default name */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量名称", Tooltip = "变量默认名称"))
	FString DefaultVariableName;

	/* Variable default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量提示", Tooltip = "变量默认提示文本"))
	FText DefaultVariableTooltip;

	/* Variable default Category */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "默认变量分类", Tooltip = "变量默认分类"))
	FText DefaultVariableCategory;

	////////////////////////////////////////////////////////////
	// Create function defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on functions when they are created */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "启用函数默认值", Tooltip = "创建函数时设置默认属性"))
	bool bEnableFunctionDefaults;

	/* Function default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认函数访问说明符", Tooltip = "函数默认访问说明符"))
	EBAFunctionAccessSpecifier DefaultFunctionAccessSpecifier;

	/* Function default Pure */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认函数纯函数", Tooltip = "函数默认为纯函数"))
	bool bDefaultFunctionPure;
	
	/* Function default Const */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认函数常量", Tooltip = "函数默认为常量"))
	bool bDefaultFunctionConst;

	/* Function default Exec */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认函数可执行", Tooltip = "函数默认可执行"))
	bool bDefaultFunctionExec;

	/* Function default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认函数提示", Tooltip = "函数默认提示文本"))
	FText DefaultFunctionTooltip;

	/* Function default Keywords */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认函数关键字", Tooltip = "函数默认关键字"))
	FText DefaultFunctionKeywords;

	/* Function default Category */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "默认函数分类", Tooltip = "函数默认分类"))
	FText DefaultFunctionCategory;

	////////////////////////////////////////////////////////////
	// Custom event defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on custom events when they are created */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (DisplayName = "启用事件默认值", Tooltip = "创建自定义事件时设置默认属性"))
	bool bEnableEventDefaults;

	/* Event default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (EditCondition = "bEnableEventDefaults", DisplayName = "默认事件访问说明符", Tooltip = "事件默认访问说明符"))
	EBAFunctionAccessSpecifier DefaultEventAccessSpecifier;

	/* Event default Net Reliable (for RPC calls) */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (EditCondition = "bEnableEventDefaults", DisplayName = "默认事件网络可靠", Tooltip = "事件默认网络可靠(用于RPC调用)"))
	bool bDefaultEventNetReliable;

	////////////////////////////////////////////////////////////
	// Inputs
	////////////////////////////////////////////////////////////

	/* Copy the pin value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "复制引脚值快捷键", Tooltip = "将引脚值复制到剪贴板"))
	FInputChord CopyPinValueChord;

	/* Paste the hovered value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "粘贴引脚值快捷键", Tooltip = "从剪贴板粘贴值到悬停的引脚"))
	FInputChord PastePinValueChord;

	/* Focus the hovered node in the details panel */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "在详情面板聚焦快捷键", Tooltip = "在详情面板中聚焦悬停的节点"))
	FInputChord FocusInDetailsPanelChord;

	/** Extra input chords to for dragging selected nodes with cursor (same as left-click-dragging) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "额外拖动节点快捷键", Tooltip = "用光标拖动选中节点的额外输入组合键(与左键拖动相同)"))
	TArray<FInputChord> AdditionalDragNodesChords;

	/** Input chords for group dragging (move all linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "组移动快捷键", Tooltip = "组拖动的输入组合键(移动所有链接的节点)"))
	TArray<FInputChord> GroupMovementChords;

	/** Input chords for group dragging (move left linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "左子树移动快捷键", Tooltip = "组拖动的输入组合键(移动左侧链接的节点)"))
	TArray<FInputChord> LeftSubTreeMovementChords;

	/** Input chords for group dragging (move right linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "右子树移动快捷键", Tooltip = "组拖动的输入组合键(移动右侧链接的节点)"))
	TArray<FInputChord> RightSubTreeMovementChords;


	////////////////////////////////////////////////////////////
	// Misc
	////////////////////////////////////////////////////////////

	/* By default the Blueprint Assist Hotkey Menu only displays this plugin's hotkeys. Enable this to display all hotkeys for the editor. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "显示所有快捷键", Tooltip = "默认情况下快捷键菜单仅显示此插件的快捷键,启用此选项可显示编辑器的所有快捷键"))
	bool bDisplayAllHotkeys;

	/* Show the welcome screen when the engine launches */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "启动时显示欢迎屏幕", Tooltip = "引擎启动时显示欢迎屏幕"))
	bool bShowWelcomeScreenOnLaunch;

	/* Double click on a node to go to definition. Currently only implemented for Cast blueprint node. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "启用双击跳转到定义", Tooltip = "双击节点跳转到定义,目前仅对Cast蓝图节点实现"))
	bool bEnableDoubleClickGoToDefinition;

	/* Knot nodes will be hidden (requires graphs to be re-opened) */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "启用隐形结点节点", Tooltip = "结点节点将被隐藏(需要重新打开图表)"))
	bool bEnableInvisibleKnotNodes;

	/* Play sound on successful compile */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "编译成功时播放声音", Tooltip = "编译成功时播放声音"))
	bool bPlayLiveCompileSound;

	/** Input for folder bookmarks */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "文件夹书签", Tooltip = "文件夹书签的输入键"))
	TArray<FKey> FolderBookmarks;

	/** Duration to differentiate between a click and a drag */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "点击时间", Tooltip = "区分点击和拖动的持续时间"))
	float ClickTime;

	/* What category to assign to generated getter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "默认生成的获取器分类", Tooltip = "分配给生成的获取器函数的分类,覆盖DefaultFunctionCategory"))
	FText DefaultGeneratedGettersCategory;

	/* What category to assign to generated setter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "默认生成的设置器分类", Tooltip = "分配给生成的设置器函数的分类,覆盖DefaultFunctionCategory"))
	FText DefaultGeneratedSettersCategory;

	FORCEINLINE static const UBASettings_EditorFeatures& Get() { return *GetDefault<UBASettings_EditorFeatures>(); }
	FORCEINLINE static UBASettings_EditorFeatures& GetMutable() { return *GetMutableDefault<UBASettings_EditorFeatures>(); }

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};

class FBASettingsDetails_EditorFeatures final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};