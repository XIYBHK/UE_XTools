// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistEditorFeatures.h"

#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"

#if BA_UE_VERSION_OR_LATER(5, 1)
#include "Misc/TransactionObjectEvent.h"
#else
#include "Misc/ITransaction.h"
#endif

void UBAEditorFeatures::Init()
{
	FCoreUObjectDelegates::OnObjectTransacted.AddUObject(this, &ThisClass::OnObjectTransacted);
}

UBAEditorFeatures::~UBAEditorFeatures()
{
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
}

void UBAEditorFeatures::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	static const FName CustomFunctionName = GET_MEMBER_NAME_CHECKED(UK2Node_CustomEvent, CustomFunctionName);
	static const FName FunctionFlagsName = GET_MEMBER_NAME_CHECKED(UK2Node_CustomEvent, FunctionFlags);

	if (Event.GetEventType() == ETransactionObjectEventType::Finalized)
	{
		if (Event.GetChangedProperties().Num() == 1)
		{
			const FName& PropertyName = Event.GetChangedProperties()[0];
			const UBASettings_EditorFeatures* Settings = GetDefault<UBASettings_EditorFeatures>();

			if (PropertyName == CustomFunctionName)
			{
				if (Settings->bSetReplicationFlagsAfterRenaming)
				{
					if (UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Object))
					{
						EFunctionFlags NetFlags = FUNC_None;

						const FString NewTitle = EventNode->GetNodeTitle(ENodeTitleType::MenuTitle).ToString();
						if (NewTitle.StartsWith(Settings->MulticastPrefix))
						{
							NetFlags = FUNC_NetMulticast;
						}
						else if (NewTitle.StartsWith(Settings->ServerPrefix))
						{
							NetFlags = FUNC_NetServer;
						}
						else if (NewTitle.StartsWith(Settings->ClientPrefix))
						{
							NetFlags = FUNC_NetClient;
						}

						// don't update flags if we have no matching prefix and the setting to clear flags is disabled
						const bool bDontUpdateFlags = (NetFlags == FUNC_None) && !GetDefault<UBASettings_EditorFeatures>()->bClearReplicationFlagsWhenRenamingWithNoPrefix;
						if (!bDontUpdateFlags)
						{
							SetNodeNetFlags(EventNode, NetFlags);
						}
					}
				}
			}
			else if (PropertyName == FunctionFlagsName)
			{
				if (Settings->bAddReplicationPrefixToCustomEventTitle)
				{
					if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Object))
					{
						const FString CurrentNodeTitle = CustomEvent->GetNodeTitle(ENodeTitleType::MenuTitle).ToString();
						FString Prefix = "";
						if (CustomEvent->FunctionFlags & FUNC_NetMulticast)
						{
							Prefix = Settings->MulticastPrefix;
						}
						else if (CustomEvent->FunctionFlags & FUNC_NetServer)
						{
							Prefix = Settings->ServerPrefix;
						}
						else if (CustomEvent->FunctionFlags & FUNC_NetClient)
						{
							Prefix = Settings->ClientPrefix;
						}

						FString NewTitle = CurrentNodeTitle;
						if (!CurrentNodeTitle.StartsWith(Prefix))
						{
							NewTitle.RemoveFromStart(Settings->MulticastPrefix);
							NewTitle.RemoveFromStart(Settings->ServerPrefix);
							NewTitle.RemoveFromStart(Settings->ClientPrefix);

							NewTitle = Prefix + NewTitle;

							CustomEvent->OnRenameNode(NewTitle);
						}
					}
				}
			}
		}
	}
}

// Logic to set rep flags from FBlueprintGraphActionDetails::SetNetFlags
bool UBAEditorFeatures::SetNodeNetFlags(UK2Node* Node, EFunctionFlags NetFlags)
{
	const int32 FlagsToSet = NetFlags ? FUNC_Net | NetFlags : 0;
	constexpr int32 FlagsToClear = FUNC_Net | FUNC_NetMulticast | FUNC_NetServer | FUNC_NetClient;

	bool bBlueprintModified = false;

	// Clear all net flags before setting
	if (FlagsToSet != FlagsToClear)
	{
		if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
		{
			Node->Modify();
			CustomEventNode->FunctionFlags &= ~FlagsToClear;
			CustomEventNode->FunctionFlags |= FlagsToSet;
			bBlueprintModified = true;
		}

		if (bBlueprintModified)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Node->GetBlueprint());
		}
	}

	return bBlueprintModified;
}
