// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistSettings.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintAssistUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "EdGraphSchema_K2.h"

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
		"DialogueEditor",
		"CustomizableObjectEditor",
		"CameraRigAssetEditor",
		"CameraAssetEditor",
	};

	SupportedGraphEditors = { "SGraphEditor", "SFlowGraphEditor" };

	bEnableShakeNodeOffWire = true;
	ShakeNodeOffWireTimeWindow = 0.3f;

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

	bGloballyDisableAutoFormatting = true;
	bSkipAutoFormattingAfterBreakingPins = true;

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
		EBAAutoFormatting::Never,
		EGPD_Output,
		{ "BehaviorTreeGraphNode_Root" }
	);

	BehaviorTreeSettings.FormatterType = EBAFormatterType::BehaviorTree;

	BehaviorTreeBranchExtraPadding = 100.0f;

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

	FBAFormatterSettings ControlRigSettings;

#if BA_UE_VERSION_OR_LATER(5, 3)
	ControlRigSettings.ExecPinName = "RigVMExecuteContext";
#else
	ControlRigSettings.ExecPinName = "ControlRigExecuteContext";
#endif
	ControlRigSettings.bEnabled = false;
	ControlRigSettings.Padding = DefaultFormatterPaddingSize;
	ControlRigSettings.AutoFormatting = EBAAutoFormatting::Never;
	ControlRigSettings.FormatterDirection = EGPD_Output;
	NonBlueprintFormatterSettings.Add("ControlRigGraph", ControlRigSettings);

	FBAFormatterSettings MetaSoundSettings;
	MetaSoundSettings.Padding = DefaultFormatterPaddingSize;
	MetaSoundSettings.AutoFormatting = EBAAutoFormatting::Never;
	MetaSoundSettings.FormatterDirection = EGPD_Output;
	MetaSoundSettings.RootNodes = { "MetasoundEditorGraphInputNode" };
	MetaSoundSettings.ExecPinName = "Trigger";
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

	FBAFormatterSettings MutableSettings;
	MutableSettings.FormatterType = EBAFormatterType::Simple;
	MutableSettings.Padding = DefaultFormatterPaddingSize;
	MutableSettings.RootNodes = { "CustomizableObjectNodeObject" };
	MutableSettings.AutoFormatting = EBAAutoFormatting::Never;
	MutableSettings.FormatterDirection = EGPD_Input;
	NonBlueprintFormatterSettings.Add("CustomizableObjectGraph", MutableSettings);

	FBAFormatterSettings ObjectTreeGraphSettings;
	ObjectTreeGraphSettings.FormatterType = EBAFormatterType::Simple;
	ObjectTreeGraphSettings.Padding = DefaultFormatterPaddingSize;
	ObjectTreeGraphSettings.RootNodes = { "ObjectTreeGraphNode" };
	ObjectTreeGraphSettings.AutoFormatting = EBAAutoFormatting::Never;
	ObjectTreeGraphSettings.FormatterDirection = EGPD_Output;
	NonBlueprintFormatterSettings.Add("ObjectTreeGraph", ObjectTreeGraphSettings);

	FBAFormatterSettings ReferenceViewerGraphSettings;
	ReferenceViewerGraphSettings.bEnabled = false;
	ReferenceViewerGraphSettings.FormatterType = EBAFormatterType::Simple;
	ReferenceViewerGraphSettings.Padding = DefaultFormatterPaddingSize;
	ReferenceViewerGraphSettings.AutoFormatting = EBAAutoFormatting::Never;
	ReferenceViewerGraphSettings.FormatterDirection = EGPD_Output;
	NonBlueprintFormatterSettings.Add("EdGraph_ReferenceViewer", ReferenceViewerGraphSettings);

	bCreateKnotNodes = true;

	bDetectNewNodesAndCacheNodeSizes = false;

	bTreatDelegatesAsExecutionPins = true;

	bCenterBranches = false;
	NumRequiredBranches = 3;

	bCenterBranchesForParameters = false;
	NumRequiredBranchesForParameters = 2;

	bApplyCommentPadding = true;
	bAddKnotNodesToComments = true;
	CommentNodePadding = FIntPoint(30, 30);
	bRefreshCommentTitleBarSize = true;

	bEnableFasterFormatting = false;

	bUseKnotNodePool = false;

	KnotNodeDistanceThreshold = 800;

	bExpandNodesAheadOfParameters = true;
	bExpandNodesByHeight = true;
	ExpandNodesMaxDist = 400;
	bExpandParametersByHeight = true;
	ExpandParametersMaxDist = 160;

	bSnapToGrid = false;
	bAlignExecNodesTo8x8Grid = false;

	// ------------------------ //
	// Accessibility            //
	// ------------------------ //

	bShowOverlayWhenCachingNodes = true;
	RequiredNodesToShowOverlayProgressBar = 15;

	bSaveAllBeforeFormatting = false;
	bFormatAllAfterSaving = false;

	SaveSettingsDefaults();
}

void UBASettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	check(FBlueprintAssistModule::IsAvailable())

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// when we change any formatting related setting, clear the formatting data
	for (auto GH : FBATabHandler::Get().GetAllGraphHandlers())
	{
		if (GH)
		{
			GH->ClearFormatters();
		}
	}

	FBACache::Get().ClearLastFormatted();

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
		"Comments",
		"BlueprintFormatting",
		"OtherGraphs",
		"Misc",
		"Accessibility",
		"Experimental"
	};

	for (int i = 0; i < CategoryOrder.Num(); ++i)
	{
		DetailBuilder.EditCategory(CategoryOrder[i]).SetSortOrder(i);
	}

	static TArray<FName> DefaultCollapsedCategories = { "OtherGraphs" };
	for (FName& CategoryName : DefaultCollapsedCategories)
	{
		DetailBuilder.EditCategory(CategoryName).InitiallyCollapsed(true);
	}
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

	if (FBAFormatterSettings* FoundSettings = BASettings.NonBlueprintFormatterSettings.Find(FBAUtils::GetObjectClassName(Graph)))
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
