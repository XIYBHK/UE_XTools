// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsSettings.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsMacros.h"
#include "AutoSizeCommentsState.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"

UAutoSizeCommentsSettings::UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	ResizingMode = EASCResizingMode::Reactive;
	ResizeToFitWhenDisabled = false;
	bUseTwoPassResize = true;
	AutoInsertComment = EASCAutoInsertComment::Always;
	bSelectNodeWhenClickingOnPin = true;
	bAutoRenameNewComments = true;
	CommentNodePadding = FVector2D(30, 30);
	MinimumVerticalPadding = 24.0f;
	CommentTextPadding = FMargin(2, 0, 2, 0);
	CommentTextAlignment = ETextJustify::Left;
	DefaultFontSize = 18;
	bUseDefaultFontSize = false;
	DefaultCommentColorMethod = EASCDefaultCommentColorMethod::Random;  // 使用随机颜色
	HeaderColorMethod = EASCDefaultCommentColorMethod::Default;
	RandomColorOpacity = 1.f;
	bUseRandomColorFromList = true;  // 从预定义列表中选择随机颜色
	
	// 自定义随机颜色列表（来自用户配置）
	PredefinedRandomColorList.Add(FLinearColor(0.955973f, 0.116971f, 0.122139f, 1.0f));  // 红色
	PredefinedRandomColorList.Add(FLinearColor(1.0f, 0.346704f, 0.423268f, 1.0f));       // 粉红色
	PredefinedRandomColorList.Add(FLinearColor(0.879622f, 0.467784f, 0.212231f, 1.0f));  // 橙色
	PredefinedRandomColorList.Add(FLinearColor(1.0f, 0.938686f, 0.283149f, 1.0f));       // 黄色
	PredefinedRandomColorList.Add(FLinearColor(0.428690f, 1.0f, 0.407240f, 1.0f));       // 绿色
	PredefinedRandomColorList.Add(FLinearColor(0.254152f, 0.545724f, 1.0f, 1.0f));       // 蓝色
	PredefinedRandomColorList.Add(FLinearColor(0.332452f, 0.278894f, 0.991102f, 1.0f));  // 深蓝色
	PredefinedRandomColorList.Add(FLinearColor(0.686685f, 0.278894f, 1.0f, 1.0f));       // 紫色
	
	MinimumControlOpacity = 0.f;
	DefaultCommentColor = FLinearColor::White;
	HeaderStyle.Color = FLinearColor::Gray;

	// define tagged preset
	{
		FPresetCommentStyle TodoPreset;
		TodoPreset.Color = FColor(0, 255, 255);
		TaggedPresets.Add("@TODO", TodoPreset);

		FPresetCommentStyle FixmePreset;
		FixmePreset.Color = FColor::Red;
		TaggedPresets.Add("@FIXME", FixmePreset);

		FPresetCommentStyle InfoPreset;
		InfoPreset.Color = FColor::White;
		InfoPreset.bSetHeader = true;
		TaggedPresets.Add("@INFO", InfoPreset);
	}
	bAggressivelyUseDefaultColor = false;
	bUseCommentBubbleBounds = true;
	bMoveEmptyCommentBoxes = false;
	EmptyCommentBoxSpeed = 10;
	bHideCommentBubble = false;
	bEnableCommentBubbleDefaults = false;
	bDefaultColorCommentBubble = false;
	bDefaultShowBubbleWhenZoomed = true;
	CacheSaveMethod = EASCCacheSaveMethod::MetaData;
	CacheSaveLocation = EASCCacheSaveLocation::Plugin;  // 保存到插件目录
	bSaveCommentDataOnSavingGraph = true;
	bSaveCommentDataOnExit = true;  // 退出时保存数据
	bPrettyPrintCommentCacheJSON = false;
	bApplyColorToExistingNodes = false;
	bResizeExistingNodes = false;
	bDetectNodesContainedForNewComments = true;
	ResizeChord = FInputChord(EKeys::LeftMouseButton, EModifierKey::Shift);
	ResizeCollisionMethod = ECommentCollisionMethod::Contained;
	EnableCommentControlsKey = FInputChord();
	AltCollisionMethod = ECommentCollisionMethod::Intersect;
	ResizeCornerAnchorSize = 40.0f;
	ResizeSidePadding = 20.0f;
	bSnapToGridWhileResizing = false;
	bIgnoreKnotNodes = false;
	bIgnoreKnotNodesWhenPressingAlt = false;
	bIgnoreKnotNodesWhenResizing = false;
	bIgnoreSelectedNodesOnCreation = false;
	bRefreshContainingNodesOnMove = false;
	bDisableTooltip = false;  // 启用工具提示
	bHighlightContainingNodesOnSelection = true;
	bUseMaxDetailNodes = false;
	IgnoredGraphs.Add("ControlRigGraph");
	bSuppressSuggestedSettings = false;
	bSuppressSourceControlNotification = false;
	bHideResizeButton = false;
	bHideHeaderButton = false;
	bHideCommentBoxControls = false;
	bHidePresets = false;
	bHideRandomizeButton = false;
	bHideCornerPoints = false;

	bEnableFixForSortDepthIssue = false;

	bDebugGraph_ASC = false;
	bDisablePackageCleanup = false;
	bDisableASCGraphNode = false;
	
	// New settings
	bEnablePlugin = true;
}

void UAutoSizeCommentsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAutoSizeCommentsSettings, bHighlightContainingNodesOnSelection))
	{
		if (!bHighlightContainingNodesOnSelection)
		{
			FAutoSizeCommentGraphHandler::Get().ClearUnrelatedNodes();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TSharedRef<IDetailCustomization> FASCSettingsDetails::MakeInstance()
{
	return MakeShareable(new FASCSettingsDetails);
}
void FASCSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory("CommentCache");
	auto& SizeCache = FAutoSizeCommentsCacheFile::Get();

	const FString CachePath = SizeCache.GetCachePath(true);

	const auto DeleteSizeCache = [&SizeCache]()
	{
		static FText Title = INVTEXT("清除注释缓存");
		static FText Message = INVTEXT("确定要删除注释缓存吗？");

#if ASC_UE_VERSION_OR_LATER(5, 3)
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);
#else
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title);
#endif
		if (Result == EAppReturnType::Yes)
		{
			SizeCache.DeleteCache();
		}

#if 0 // Testing to full clear all cache data 
		FASCState::Get().CommentToASCMapping.Empty();
		FAutoSizeCommentGraphHandler::Get().ClearGraphData();
#endif

		return FReply::Handled();
	};

	GeneralCategory.AddCustomRow(INVTEXT("清除注释缓存"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(INVTEXT("清除注释缓存"))
			.Font(ASC_GET_FONT_STYLE(TEXT("PropertyWindow.NormalFont")))
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(5).AutoWidth()
			[
				SNew(SButton)
				.Text(INVTEXT("清除注释缓存"))
				.ToolTipText(FText::FromString(FString::Printf(TEXT("删除位于此处的注释缓存文件: %s"), *CachePath)))
				.OnClicked_Lambda(DeleteSizeCache)
			]
		];
	
}
