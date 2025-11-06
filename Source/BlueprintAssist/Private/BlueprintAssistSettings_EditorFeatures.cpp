#include "BlueprintAssistSettings_EditorFeatures.h"

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

	//~~~ General | NewNodeBehaviour
	InsertNewNodeKeyChord = FInputChord(EKeys::LeftControl);
	bAlwaysConnectExecutionFromParameter = true;
	bAlwaysInsertFromParameter = false;
	bAlwaysInsertFromExecution = false;
	bSelectValuePinWhenCreatingNewNodes = false;

	CopyPinValueChord = FInputChord(EKeys::RightMouseButton, EModifierKey::Control | EModifierKey::Alt);
	PastePinValueChord = FInputChord(EKeys::LeftMouseButton, EModifierKey::Control | EModifierKey::Alt);
}
