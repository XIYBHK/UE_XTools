#include "BlueprintAssistTypes.h"

#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistUtils.h"
#include "ISettingsEditorModule.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "EdGraph/EdGraph.h"
#include "Modules/ModuleManager.h"

void FBAGraphPinHandle::SetPin(UEdGraphPin* Pin)
{
	if (!FBAUtils::IsValidPin(Pin))
	{
		Invalidate();
	}
	else if (UEdGraphNode* Node = Pin->GetOwningNodeUnchecked())
	{
		Graph = Node->GetGraph();
		NodeGuid = Node->NodeGuid;
		PinId = Pin->PinId;
		PinType = Pin->PinType;
		PinName = Pin->PinName;
	}
}

UEdGraphPin* FBAGraphPinHandle::GetPin(bool bFallbackOnPinName)
{
	if (UEdGraphPin* Pin = GetPinConst(bFallbackOnPinName))
	{
		if (PinId != Pin->PinId)
		{
			PinId = Pin->PinId;
		}

		return Pin;
	}

	return nullptr;
}

UEdGraphPin* FBAGraphPinHandle::GetPinConst(bool bFallbackOnPinName) const
{
	if (!IsValid())
	{
		return nullptr;
	}

	for (auto Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		if (Node->NodeGuid == NodeGuid)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->PinId == PinId)
				{
					return Pin;
				}
			}

			if (bFallbackOnPinName)
			{
				// guid failed, find using PinType & PinName
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if ((Pin->PinType == PinType) && (Pin->PinName == PinName))
					{
						return Pin;
					}
				}
			}

			return nullptr;
		}
	}

	return nullptr;
}

FString FBAGraphPinHandle::ToString() const
{
	return FString::Printf(TEXT("N[%s] P[%s]Name[%s]"), *NodeGuid.ToString(), *PinId.ToString(), *PinName.ToString()); 
}

void FBANodePinHandle::SetPin(UEdGraphPin* Pin)
{
	if (Pin)
	{
		PinId = Pin->PinId;
		Node = Pin->GetOwningNodeUnchecked();
		PinType = Pin->PinType;
		PinName = Pin->PinName;
	}
	else
	{
		PinId.Invalidate();
		Node = nullptr;
		PinType.ResetToDefaults();
		PinName = NAME_None;
	}
}

UEdGraphPin* FBANodePinHandle::GetPin()
{
	// find the pin from the node using the stored guid
	if (!PinId.IsValid() || !Node.IsValid())
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin->PinId == PinId)
		{
			return Pin;
		}
	}

	// guid failed, find using PinType & PinName
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if ((Pin->PinType == PinType) && (Pin->PinName == PinName))
		{
			// side effect: also update the latest PinId
			PinId = Pin->PinId;

			return Pin;
		}
	}

	return nullptr;
}

void FBANodeMovementTransaction::Begin(const TSet<UEdGraphNode*>& Nodes, const FText& InSessionName, const EBADragMethod& InDragMethod)
{
	if (Nodes.Num() == 0)
	{
		return;
	}

	const bool bHasTransaction = TransactionPtr.IsValid() && TransactionPtr->IsOutstanding();
	if (!bHasTransaction)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(InSessionName);
		DragMethod = InDragMethod;
	}

	for (UEdGraphNode* Node : Nodes)
	{
		Node->Modify(false);
	}
}

void FBANodeMovementTransaction::End(const EBADragMethod& InDragMethod)
{
	if (!IsValid() || DragMethod != InDragMethod)
	{
		return;
	}

	TransactionPtr.Reset();
}

void FBANodeSet::Add(UEdGraphNode* Node)
{
	WeakNodes.Add(Node);
}

TSet<UEdGraphNode*> FBANodeSet::GetNodeObjs() const
{
	TSet<UEdGraphNode*> OutNodeArray;
	for (TWeakObjectPtr<UEdGraphNode> Node : WeakNodes)
	{
		if (Node.IsValid())
		{
			OutNodeArray.Add(Node.Get());
		}
	}

	return OutNodeArray;
}

void FBANodeSet::Empty()
{
	WeakNodes.Empty();
}

void FBASettingsPropertyHook::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	FNotifyHook::NotifyPreChange(PropertyAboutToChange);
	if (UClass* OwnerClass = PropertyAboutToChange->GetOwnerClass())
	{
		if (UObject* CDO = OwnerClass->GetDefaultObject())
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(PropertyAboutToChange);
			CDO->PreEditChange(PropertyChain);
		}
	}
}

void FBASettingsPropertyHook::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	// See SSettingsEditor::NotifyPostChange
	if (PropertyChangedEvent.GetNumObjectsBeingEdited() > 0)
	{
		if (UObject* ObjectBeingEdited = const_cast<UObject*>(PropertyChangedEvent.GetObjectBeingEdited(0)))
		{
			ObjectBeingEdited->PostEditChange();
			ObjectBeingEdited->SaveConfig();

			static const FName ConfigRestartRequiredKey = "ConfigRestartRequired";
			const bool bShowRestartEditor = PropertyChangedEvent.Property->GetBoolMetaData(ConfigRestartRequiredKey) || (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetBoolMetaData(ConfigRestartRequiredKey));
			if (bShowRestartEditor)
			{
				ISettingsEditorModule* SettingsEditorModule = static_cast<ISettingsEditorModule*>(FModuleManager::Get().LoadModule("SettingsEditor"));
				if (SettingsEditorModule)
				{
					SettingsEditorModule->OnApplicationRestartRequired();
				}
			}
		}
	}
}
