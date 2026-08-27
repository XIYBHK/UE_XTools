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
	/* Select the right execution pin */
	Execution UMETA(DisplayName = "执行"),

	/* Select the first value (unlinked parameter) pin, otherwise select the right execution pin */
	Value UMETA(DisplayName = "值"),
};

USTRUCT()
struct FBAVariableDefaults
{
	GENERATED_BODY()

	FBAVariableDefaults();

	/* Variable default Private */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量私有", ToolTip = "变量默认私有"))
	bool bDefaultVariablePrivate;

	/* Variable default Instance Editable */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量实例可编辑", ToolTip = "变量默认实例可编辑"))
	bool bDefaultVariableInstanceEditable;

	/* Variable default Blueprint Read Only */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量蓝图只读", ToolTip = "变量默认蓝图只读"))
	bool bDefaultVariableBlueprintReadOnly;

	/* Variable default Expose on Spawn */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量生成时公开", ToolTip = "变量默认生成时公开"))
	bool bDefaultVariableExposeOnSpawn;

	/* Variable default Expose to Cinematics */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量公开到过场动画", ToolTip = "变量默认公开到过场动画"))
	bool bDefaultVariableExposeToCinematics;

	/* Variable default Transient */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量瞬态", ToolTip = "变量默认标记为瞬态，不参与保存或加载"))
	bool bDefaultVariableTransient;

	/* Variable default Save Game */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量保存到游戏", ToolTip = "变量默认标记为可保存到游戏存档"))
	bool bDefaultVariableSaveGame;

	/* Variable default Advanced Display */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量高级显示", ToolTip = "变量默认放入高级显示区域"))
	bool bDefaultVariableAdvancedDisplay;

	/* Variable default Config Variable */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认配置变量", ToolTip = "变量默认从配置文件读取"))
	bool bDefaultConfigVariable;

	/* Variable default Name */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量名称", ToolTip = "变量默认名称"))
	FString DefaultVariableName;

	/* Variable default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量提示", ToolTip = "变量默认提示文本"))
	FText DefaultVariableTooltip;

	/* Variable default Category */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "默认变量分类", ToolTip = "变量默认分类"))
	FText DefaultVariableCategory;

	/* Should these defaults apply to event dispatchers? */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "应用变量默认值到事件调度器", ToolTip = "将变量默认值应用到事件调度器"))
	bool bApplyVariableDefaultsToEventDispatchers;
};

USTRUCT()
struct FBAFunctionDefaults
{
	GENERATED_BODY()

	FBAFunctionDefaults();

	/* Function default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数访问说明符", ToolTip = "函数默认访问说明符"))
	EBAFunctionAccessSpecifier DefaultFunctionAccessSpecifier;

	/* Function default Pure */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数纯函数", ToolTip = "函数默认为纯函数"))
	bool bDefaultFunctionPure;

	/* Function default Const */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数常量", ToolTip = "函数默认为常量"))
	bool bDefaultFunctionConst;

	/* Function default Exec */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数可执行", ToolTip = "函数默认可执行"))
	bool bDefaultFunctionExec;

	/* Function default ThreadSafe */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数线程安全", ToolTip = "函数默认标记为线程安全"))
	bool bDefaultFunctionThreadSafe;

	/* Function default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数提示", ToolTip = "函数默认提示文本"))
	FText DefaultFunctionTooltip;

	/* Function default Keywords */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数关键字", ToolTip = "函数默认关键字"))
	FText DefaultFunctionKeywords;

	/* Function default Category */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "默认函数分类", ToolTip = "函数默认分类"))
	FText DefaultFunctionCategory;
};

USTRUCT()
struct FBACustomEventDefaults
{
	GENERATED_BODY()

	FBACustomEventDefaults();

