// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistSettings.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistTabHandler.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"

FBAFormatterSettings::FBAFormatterSettings()
{
	FormatterDirection = EEdGraphPinDirection::EGPD_Output;
}

EBAAutoFormatting FBAFormatterSettings::GetAutoFormatting() const
{
	return UBASettings::Get().bGloballyDisableAutoFormatting ? EBAAutoFormatting::Never : AutoFormatting; 
}

UBASettings::UBASettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UseBlueprintFormattingForTheseGraphs =
	{
		"EdGraph",
		"GameplayAbilityGraph",
		"AnimationTransitionGraph",
		"SMStateGraph",
		"SMTransitionGraph",
		"SMPropertyGraph"
	};

	ShiftCameraDistance = 400;

	CacheSaveLocation = EBACacheSaveLocation::Plugin;
	bSaveBlueprintAssistCacheToFile = true;

	bAddToolbarWidget = true;

	SelectedPinHighlightColor = FLinearColor(0.6f, 0.6f, 0.6f, 0.33);

	SupportedAssetEditors = {
		"SoundCueEditor",
		"Niagara",
		"BlueprintEditor",
		"ControlRigEditor",
		"MaterialEditor",
		"MetaSoundEditor",
		"Behavior Tree",
		"AnimationBlueprintEditor",
		"Environment Query",
		"GameplayAbilitiesEditor",
		"FSMBlueprintEditor",
		"WidgetBlueprintEditor",
		"PCGEditor",
		"FlowEditor",
		"DialogueEditor"
	};

	SupportedGraphEditors = { "SGraphEditor", "SFlowGraphEditor" };

	// ------------------- //
	// Format all settings //
	// ------------------- //

	FormatAllStyle = EBAFormatAllStyle::Simple;
	FormatAllHorizontalAlignment = EBAFormatAllHorizontalAlignment::RootNode;
	bAutoPositionEventNodes = false;
	bAlwaysFormatAll = false;
	FormatAllPadding = FIntPoint(600, 200);

	bUseFormatAllPaddingInComment = false;
	FormatAllPaddingInComment = 200;

	ExecutionWiringStyle = EBAWiringStyle::AlwaysMerge;
	ParameterWiringStyle = EBAWiringStyle::AlwaysMerge;

	// 禁用全局自动格式化（基于用户习惯）
	bGloballyDisableAutoFormatting = true;
	FormattingStyle = EBANodeFormattingStyle::Expanded;
	ParameterStyle = EBAParameterFormattingStyle::Helixing;

	BlueprintParameterPadding = FIntPoint(40, 25);
	BlueprintKnotTrackSpacing = 26;
	VerticalPinSpacing = 26;
	ParameterVerticalPinSpacing = 26;

	bDisableHelixingWithMultiplePins = true;
	DisableHelixingPinCount = 2;
	bLimitHelixingHeight = true;
	HelixingHeightMax = 500;
	SingleNodeMaxHeight = 300;

	CullKnotVerticalThreshold = 8;

	// ------------------ //
	// Formatter Settings //
	// ------------------ //

	// TODO change the default padding size to 128, 128 in the next major formatter upgrade
	// const FVector2D DefaultFormatterPaddingSize(128.0f, 128.0f);
	const FIntPoint DefaultFormatterPaddingSize(100, 100);

	BlueprintFormatterSettings.FormatterType = EBAFormatterType::Blueprint;
	BlueprintFormatterSettings.Padding = DefaultFormatterPaddingSize; 
	BlueprintFormatterSettings.AutoFormatting = EBAAutoFormatting::FormatAllConnected; 
	BlueprintFormatterSettings.FormatterDirection = EGPD_Output;
	BlueprintFormatterSettings.RootNodes = { "K2Node_Tunnel" }; 
	BlueprintFormatterSettings.ExecPinName = UEdGraphSchema_K2::PC_Exec; 
	BlueprintFormatterSettings.ExecPinName = "exec";

	FBAFormatterSettings BehaviorTreeSettings(
		DefaultFormatterPaddingSize,
		EBAAutoFormatting::FormatAllConnected,
		EGPD_Output,
		{ "BehaviorTreeGraphNode_Root" }
	);

	BehaviorTreeSettings.FormatterType = EBAFormatterType::BehaviorTree;

	NonBlueprintFormatterSettings.Add("BehaviorTreeGraph", BehaviorTreeSettings);

	FBAFormatterSettings SoundCueSettings(
		DefaultFormatterPaddingSize,
		EBAAutoFormatting::Never,
		EGPD_Input,
		{ "SoundCueGraphNode_Root" }
	);
	NonBlueprintFormatterSettings.Add("SoundCueGraph", SoundCueSettings);

	FBAFormatterSettings MaterialGraphSettings(
		DefaultFormatterPaddingSize,
		EBAAutoFormatting::Never,
		EGPD_Input,
		{ "MaterialGraphNode_Root" }
	);
	NonBlueprintFormatterSettings.Add("MaterialGraph", MaterialGraphSettings);

	FBAFormatterSettings AnimGraphSetting;
	AnimGraphSetting.Padding = DefaultFormatterPaddingSize; 
	AnimGraphSetting.AutoFormatting = EBAAutoFormatting::Never; 
	AnimGraphSetting.FormatterDirection = EGPD_Input;
	AnimGraphSetting.RootNodes = { "AnimGraphNode_Root", "AnimGraphNode_TransitionResult", "AnimGraphNode_StateResult" }; 
	AnimGraphSetting.ExecPinName = "PoseLink"; 
	NonBlueprintFormatterSettings.Add("AnimationGraph", AnimGraphSetting);
	NonBlueprintFormatterSettings.Add("AnimationStateGraph", AnimGraphSetting);

	FBAFormatterSettings NiagaraSettings;
	NiagaraSettings.Padding = DefaultFormatterPaddingSize;
	NiagaraSettings.AutoFormatting = EBAAutoFormatting::Never;
	NiagaraSettings.FormatterDirection = EGPD_Output;
	NiagaraSettings.RootNodes = { "NiagaraNodeInput" };
	NiagaraSettings.ExecPinName = "NiagaraParameterMap";

