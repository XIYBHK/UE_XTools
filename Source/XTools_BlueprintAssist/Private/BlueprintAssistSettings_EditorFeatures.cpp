#include "BlueprintAssistSettings_EditorFeatures.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "InputCoreTypes.h"
#include "Framework/Commands/InputChord.h"

FBAVariableDefaults::FBAVariableDefaults()
	: bDefaultVariablePrivate(false)
	, bDefaultVariableInstanceEditable(false)
	, bDefaultVariableBlueprintReadOnly(false)
	, bDefaultVariableExposeOnSpawn(false)
	, bDefaultVariableExposeToCinematics(false)
	, bDefaultVariableTransient(false)
	, bDefaultVariableSaveGame(false)
	, bDefaultVariableAdvancedDisplay(false)
	, bDefaultConfigVariable(false)
	, DefaultVariableName("VarName")
	, bApplyVariableDefaultsToEventDispatchers(false)
{
}

FBAFunctionDefaults::FBAFunctionDefaults()
	: DefaultFunctionAccessSpecifier(EBAFunctionAccessSpecifier::Public)
	, bDefaultFunctionPure(false)
	, bDefaultFunctionConst(false)
	, bDefaultFunctionExec(false)
	, bDefaultFunctionThreadSafe(false)
{
}

FBACustomEventDefaults::FBACustomEventDefaults()
	: DefaultEventAccessSpecifier(EBAFunctionAccessSpecifier::Public)
	, bDefaultEventNetReliable(false)
{
}

UBASettings_EditorFeatures::UBASettings_EditorFeatures(const FObjectInitializer& ObjectInitializer)
{
	//~~~ CustomEventReplication
	bSetReplicationFlagsAfterRenaming = true;
	bClearReplicationFlagsWhenRenaming = false;
	bAddReplicationAffixToCustomEventTitle = true;
	MulticastAffix = FBAStringAffix("Multicast_");
	ServerAffix = FBAStringAffix("Server_");
	ClientAffix = FBAStringAffix("Client_");

	//~~~ NodeGroup
	bDrawNodeGroupOutline = true;
	bOnlyDrawGroupOutlineWhenSelected = false;
	NodeGroupOutlineColor = FLinearColor(0.5, 0.5, 0, 0.4);
	NodeGroupOutlineWidth = 4.0f;
	NodeGroupOutlineMargin = FMargin(12.0f);

	bDrawNodeGroupFill = false;
	NodeGroupFillColor = FLinearColor(0.5f, 0.5f, 0, 0.15f);

	//~~~ Mouse Features
	GroupMovementChords.Add(FInputChord(EKeys::SpaceBar));
	bDragRequiresNodeUnderCursor = false;

	//~~~ Graph
	ShiftCameraDistance = 400;
	bAutoAddParentNode = true;
	SelectedPinHighlightColor = FLinearColor(0.6f, 0.6f, 0.6f, 0.33);
	PinSelectionMethod_Execution = EBAPinSelectionMethod::Execution;
	PinSelectionMethod_Parameter = EBAPinSelectionMethod::Execution;

	//~~~ Graph | Comments
	bEnableGlobalCommentBubblePinned = false;
	bGlobalCommentBubblePinnedValue = true;

	//~~~ Graph | NewNodeBehaviour
	InsertNewNodeKeyChord = FInputChord(EKeys::LeftControl);
	bAlwaysConnectExecutionFromParameter = true;
	bAlwaysInsertFromParameter = false;
	bAlwaysInsertFromExecution = false;
	bSelectValuePinWhenCreatingNewNodes = false;

	//~~~ General
	bAddToolbarWidget = true;

	//~~~ General | Getters and Setters
	bAutoRenameGettersAndSetters = true;
	bMergeGenerateGetterAndSetterButton = false;

	//~~~ Defaults
	bEnableVariableDefaults = false;
	bEnableFunctionDefaults = false;
	bEnableCustomEventDefaults = false;

	//~~~ Misc
	bDisplayAllHotkeys = false;
	bShowWelcomeScreenOnLaunch = true;

	CopyPinValueChord = FInputChord(EKeys::RightMouseButton, EModifierKey::Shift);
	PastePinValueChord = FInputChord(EKeys::LeftMouseButton, EModifierKey::Shift);
	FocusInDetailsPanelChord = FInputChord();
	AllowConversionConnectionsModifier = EKeys::LeftShift;
	bEnterCyclesPinValues = true;
	bEnterInteractsWithPin = true;
	bTabCyclePinValues = true;

	DefaultGeneratedGettersCategory = INVTEXT("Generated|Getters");
	DefaultGeneratedSettersCategory = INVTEXT("Generated|Setters");
	bEnableDoubleClickGoToDefinition = true;
	bPlayLiveCompileSound = false;
	bEnableInvisibleKnotNodes = false;
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

	SaveSettingsDefaults();
}

void UBASettings_EditorFeatures::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	TSharedPtr<FBAGraphHandler> GraphHandler = FBAUtils::GetCurrentGraphHandler();
	if (GraphHandler.IsValid())
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bEnableGlobalCommentBubblePinned) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bGlobalCommentBubblePinnedValue))
		{
			GraphHandler->ApplyGlobalCommentBubblePinned();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bPlayLiveCompileSound))
	{
		if (bPlayLiveCompileSound)
		{
			FBlueprintAssistModule::Get().BindLiveCodingSound();
		}
		else
		{
			FBlueprintAssistModule::Get().UnbindLiveCodingSound();
		}
	}
}

TSharedRef<IDetailCustomization> FBASettingsDetails_EditorFeatures::MakeInstance()
{
	return MakeShared<FBASettingsDetails_EditorFeatures>();
}

void FBASettingsDetails_EditorFeatures::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<FName> CategoryOrder = {
		"General",
		"Graph",
		"Inputs"
		"Misc",
	};

	for (int i = 0; i < CategoryOrder.Num(); ++i)
	{
		DetailBuilder.EditCategory(CategoryOrder[i]).SetSortOrder(i);
	}
}
