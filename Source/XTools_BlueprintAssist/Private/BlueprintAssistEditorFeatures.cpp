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
						if (Settings->MulticastAffix.Matches(NewTitle))
						{
							NetFlags = FUNC_NetMulticast;
						}
						else if (Settings->ServerAffix.Matches(NewTitle))
						{
							NetFlags = FUNC_NetServer;
						}
						else if (Settings->ClientAffix.Matches(NewTitle))
						{
							NetFlags = FUNC_NetClient;
						}

						// don't update flags if we have no matching prefix and the setting to clear flags is disabled
						const bool bDontUpdateFlags = (NetFlags == FUNC_None) && !GetDefault<UBASettings_EditorFeatures>()->bClearReplicationFlagsWhenRenaming;
						if (!bDontUpdateFlags)
						{
							SetNodeNetFlags(EventNode, NetFlags);
						}
					}
				}
			}
			else if (PropertyName == FunctionFlagsName)
			{
				if (Settings->bAddReplicationAffixToCustomEventTitle)
				{
					if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Object))
					{
						FBAStringAffix Affix;
						if (CustomEvent->FunctionFlags & FUNC_NetMulticast)
						{
							Affix = Settings->MulticastAffix;
						}
						else if (CustomEvent->FunctionFlags & FUNC_NetServer)
						{
							Affix = Settings->ServerAffix;
						}
						else if (CustomEvent->FunctionFlags & FUNC_NetClient)
						{
							Affix = Settings->ClientAffix;
						}

						FString NodeTitle = CustomEvent->GetNodeTitle(ENodeTitleType::MenuTitle).ToString();
						if (!Affix.Matches(NodeTitle))
						{
							// clear any existing affixes if they apply
							Settings->MulticastAffix.RemoveAffix(NodeTitle);
							Settings->ServerAffix.RemoveAffix(NodeTitle);
							Settings->ClientAffix.RemoveAffix(NodeTitle);

							// set the new affix
							Affix.AddAffix(NodeTitle);

							CustomEvent->OnRenameNode(NodeTitle);
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
