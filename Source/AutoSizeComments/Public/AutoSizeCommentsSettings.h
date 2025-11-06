// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Text/TextLayout.h"
#include "Layout/Margin.h"
#include "AutoSizeCommentsSettings.generated.h"

UENUM()
enum class EASCCacheSaveMethod : uint8
{
	/** Save the cache to an external json file */
	File UMETA(DisplayName = "File"),

	/** Save to cache in the package's meta data (the .uasset) */
	MetaData UMETA(DisplayName = "Package Meta Data"),
};

UENUM()
enum class EASCCacheSaveLocation : uint8
{
	/** Save to PluginFolder/ASCCache/PROJECT_ID.json */
	Plugin UMETA(DisplayName = "Plugin"),

	/** Save to ProjectFolder/Saved/AutoSizeComments/AutoSizeCommentsCache.json */
	Project UMETA(DisplayName = "Project"),
};

UENUM()
enum class EASCResizingMode : uint8
{
	/** Resize to containing nodes on tick */
	Always UMETA(DisplayName = "Always"),

	/** Resize when we detect a containing node moves or changes size */
	Reactive UMETA(DisplayName = "Reactive"),

	/** Never resize */
	Disabled UMETA(DisplayName = "Disabled"),
};

UENUM()
enum class ECommentCollisionMethod : uint8
{
	/** Add the node if the top-left corner is inside the comment */
	Point UMETA(DisplayName = "Point"),

	/** Add the node if it is intersecting the comment */
	Intersect UMETA(DisplayName = "Overlap"),

	/** Add the node if it is fully contained in the comment */
	Contained UMETA(DisplayName = "Contained"),
	Disabled UMETA(DisplayName = "Disabled"),
};

UENUM()
enum class EASCAutoInsertComment : uint8
{
	/** Never insert new nodes into comments */
	Never UMETA(DisplayName = "Never"),

	/** Insert new nodes when a node is created from a pin */
	Always UMETA(DisplayName = "Always"),

	/** Insert new nodes when a node is created from a pin (and is surrounded by multiple nodes inside the comment) */
	Surrounded UMETA(DisplayName = "Surrounded"),
};

UENUM()
enum class EASCDefaultCommentColorMethod : uint8
{
	/** Do not change the color */
	None UMETA(DisplayName = "None"),

	/** Use a random color when spawning the comment */
	Random UMETA(DisplayName = "Random"),

	/** Apply the default color defined in the settings here */
	Default UMETA(DisplayName = "Default"),
};

USTRUCT()
struct FPresetCommentStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Default)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, config, Category = Default)
	int FontSize = 18;

	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bSetHeader = false;
};

USTRUCT()
struct FASCGraphSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Default)
	EASCResizingMode ResizingMode = EASCResizingMode::Always;
};