#if BA_UE_VERSION_OR_LATER(5, 0)
	NonBlueprintFormatterSettings.Add("NiagaraGraph", NiagaraSettings);
#else
	NonBlueprintFormatterSettings.Add("NiagaraGraphEditor", NiagaraSettings);
#endif

	// TODO: Reenable support for control rig after fixing issues
	FBAFormatterSettings ControlRigSettings;

#if BA_UE_VERSION_OR_LATER(5, 3)
	ControlRigSettings.ExecPinName = "RigVMExecuteContext";
#else
	ControlRigSettings.ExecPinName = "ControlRigExecuteContext";
#endif

#if BA_UE_VERSION_OR_LATER(5, 0)
	ControlRigSettings.bEnabled = true;
#else
	ControlRigSettings.bEnabled = false;
#endif
	ControlRigSettings.Padding = DefaultFormatterPaddingSize;
	ControlRigSettings.AutoFormatting = EBAAutoFormatting::Never;
	ControlRigSettings.FormatterDirection = EGPD_Output;
	NonBlueprintFormatterSettings.Add("ControlRigGraph", ControlRigSettings);

	FBAFormatterSettings MetaSoundSettings;
	MetaSoundSettings.Padding = DefaultFormatterPaddingSize;
	MetaSoundSettings.AutoFormatting = EBAAutoFormatting::Never;
	MetaSoundSettings.FormatterDirection = EGPD_Output;
	MetaSoundSettings.RootNodes = { "MetasoundEditorGraphInputNode" };
	NonBlueprintFormatterSettings.Add("MetasoundEditorGraph", MetaSoundSettings);

	FBAFormatterSettings EnvironmentQuerySettings;
	EnvironmentQuerySettings.FormatterType = EBAFormatterType::BehaviorTree;
	EnvironmentQuerySettings.Padding = DefaultFormatterPaddingSize;
	EnvironmentQuerySettings.AutoFormatting = EBAAutoFormatting::Never;
	EnvironmentQuerySettings.FormatterDirection = EGPD_Output;
	EnvironmentQuerySettings.RootNodes = { "EnvironmentQueryGraphNode_Root" };
	NonBlueprintFormatterSettings.Add("EnvironmentQueryGraph", EnvironmentQuerySettings);

	FBAFormatterSettings LogicDriverStateMachineGraphK2Settings;
	LogicDriverStateMachineGraphK2Settings.FormatterType = EBAFormatterType::Simple;
	LogicDriverStateMachineGraphK2Settings.Padding = DefaultFormatterPaddingSize;
	LogicDriverStateMachineGraphK2Settings.AutoFormatting = EBAAutoFormatting::Never;
	LogicDriverStateMachineGraphK2Settings.FormatterDirection = EGPD_Input;
	LogicDriverStateMachineGraphK2Settings.RootNodes = { "SMGraphK2Node_StateMachineSelectNode" };
	NonBlueprintFormatterSettings.Add("SMGraphK2", LogicDriverStateMachineGraphK2Settings);

	FBAFormatterSettings PCGGraphSettings;
	PCGGraphSettings.FormatterType = EBAFormatterType::Simple;
	PCGGraphSettings.Padding = DefaultFormatterPaddingSize;
	PCGGraphSettings.AutoFormatting = EBAAutoFormatting::Never;
	PCGGraphSettings.FormatterDirection = EGPD_Output;
	PCGGraphSettings.RootNodes = { "PCGEditorGraphNodeInput", "PCGEditorGraphNodeOutput" };
	NonBlueprintFormatterSettings.Add("PCGEditorGraph", PCGGraphSettings);

	FBAFormatterSettings FlowGraphSettings;
	FlowGraphSettings.FormatterType = EBAFormatterType::Simple;
	FlowGraphSettings.Padding = DefaultFormatterPaddingSize;
	FlowGraphSettings.AutoFormatting = EBAAutoFormatting::Never;
	FlowGraphSettings.FormatterDirection = EGPD_Output;
	NonBlueprintFormatterSettings.Add("FlowGraph", FlowGraphSettings);

	FBAFormatterSettings NotYetDialogueSettings;
	NotYetDialogueSettings.FormatterType = EBAFormatterType::BehaviorTree;
	// half height because the edge nodes count as a node
	NotYetDialogueSettings.Padding = FIntPoint(DefaultFormatterPaddingSize.X, DefaultFormatterPaddingSize.Y * 0.5f);
	NotYetDialogueSettings.AutoFormatting = EBAAutoFormatting::Never;
	NotYetDialogueSettings.FormatterDirection = EGPD_Output;
	NotYetDialogueSettings.RootNodes = { "DialogueGraphNode_Root" };
	NonBlueprintFormatterSettings.Add("DialogueGraph", NotYetDialogueSettings);

	bCreateKnotNodes = true;

	bAutoAddParentNode = true;

	bAutoRenameGettersAndSetters = true;
	bMergeGenerateGetterAndSetterButton = false;

	bEnableGlobalCommentBubblePinned = false;
	bGlobalCommentBubblePinnedValue = true;

	bDetectNewNodesAndCacheNodeSizes = false;
	bRefreshNodeSizeBeforeFormatting = true;

	bTreatDelegatesAsExecutionPins = true;

	bCenterBranches = false;
	NumRequiredBranches = 3;

	bCenterBranchesForParameters = false;
	NumRequiredBranchesForParameters = 2;

	bAddKnotNodesToComments = true;
	CommentNodePadding = FIntPoint(30, 30);

	bEnableFasterFormatting = false;

	bUseKnotNodePool = false;

	bSlowButAccurateSizeCaching = false;

	bApplyCommentPadding = true;

	KnotNodeDistanceThreshold = 800;

	bExpandNodesAheadOfParameters = true;
	bExpandNodesByHeight = true;
	bExpandParametersByHeight = false;

	bSnapToGrid = false;
	bAlignExecNodesTo8x8Grid = false;

	// ------------------------ //
	// Create variable defaults //
	// ------------------------ //

	bEnableVariableDefaults = false;
	bApplyVariableDefaultsToEventDispatchers = false;
	bDefaultVariableInstanceEditable = false;
	bDefaultVariableBlueprintReadOnly = false;
	bDefaultVariableExposeOnSpawn = false;
	bDefaultVariablePrivate = false;
	bDefaultVariableExposeToCinematics = false;
	DefaultVariableName = TEXT("VarName");
	DefaultVariableTooltip = FText::FromString(TEXT(""));
	DefaultVariableCategory = FText::FromString(TEXT(""));

	// ----------------- //
	// Function defaults //
	// ----------------- //

	bEnableFunctionDefaults = false;
	DefaultFunctionAccessSpecifier = EBAFunctionAccessSpecifier::Public;
	bDefaultFunctionPure = false;
	bDefaultFunctionConst = false;
	bDefaultFunctionExec = false;
	DefaultFunctionTooltip = FText::FromString(TEXT(""));
	DefaultFunctionKeywords = FText::FromString(TEXT(""));
	DefaultFunctionCategory = FText::FromString(TEXT(""));

	// ------------------------ //
	// Misc                     //
	// ------------------------ //

	bDisableBlueprintAssistPlugin = false;
	DefaultGeneratedGettersCategory = INVTEXT("Generated|Getters");
	DefaultGeneratedSettersCategory = INVTEXT("Generated|Setters");
	bEnableDoubleClickGoToDefinition = true;
	bPlayLiveCompileSound = false;
	bEnableInvisibleKnotNodes = false;
	bHighlightBadComments = false;
	FolderBookmarks.Add(EKeys::One);
	FolderBookmarks.Add(EKeys::Two);
	FolderBookmarks.Add(EKeys::Three);
	FolderBookmarks.Add(EKeys::Four);
	FolderBookmarks.Add(EKeys::Five);
	FolderBookmarks.Add(EKeys::Six);
	FolderBookmarks.Add(EKeys::Seven);
	FolderBookmarks.Add(EKeys::Eight);
	FolderBookmarks.Add(EKeys::Nine);
	FolderBookmarks.Add(EKeys::Zero);
	ClickTime = 0.35f;

	// ------------------------ //
	// Accessibility            //
	// ------------------------ //

	bShowOverlayWhenCachingNodes = true;
	RequiredNodesToShowOverlayProgressBar = 15;
}

void UBASettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	check(FBlueprintAssistModule::IsAvailable())

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();
	if (GraphHandler.IsValid())
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, bEnableGlobalCommentBubblePinned) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, bGlobalCommentBubblePinnedValue))
		{
			GraphHandler->ApplyGlobalCommentBubblePinned();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, ParameterStyle)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, FormattingStyle)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, ParameterWiringStyle)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, ExecutionWiringStyle)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, bLimitHelixingHeight)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, HelixingHeightMax)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, SingleNodeMaxHeight)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, BlueprintKnotTrackSpacing)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, BlueprintParameterPadding)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, FormatAllPadding)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, bTreatDelegatesAsExecutionPins)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, bExpandNodesByHeight)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, bExpandParametersByHeight)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, bCreateKnotNodes)
			|| PropertyName == NAME_None) // if the name is none, this probably means we changed a property through the toolbar
			// TODO: maybe there's a way to change property externally while passing in correct info name
		{
			GraphHandler->ClearFormatters();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings, CacheSaveLocation))
	{
		FBACache::Get().SaveCache();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TSharedRef<IDetailCustomization> FBASettingsDetails::MakeInstance()
{
	return MakeShareable(new FBASettingsDetails);
}

void FBASettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<FName> CategoryOrder = {
		"General",
		"FormattingOptions",
		"FormatAll",
		"BlueprintFormatting",
		"OtherGraphs",
		"CommentSettings",
		"Misc",
		"Accessibility",
		"NewFunctionDefaults",
		"NewVariableDefaults",
		"Experimental"
	};

	for (int i = 0; i < CategoryOrder.Num(); ++i)
	{
		DetailBuilder.EditCategory(CategoryOrder[i]).SetSortOrder(i);
	}


	static TArray<FName> DefaultCollapsedCategories = { "OtherGraphs", "NewVariableDefaults", "NewFunctionDefaults" };
	for (FName& CategoryName : DefaultCollapsedCategories)
	{
		DetailBuilder.EditCategory(CategoryName).InitiallyCollapsed(true);
	}

	//--------------------
	// General
	// -------------------

	IDetailCategoryBuilder& MiscCategory = DetailBuilder.EditCategory("Misc");
	auto& BACache = FBACache::Get();

	const FString CachePath = BACache.GetCachePath(true);

	const auto DeleteSizeCache = [&BACache]()
	{
		static FText Title = FText::FromString("Delete cache file");
		static FText Message = FText::FromString("Are you sure you want to delete the cache file?");

#if BA_UE_VERSION_OR_LATER(5, 3)
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);
#else
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title);
#endif
		if (Result == EAppReturnType::Yes)
		{
			BACache.DeleteCache();
		}

		return FReply::Handled();
	};

	MiscCategory.AddCustomRow(FText::FromString("Delete cache file"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Delete cache file"))
			.Font(BA_GET_FONT_STYLE(TEXT("PropertyWindow.NormalFont")))
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(5).AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Delete cache file"))
				.ToolTipText(FText::FromString(FString::Printf(TEXT("Delete cache file located at: %s"), *CachePath)))
				.OnClicked_Lambda(DeleteSizeCache)
			]
		];
}

FBAFormatterSettings UBASettings::GetFormatterSettings(UEdGraph* Graph)
{
	if (FBAFormatterSettings* Settings = FindFormatterSettings(Graph))
	{
		return *Settings;
	}

	return FBAFormatterSettings();
}

FBAFormatterSettings* UBASettings::FindFormatterSettings(UEdGraph* Graph)
{
	if (!Graph)
	{
		return nullptr;
	}

	UBASettings& BASettings = GetMutable();

	if (FBAFormatterSettings* FoundSettings = BASettings.NonBlueprintFormatterSettings.Find(Graph->GetClass()->GetFName()))
	{
		if (FoundSettings->bEnabled)
		{
			return FoundSettings;
		}
	}

	if (FBAUtils::IsBlueprintGraph(Graph, false))
	{
		return &BASettings.BlueprintFormatterSettings;
	}

	return nullptr; 
}