	/* Event default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (DisplayName = "默认事件访问说明符", ToolTip = "事件默认访问说明符"))
	EBAFunctionAccessSpecifier DefaultEventAccessSpecifier;

	/* Event default Net Reliable (for RPC calls) */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (DisplayName = "默认事件网络可靠", ToolTip = "事件默认网络可靠(用于RPC调用)"))
	bool bDefaultEventNetReliable;
};

UENUM(meta = (ToolTip = "字符串附加位置"))
enum class EBAAffixType : uint8
{
	Prefix UMETA(DisplayName = "前缀", ToolTip = "添加到名称开头"),
	Suffix UMETA(DisplayName = "后缀", ToolTip = "添加到名称结尾"),
};

USTRUCT()
struct FBAStringAffix
{
	GENERATED_BODY()

	FBAStringAffix() = default;
	FBAStringAffix(const FString& Affix) : AffixValue(Affix) {}

	UPROPERTY(EditAnywhere, config, Category = Default, meta = (DisplayName = "附加文本", ToolTip = "用于匹配和写入名称的前缀或后缀文本"))
	FString AffixValue;

	UPROPERTY(EditAnywhere, config, Category = Default, meta = (DisplayName = "附加位置", ToolTip = "将文本用作前缀或后缀"))
	EBAAffixType AffixType = EBAAffixType::Prefix;

	bool Matches(const FString& String) const
	{
		return (AffixType == EBAAffixType::Prefix) ? String.StartsWith(AffixValue) : String.EndsWith(AffixValue);
	}

	void AddAffix(FString& String) const
	{
		String = (AffixType == EBAAffixType::Prefix) ? (AffixValue + String) : (String + AffixValue);
	}

	bool RemoveAffix(FString& String) const
	{
		return (AffixType == EBAAffixType::Prefix) ? String.RemoveFromStart(AffixValue) : String.RemoveFromEnd(AffixValue);
	}
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

