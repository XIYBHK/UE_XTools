#include "BlueprintAssistSettings_EditorFeatures.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "InputCoreTypes.h"
#include "Framework/Commands/InputChord.h"

UBASettings_EditorFeatures::UBASettings_EditorFeatures(const FObjectInitializer& ObjectInitializer)
{
	//~~~ CustomEventReplication
	bSetReplicationFlagsAfterRenaming = true;
	bClearReplicationFlagsWhenRenamingWithNoPrefix = false;
	bAddReplicationPrefixToCustomEventTitle = true;
	MulticastPrefix = "Multicast_";
	ServerPrefix = "Server_";
	ClientPrefix = "Client_";

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

	//~~~ Misc
	bDisplayAllHotkeys = false;
	bShowWelcomeScreenOnLaunch = false;

	CopyPinValueChord = FInputChord(EKeys::RightMouseButton, EModifierKey::Shift);
	PastePinValueChord = FInputChord(EKeys::LeftMouseButton, EModifierKey::Shift);
	FocusInDetailsPanelChord = FInputChord();

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

	// --------------------- //
	// Custom event defaults //
	// --------------------- //

	bEnableEventDefaults = false;
	DefaultEventAccessSpecifier = EBAFunctionAccessSpecifier::Public;
	bDefaultEventNetReliable = false;

	// ------------------------ //
	// Misc                     //
	// ------------------------ //

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