UCLASS(config = EditorPerProjectUserSettings)
class AUTOSIZECOMMENTS_API UAutoSizeCommentsSettings final : public UObject
{
	GENERATED_BODY()

public:
	UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer);

	/** The default font size for comment boxes */
	UPROPERTY(EditAnywhere, config, Category = UI, meta = (DisplayName = "默认字体大小", Tooltip = "注释框的默认字体大小"))
	int DefaultFontSize;

	/** If enabled, all nodes will be changed to the default font size (unless they are a preset or floating node) */
	UPROPERTY(EditAnywhere, config, Category = UI, meta = (DisplayName = "使用默认字体大小", Tooltip = "启用后，所有注释框将使用默认字体大小（预设样式和浮动节点除外）"))
	bool bUseDefaultFontSize;

	/** How to color the comment when creating the node */
	UPROPERTY(EditAnywhere, Config, Category = Color, meta = (DisplayName = "默认注释颜色方法", Tooltip = "创建注释框时如何着色"))
	EASCDefaultCommentColorMethod DefaultCommentColorMethod;

	/** How to color the comment when pressing the `Toggle Header` button */
	UPROPERTY(EditAnywhere, Config, Category = Color, meta = (DisplayName = "标题栏颜色方法", Tooltip = "按下"切换标题"按钮时如何着色"))
	EASCDefaultCommentColorMethod HeaderColorMethod;

	/** Comment boxes will spawn with this default color */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Default", EditConditionHides, DisplayName = "默认注释颜色", Tooltip = "注释框将使用此默认颜色生成"))
	FLinearColor DefaultCommentColor;

	/** Set all comments on the graph to the default color */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Default", EditConditionHides, DisplayName = "强制使用默认颜色", Tooltip = "将图表上的所有注释设置为默认颜色"))
	bool bAggressivelyUseDefaultColor;

	/** Opacity used for the random color */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Random", EditConditionHides, DisplayName = "随机颜色不透明度", Tooltip = "随机颜色使用的不透明度"))
	float RandomColorOpacity;

	/** If enabled, select a random color from predefined list */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Random", EditConditionHides, DisplayName = "从列表中选择随机颜色", Tooltip = "启用后，从预定义列表中选择随机颜色"))
	bool bUseRandomColorFromList;

	/** If UseRandomColorFromList is enabled, new comments will select a color from one of these */
	UPROPERTY(EditAnywhere, config, Category = Color, meta = (EditCondition = "bUseRandomColorFromList && DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Random", EditConditionHides, DisplayName = "预定义随机颜色列表", Tooltip = "新注释将从这些颜色中随机选择一个"))
	TArray<FLinearColor> PredefinedRandomColorList;

	/** Minimum opacity for comment box controls when not hovered */
	UPROPERTY(EditAnywhere, config, Category = Color, meta = (DisplayName = "最小控件不透明度", Tooltip = "未悬停时注释框控件的最小不透明度"))
	float MinimumControlOpacity;

	/** Style for header comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Styles, meta = (DisplayName = "标题样式", Tooltip = "标题注释框的样式"))
	FPresetCommentStyle HeaderStyle;

	/** Preset styles (each style will have its own button on the comment box) */
	UPROPERTY(EditAnywhere, config, Category = Styles, meta = (DisplayName = "预设样式", Tooltip = "每个样式都会在注释框上有自己的按钮"))
	TArray<FPresetCommentStyle> PresetStyles;

	/** Preset style that will apply if the title starts with the according prefix */
	UPROPERTY(EditAnywhere, config, Category = Styles, meta = (DisplayName = "标签预设", Tooltip = "如果标题以相应前缀开头，将应用的预设样式"))
	TMap<FString, FPresetCommentStyle> TaggedPresets;

	/** The title bar uses a minimal style when being edited (requires UE5 or later) */
	UPROPERTY(EditAnywhere, config, Category = "UI", meta = (DisplayName = "使用简洁标题栏样式", Tooltip = "编辑时标题栏使用简洁样式（需要 UE5 或更高版本）"))
	bool bUseMinimalTitlebarStyle = false;

	/** Always hide the comment bubble */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble, meta = (DisplayName = "隐藏注释气泡", Tooltip = "始终隐藏注释气泡"))
	bool bHideCommentBubble;

	/** Set default values for comment bubble */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble, meta = (DisplayName = "启用注释气泡默认值", Tooltip = "为注释气泡设置默认值"))
	bool bEnableCommentBubbleDefaults;

	/** Default value for "Color Bubble" */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble, meta = (EditCondition = "bEnableCommentBubbleDefaults", DisplayName = "默认着色气泡", Tooltip = ""着色气泡"的默认值"))
	bool bDefaultColorCommentBubble;

	/** Default value for "Show Bubble When Zoomed" */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble, meta = (EditCondition = "bEnableCommentBubbleDefaults", DisplayName = "默认缩放时显示气泡", Tooltip = ""缩放时显示气泡"的默认值"))
	bool bDefaultShowBubbleWhenZoomed;

	/** The auto resizing behavior for comments (always: on tick | reactive: upon detecting node movement) */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "调整大小模式", Tooltip = "注释的自动调整大小行为（始终: 每帧 | 响应式: 检测到节点移动时）"))
	EASCResizingMode ResizingMode;

	/** Should the comment resize to fit after running user commands in disabled mode */
    UPROPERTY(EditAnywhere, config, Category = Misc, meta = (EditCondition = "ResizingMode == EASCResizingMode::Disabled", EditConditionHides, DisplayName = "禁用模式下调整大小", Tooltip = "禁用模式下运行用户命令后是否调整注释大小"))
    bool ResizeToFitWhenDisabled;

	/** In reactive mode, run a 2nd resize so that the title is correctly calculated */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (EditCondition = "ResizingMode == EASCResizingMode::Reactive", DisplayName = "使用两遍调整", Tooltip = "在响应式模式下，运行第二次调整以正确计算标题"))
	bool bUseTwoPassResize;

	/** Determines when to insert newly created nodes into existing comments */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "自动插入注释", Tooltip = "确定何时将新创建的节点插入现有注释"))
	EASCAutoInsertComment AutoInsertComment;

	/** When a user places a comment, give keyboard focus to the title bar so it can be easily renamed */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "自动重命名新注释", Tooltip = "放置注释时，将键盘焦点设置到标题栏以便轻松重命名"))
	bool bAutoRenameNewComments;

	/** When you click a node's pin, also select the node (required for AutoInsertComment to function correctly) */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "点击引脚时选择节点", Tooltip = "点击节点的引脚时也选择该节点（自动插入注释功能需要此选项）"))
	bool bSelectNodeWhenClickingOnPin;

	/** Amount of padding for around the contents of a comment node */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "注释节点内边距", Tooltip = "注释节点内容周围的内边距"))
	FVector2D CommentNodePadding;

	/** Amount of padding around the comment title text */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "注释文本内边距", Tooltip = "注释标题文本周围的内边距"))
	FMargin CommentTextPadding;

	/** Minimum vertical padding above and below the node */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay, meta = (DisplayName = "最小垂直内边距", Tooltip = "节点上下的最小垂直内边距"))
	float MinimumVerticalPadding;

	/** Comment text alignment */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "注释文本对齐", Tooltip = "注释文本的对齐方式"))
	TEnumAsByte<ETextJustify::Type> CommentTextAlignment;

	/** If enabled, add any containing node's comment bubble to the comment bounds */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "使用注释气泡边界", Tooltip = "启用后，将包含节点的注释气泡添加到注释边界"))
	bool bUseCommentBubbleBounds;

	/** If enabled, empty comment boxes will move out of the way of other comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "移动空注释框", Tooltip = "启用后，空注释框会自动避让其他注释框"))
	bool bMoveEmptyCommentBoxes;

	/** The speed at which empty comment boxes move */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "空注释框移动速度", Tooltip = "空注释框移动的速度"))
	float EmptyCommentBoxSpeed;

	/** Choose cache save method: as an external file or inside the package's metadata */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, meta = (DisplayName = "缓存保存方法", Tooltip = "选择缓存保存方法：作为外部文件或包的元数据"))
	EASCCacheSaveMethod CacheSaveMethod;

	/** Choose where to save the json file: project or plugin folder */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, meta = (EditCondition = "CacheSaveMethod == EASCCacheSaveMethod::File", EditConditionHides, DisplayName = "缓存保存位置", Tooltip = "选择保存json文件的位置：项目或插件文件夹"))
	EASCCacheSaveLocation CacheSaveLocation;

	/** If enabled, nodes will be saved to file when the graph is saved */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, meta = (DisplayName = "保存图表时保存注释数据", Tooltip = "启用后，保存图表时会将节点保存到文件"))
	bool bSaveCommentDataOnSavingGraph;

	/** If enabled, nodes will be saved to file when the program is exited */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, meta = (DisplayName = "退出时保存注释数据", Tooltip = "启用后，程序退出时会将节点保存到文件"))
	bool bSaveCommentDataOnExit;

	/** If enabled, cache file JSON text will be made more human-readable, but increases file size */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, AdvancedDisplay, meta = (DisplayName = "美化JSON输出", Tooltip = "启用后，缓存文件的JSON文本将更易读，但会增加文件大小"))
	bool bPrettyPrintCommentCacheJSON;

	/** When opening a new graph, existing comments will apply default color settings (suggest disabled) */
	UPROPERTY(EditAnywhere, config, Category = Initialization, meta = (DisplayName = "对现有节点应用颜色", Tooltip = "打开新图表时，现有注释将应用默认颜色设置（建议禁用）"))
	bool bApplyColorToExistingNodes;

	/** When opening a new graph, existing comments will try to fit to their overlapping nodes (suggest disabled) */
	UPROPERTY(EditAnywhere, config, Category = Initialization, meta = (DisplayName = "调整现有节点大小", Tooltip = "打开新图表时，现有注释将尝试适配其重叠的节点（建议禁用）"))
	bool bResizeExistingNodes;

	/** Commments will detect and add nodes are underneath on creation */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "检测新注释包含的节点", Tooltip = "创建注释时将检测并添加下方的节点"))
	bool bDetectNodesContainedForNewComments;

	/** Mouse input chord to resize a node */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "调整大小快捷键", Tooltip = "调整节点大小的鼠标输入组合键"))
	FInputChord ResizeChord;

	/** Input key to enable comment controls */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "启用注释控件键", Tooltip = "启用注释控件的输入键"))
	FInputChord EnableCommentControlsKey;

	/** Collision method to use when resizing comment nodes */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "调整大小碰撞方法", Tooltip = "调整注释节点大小时使用的碰撞方法"))
	ECommentCollisionMethod ResizeCollisionMethod;

	/** Collision method to use when releasing alt */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "备用碰撞方法", Tooltip = "释放Alt键时使用的碰撞方法"))
	ECommentCollisionMethod AltCollisionMethod;

	/** Snap to the grid when resizing the node */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "调整大小时吸附网格", Tooltip = "调整节点大小时吸附到网格"))
	bool bSnapToGridWhileResizing;

	/** Don't add knot nodes to comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "忽略连接线节点", Tooltip = "不将连接线节点添加到注释框"))
	bool bIgnoreKnotNodes;

	/** Don't add knot nodes to comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "按Alt时忽略连接线节点", Tooltip = "按住Alt时不将连接线节点添加到注释框"))
	bool bIgnoreKnotNodesWhenPressingAlt;

	/** Don't add knot nodes to comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "调整大小时忽略连接线节点", Tooltip = "调整大小时不将连接线节点添加到注释框"))
	bool bIgnoreKnotNodesWhenResizing;

	/** Don't snap to selected nodes on creation */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "创建时忽略选中节点", Tooltip = "创建时不吸附到选中的节点"))
	bool bIgnoreSelectedNodesOnCreation;

	/** Refresh the nodes inside the comment when you start moving the comment */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "移动时刷新包含节点", Tooltip = "开始移动注释时刷新其中的节点"))
	bool bRefreshContainingNodesOnMove;

	/** Disable the tooltip when hovering the titlebar */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "禁用工具提示", Tooltip = "悬停标题栏时禁用工具提示"))
	bool bDisableTooltip;

	/** Highlight the contained node for a comment when you select it */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "选择时高亮包含节点", Tooltip = "选择注释时高亮显示包含的节点"))
	bool bHighlightContainingNodesOnSelection;

	/** Force the graph panel to use the 1:1 LOD for nodes (UE 5.0+) */
	UPROPERTY(EditAnywhere, config, Category = Misc, meta = (DisplayName = "使用最大细节节点", Tooltip = "强制图表面板对节点使用1:1 LOD（UE 5.0+）"))
	bool bUseMaxDetailNodes;

	/** Do not use ASC node for these graphs, turn on DebugClass_ASC and open graph to find graph class name */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay, meta = (DisplayName = "忽略的图表", Tooltip = "这些图表不使用ASC节点，打开DebugClass_ASC并打开图表以查找图表类名"))
	TArray<FString> IgnoredGraphs;

	/** Override settings (resizing mode) for these graph types */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay, meta=(ForceInlineRow, DisplayName = "图表设置覆盖", Tooltip = "为这些图表类型覆盖设置（调整大小模式）"))
	TMap<FName, FASCGraphSettings> GraphSettingsOverride;

	/** Hide prompt for suggested settings with Blueprint Assist plugin */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay, meta = (DisplayName = "抑制建议设置提示", Tooltip = "隐藏Blueprint Assist插件的建议设置提示"))
	bool bSuppressSuggestedSettings;

	/** Hide prompt for suggested settings when source control is enabled*/
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay, meta = (DisplayName = "抑制源码管理通知", Tooltip = "启用源码管理时隐藏建议设置提示"))
	bool bSuppressSourceControlNotification;

	/** Size of the corner resizing anchors */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "角落锚点大小", Tooltip = "角落调整大小锚点的大小"))
	float ResizeCornerAnchorSize;

	/** Padding to activate resizing on the side of a comment */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "侧边内边距", Tooltip = "激活注释侧边调整大小的内边距"))
	float ResizeSidePadding;

	/** Hide the resize button */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "隐藏调整大小按钮", Tooltip = "隐藏调整大小按钮"))
	bool bHideResizeButton;

	/** Hide the header button */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "隐藏标题按钮", Tooltip = "隐藏标题按钮"))
	bool bHideHeaderButton;

	/** Hide the preset buttons */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "隐藏预设按钮", Tooltip = "隐藏预设按钮"))
	bool bHidePresets;

	/** Hide the randomize color button */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "隐藏随机颜色按钮", Tooltip = "隐藏随机颜色按钮"))
	bool bHideRandomizeButton;

	/** Hide controls at the bottom of the comment box */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "隐藏注释框控件", Tooltip = "隐藏注释框底部的控件"))
	bool bHideCommentBoxControls;

	/** Hide the corner points (resize still enabled) */
	UPROPERTY(EditAnywhere, config, Category = Controls, meta = (DisplayName = "隐藏角点", Tooltip = "隐藏角点（调整大小仍然启用）"))
	bool bHideCornerPoints;

	/** Experimental fix for sort depth issue in UE5 (unable to move nested nodes until you compile the blueprint) */
	UPROPERTY(EditAnywhere, config, Category = Experimental, meta = (DisplayName = "修复排序深度问题", Tooltip = "UE5排序深度问题的实验性修复（在编译蓝图之前无法移动嵌套节点）"))
	bool bEnableFixForSortDepthIssue;

	/** Print info about the graph when opening a graph */
	UPROPERTY(EditAnywhere, config, Category = Debug, meta = (DisplayName = "调试图表", Tooltip = "打开图表时打印图表信息"))
	bool bDebugGraph_ASC;

	UPROPERTY(EditAnywhere, config, Category = Debug, meta = (DisplayName = "禁用包清理", Tooltip = "禁用包清理"))
	bool bDisablePackageCleanup;

	/** Use the default Unreal comment node */
	UPROPERTY(EditAnywhere, config, Category = Debug, meta = (DisplayName = "禁用ASC图表节点", Tooltip = "使用默认的Unreal注释节点"))
	bool bDisableASCGraphNode;

	static FORCEINLINE const UAutoSizeCommentsSettings& Get()
	{
		return *GetDefault<UAutoSizeCommentsSettings>();
	}

	static FORCEINLINE UAutoSizeCommentsSettings& GetMutable()
	{
		return *GetMutableDefault<UAutoSizeCommentsSettings>();
	}

	bool ShouldResizeToFit() const
	{
		return ResizingMode != EASCResizingMode::Disabled || ResizeToFitWhenDisabled;
	}

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};

class FASCSettingsDetails final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