	/* Set the according replication flags after renaming a custom event by matching the affixes below */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "重命名后设置复制标志", ToolTip = "重命名自定义事件后通过匹配以下前缀设置相应的复制标志"))
	bool bSetReplicationFlagsAfterRenaming;

	/* If there is no matching affix in the title, apply "NotReplicated" */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (EditCondition = "bSetReplicationFlagsAfterRenaming", DisplayName = "无匹配时清除复制标志", ToolTip = "事件名称没有匹配附加文本时，将复制方式设为不复制"))
	bool bClearReplicationFlagsWhenRenaming;

	/* Add the according affix to the title after changing replication flags */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "复制设置变化时更新名称", ToolTip = "修改复制标志后，将对应附加文本写入自定义事件名称"))
	bool bAddReplicationAffixToCustomEventTitle;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "多播事件附加文本", ToolTip = "用于识别多播事件的前缀或后缀"))
	FBAStringAffix MulticastAffix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "服务端事件附加文本", ToolTip = "用于识别服务端事件的前缀或后缀"))
	FBAStringAffix ServerAffix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta = (DisplayName = "客户端事件附加文本", ToolTip = "用于识别客户端事件的前缀或后缀"))
	FBAStringAffix ClientAffix;

	////////////////////////////////////////////////////////////
	/// Node group
	////////////////////////////////////////////////////////////

	/* Draw an outline to visualise each node group on the graph */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta = (DisplayName = "绘制节点组轮廓", ToolTip = "在图表上绘制轮廓以可视化每个节点组"))
	bool bDrawNodeGroupOutline;

	/* Only draw the group outline when selected */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "仅选中时绘制轮廓", ToolTip = "只在选中时绘制节点组轮廓"))
	bool bOnlyDrawGroupOutlineWhenSelected;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓颜色", ToolTip = "节点组轮廓的颜色"))
	FLinearColor NodeGroupOutlineColor;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓宽度", ToolTip = "节点组轮廓线的宽度"))
	float NodeGroupOutlineWidth;

	/* Outline margin around each node */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides, DisplayName = "节点组轮廓边距", ToolTip = "每个节点周围的轮廓边距"))
	FMargin NodeGroupOutlineMargin;

	/* Draw a fill to show the node groups for selected nodes */
	UPROPERTY(EditAnywhere, Category = NodeGroup, meta = (DisplayName = "绘制节点组填充", ToolTip = "为选中的节点绘制填充以显示节点组"))
	bool bDrawNodeGroupFill;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupFill", EditConditionHides, DisplayName = "节点组填充颜色", ToolTip = "节点组填充的颜色"))
	FLinearColor NodeGroupFillColor;

	////////////////////////////////////////////////////////////
	/// Graph
	////////////////////////////////////////////////////////////

	/* Distance the viewport moves when running the Shift Camera command. Scaled by zoom distance. */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "移动相机距离", ToolTip = "运行移动相机命令时视口移动的距离,根据缩放距离缩放"))
	int ShiftCameraDistance;

	/* Automatically add parent nodes to event nodes */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "自动添加父节点", ToolTip = "自动将父节点添加到事件节点"))
	bool bAutoAddParentNode;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "选中引脚高亮颜色", ToolTip = "选中引脚周围边框的颜色"))
	FLinearColor SelectedPinHighlightColor;

	/* Determines which pin should be selected when you click on an execution node */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "执行节点引脚选择方式", ToolTip = "确定单击执行节点时应选中哪个引脚"))
	EBAPinSelectionMethod PinSelectionMethod_Execution;

	/* Determines which pin should be selected when you click on a parameter node */
	UPROPERTY(EditAnywhere, config, Category = "Graph", meta = (DisplayName = "参数节点引脚选择方式", ToolTip = "确定单击参数节点时应选中哪个引脚"))
	EBAPinSelectionMethod PinSelectionMethod_Parameter;

	/* Sets the 'Comment Bubble Pinned' bool for all nodes on the graph (Auto Size Comment plugin handles this value for comments) */
	UPROPERTY(EditAnywhere, config, Category = "Graph|Comments", meta = (DisplayName = "启用全局注释气泡固定", ToolTip = "为图表上的所有节点设置'注释气泡固定'布尔值"))
	bool bEnableGlobalCommentBubblePinned;

	/* The global 'Comment Bubble Pinned' value */
	UPROPERTY(EditAnywhere, config, Category = "Graph|Comments", meta = (EditCondition = "bEnableGlobalCommentBubblePinned", DisplayName = "全局注释气泡固定值", ToolTip = "全局'注释气泡固定'的值"))
	bool bGlobalCommentBubblePinnedValue;

	/* Determines if we should auto zoom to a newly created node */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "自动缩放到节点行为", ToolTip = "确定是否应自动缩放到新创建的节点"))
	EBAAutoZoomToNode AutoZoomToNodeBehavior = EBAAutoZoomToNode::Outside_Viewport;

	/* Try to insert the node between any current wires when holding down this key */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "插入新节点快捷键", ToolTip = "按住此键时尝试将节点插入到任何当前连线之间"))
	FInputChord InsertNewNodeKeyChord;

	/* When creating a new node from a parameter pin, always try to connect the execution. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "从参数总是连接执行", ToolTip = "从参数引脚创建新节点时总是尝试连接执行,按住插入键将禁用此功能"))
	bool bAlwaysConnectExecutionFromParameter;

	/* When creating a new node from a parameter pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "从参数总是插入", ToolTip = "从参数引脚创建新节点时总是尝试插入到连线之间,按住插入键将禁用此功能"))
	bool bAlwaysInsertFromParameter;

	/* When creating a new node from an execution pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "从执行总是插入", ToolTip = "从执行引脚创建新节点时总是尝试插入到连线之间,按住插入键将禁用此功能"))
	bool bAlwaysInsertFromExecution;

	/* Select the first editable parameter pin when a node is created */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour", meta = (DisplayName = "创建新节点时选中值引脚", ToolTip = "创建节点时选中第一个可编辑的参数引脚"))
	bool bSelectValuePinWhenCreatingNewNodes;

	////////////////////////////////////////////////////////////
	/// General
	////////////////////////////////////////////////////////////

	/* Add the BlueprintAssist widget to the toolbar */
	UPROPERTY(EditAnywhere, config, Category = "General", meta = (DisplayName = "添加工具栏控件", ToolTip = "将BlueprintAssist控件添加到工具栏"))
	bool bAddToolbarWidget;

	/* Automatically rename Function getters and setters when the Function is renamed */
	UPROPERTY(EditAnywhere, config, Category = "General|Getters and Setters", meta = (DisplayName = "自动重命名获取器和设置器", ToolTip = "重命名函数时自动重命名函数的获取器和设置器"))
	bool bAutoRenameGettersAndSetters;

	/* Merge the generate getter and setter into one button */
	UPROPERTY(EditAnywhere, config, Category = "General|Getters and Setters", meta = (DisplayName = "合并生成获取器和设置器按钮", ToolTip = "将生成获取器和设置器合并为一个按钮"))
	bool bMergeGenerateGetterAndSetterButton;

	////////////////////////////////////////////////////////////
	// Create Variable defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on variables when they are created */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "启用变量默认值", ToolTip = "创建变量时设置默认属性"))
	bool bEnableVariableDefaults;

	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults", DisplayName = "变量默认值", ToolTip = "创建变量时应用的默认属性"))
	FBAVariableDefaults VariableDefaults;

	/* When holding down this key while you create a new variable, these defaults will apply */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (DisplayName = "变量默认值快捷键", ToolTip = "创建变量时按住指定按键，应用对应的默认属性"))
	TMap<FKey, FBAVariableDefaults> VariableDefaultsHotkeys;

	////////////////////////////////////////////////////////////
	// Function defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on functions when they are created */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "启用函数默认值", ToolTip = "创建函数时设置默认属性"))
	bool bEnableFunctionDefaults;

	/* Function defaults */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults", DisplayName = "函数默认值", ToolTip = "创建函数时应用的默认属性"))
	FBAFunctionDefaults FunctionDefaults;

	/* When holding down this key while you create a new function, these defaults will apply */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (DisplayName = "函数默认值快捷键", ToolTip = "创建函数时按住指定按键，应用对应的默认属性"))
	TMap<FKey, FBAFunctionDefaults> FunctionDefaultsHotkeys;

	////////////////////////////////////////////////////////////
	// Custom event defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on custom events when they are created */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (DisplayName = "启用自定义事件默认值", ToolTip = "创建自定义事件时设置默认属性"))
	bool bEnableCustomEventDefaults;

	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (EditCondition = "bEnableCustomEventDefaults", DisplayName = "自定义事件默认值", ToolTip = "创建自定义事件时应用的默认属性"))
	FBACustomEventDefaults CustomEventDefaults;

	/* When holding down this key while you create a new custom event, these defaults will apply */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (DisplayName = "自定义事件默认值快捷键", ToolTip = "创建自定义事件时按住指定按键，应用对应的默认属性"))
	TMap<FKey, FBACustomEventDefaults> CustomEventDefaultsHotkeys;

	////////////////////////////////////////////////////////////
	// Inputs
	////////////////////////////////////////////////////////////

	/* Copy the pin value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "复制引脚值快捷键", ToolTip = "将引脚值复制到剪贴板"))
	FInputChord CopyPinValueChord;

	/* Paste the hovered value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "粘贴引脚值快捷键", ToolTip = "从剪贴板粘贴值到悬停的引脚"))
	FInputChord PastePinValueChord;

	/* Focus the hovered node in the details panel */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "在详情面板聚焦快捷键", ToolTip = "在详情面板中聚焦悬停的节点"))
	FInputChord FocusInDetailsPanelChord;

	/* Used smart wire operation, if this key is held then pin conversions are allowed */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "允许转换连接修饰键", ToolTip = "智能连线时按住此键，允许创建带类型转换的连接"))
	FKey AllowConversionConnectionsModifier;

	/* Enter key moves focus between pin value widgets */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "回车循环选择引脚值", ToolTip = "按回车键在引脚值控件之间移动焦点"))
	bool bEnterCyclesPinValues;

	/* Enter key tries to interact with the selected or hovered pin widget */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "回车操作引脚", ToolTip = "按回车键操作当前选中或悬停的引脚控件"))
	bool bEnterInteractsWithPin;

	/* Tab key moves focus between pin value widgets */
	UPROPERTY(EditAnywhere, config, Category = "Inputs", meta = (DisplayName = "Tab循环选择引脚值", ToolTip = "按 Tab 键在引脚值控件之间移动焦点"))
	bool bTabCyclePinValues;

	/** Extra input chords to for dragging selected nodes with cursor (same as left-click-dragging) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "额外拖动节点快捷键", ToolTip = "用光标拖动选中节点的额外输入组合键(与左键拖动相同)"))
	TArray<FInputChord> AdditionalDragNodesChords;

	/** Whether AdditionalDragNodesChords require you to have your mouse over a node */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "拖动时要求光标位于节点上", ToolTip = "使用额外拖动快捷键时，要求鼠标光标位于节点上方"))
	bool bDragRequiresNodeUnderCursor;

	/** Input chords for group dragging (move all linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "组移动快捷键", ToolTip = "组拖动的输入组合键(移动所有链接的节点)"))
	TArray<FInputChord> GroupMovementChords;

	/** Input chords for group dragging (move left linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "左子树移动快捷键", ToolTip = "组拖动的输入组合键(移动左侧链接的节点)"))
	TArray<FInputChord> LeftSubTreeMovementChords;

	/** Input chords for group dragging (move right linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features", meta = (DisplayName = "右子树移动快捷键", ToolTip = "组拖动的输入组合键(移动右侧链接的节点)"))
	TArray<FInputChord> RightSubTreeMovementChords;


	////////////////////////////////////////////////////////////
	// Misc
	////////////////////////////////////////////////////////////

	/* By default the Blueprint Assist Hotkey Menu only displays this plugin's hotkeys. Enable this to display all hotkeys for the editor. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "显示所有快捷键", ToolTip = "默认情况下快捷键菜单仅显示此插件的快捷键,启用此选项可显示编辑器的所有快捷键"))
	bool bDisplayAllHotkeys;

	/* Show the welcome screen when the engine launches */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "启动时显示欢迎屏幕", ToolTip = "引擎启动时显示欢迎屏幕"))
	bool bShowWelcomeScreenOnLaunch;

	/* Double click on a node to go to definition. Currently only implemented for Cast blueprint node. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "启用双击跳转到定义", ToolTip = "双击节点跳转到定义,目前仅对Cast蓝图节点实现"))
	bool bEnableDoubleClickGoToDefinition;

	/* Knot nodes will be hidden (requires graphs to be re-opened) */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "启用隐形结点节点", ToolTip = "结点节点将被隐藏(需要重新打开图表)"))
	bool bEnableInvisibleKnotNodes;

	/* Play sound on successful compile */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "编译成功时播放声音", ToolTip = "编译成功时播放声音"))
	bool bPlayLiveCompileSound;

	/** Input for folder bookmarks */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "文件夹书签", ToolTip = "文件夹书签的输入键"))
	TArray<FKey> FolderBookmarks;

	/** Duration to differentiate between a click and a drag */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "点击时间", ToolTip = "区分点击和拖动的持续时间"))
	float ClickTime;

	/* What category to assign to generated getter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "默认生成的获取器分类", ToolTip = "分配给生成的获取器函数的分类,覆盖DefaultFunctionCategory"))
	FText DefaultGeneratedGettersCategory;

	/* What category to assign to generated setter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "默认生成的设置器分类", ToolTip = "分配给生成的设置器函数的分类,覆盖DefaultFunctionCategory"))
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
