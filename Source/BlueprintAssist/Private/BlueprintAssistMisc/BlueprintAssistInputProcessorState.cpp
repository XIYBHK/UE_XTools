// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistMisc/BlueprintAssistInputProcessorState.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistInputProcessor.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistTabHandler.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

bool FBAInputProcessorState::OnKeyOrMouseDown(const FKey& Key)
{
	if (TryCopyPastePinValue())
	{
		bConsumeMouseUp = true;
		return true;
	}

	return false;
}

bool FBAInputProcessorState::OnKeyOrMouseUp(const FKey& Key)
{
	if (bConsumeMouseUp)
	{
		bConsumeMouseUp = false;
		return true;
	}

	return false;
}

bool FBAInputProcessorState::TryCopyPastePinValue()
{
	TSharedPtr<FBAGraphHandler> GraphHandler = FBATabHandler::Get().GetActiveGraphHandler();
	if (!GraphHandler)
	{
		return false;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphHandler->GetGraphPanel();
	if (!GraphPanel)
	{
		return false;
	}

	UEdGraphPin* PinObj = nullptr;
	TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetHoveredGraphPin(GraphPanel);
	if (GraphPin)
	{
		PinObj = GraphPin->GetPinObj();
	}
	// else // try get the hovered pin from the selected pin
	// {
	// 	PinObj = GraphHandler->GetSelectedPin();
	// 	GraphPin = FBAUtils::GetGraphPin(GraphPanel, PinObj);
	// }

	if (!GraphPin || !PinObj)
	{
		return false;
	}

	if (FBAInputProcessor::Get().IsInputChordDown(UBASettings_EditorFeatures::Get().PastePinValueChord))
	{
		if (CopiedDefaultValue.IsSet())
		{
			FScopedTransaction Transaction(INVTEXT("Paste pin value"));
			PinObj->Modify();

			const bool bSuccess = FBAUtils::TrySetDefaultPinValues(PinObj, CopiedDefaultValue->PinValue, CopiedDefaultValue->PinObject.Get(), CopiedDefaultValue->PinTextValue);

			const FText Message = bSuccess ? INVTEXT("Pasted pin value") : INVTEXT("Pin not supported for pasting");
			FNotificationInfo Notification(Message);
			Notification.ExpireDuration = 1.0f;
			FSlateNotificationManager::Get().AddNotification(Notification);

			if (bSuccess)
			{
				if (TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel))
				{
					HoveredNode->GetNodeObj()->ReconstructNode();
					HoveredNode->UpdateGraphNode();

					// select the pin
					UEdGraphPin* Pin = GraphPin->GetPinObj();
					GraphHandler->SetSelectedPin(Pin);
				}
			}
		}

		return true;
	}

	if (FBAInputProcessor::Get().IsInputChordDown(UBASettings_EditorFeatures::Get().CopyPinValueChord))
	{
		// copy
		FBAPinDefaultValue NewDefaultValue;

		// Check the default value and make it an error if it's bogus
		NewDefaultValue.PinValue = PinObj->DefaultValue;
		NewDefaultValue.PinTextValue = PinObj->DefaultTextValue;
		NewDefaultValue.PinObject = PinObj->DefaultObject;
		CopiedDefaultValue = NewDefaultValue;

		FNotificationInfo Notification(FText::FromString(FString::Printf(TEXT("Copied pin value %s %s"), *PinObj->GetDisplayName().ToString(), *PinObj->PinType.PinCategory.ToString())));
		Notification.ExpireDuration = 1.0f;
		FSlateNotificationManager::Get().AddNotification(Notification);

		// select the pin
		UEdGraphPin* Pin = GraphPin->GetPinObj();
		GraphHandler->SetSelectedPin(Pin);

		// CopiedPinType = PinObj->PinType;

		return true;
	}

	return false;
}

// TODO context hovered copy paste node, if pasted on a node it will replace, if pasted on a wire it will insert
// bool FBAInputProcessorState::SpecialCopyPasteNode()
// {
	// if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && SlateApp.GetModifierKeys().IsAltDown())
	// {
	// 	if (TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel))
	// 	{
	// 		FString ExportedText;
	// 		FEdGraphUtilities::ExportNodesToText({ HoveredNode->GetNodeObj() }, ExportedText);
	// 		FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
	//
	// 		FNotificationInfo Notification(FText::FromString(FString::Printf(TEXT("Copied node"))));
	// 		Notification.ExpireDuration = 1.0f;
	// 		FSlateNotificationManager::Get().AddNotification(Notification);
	//
	// 		return true;
	// 	}
	// }

	// if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && SlateApp.GetModifierKeys().IsAltDown())
	// {
	// 	TSharedPtr<FScopedTransaction> Transaction = MakeShareable(new FScopedTransaction(INVTEXT("Paste Node")));
	// 	TSet<UEdGraphNode*> PastedNodes;
	// 	FString ExportedText;
	// 	FPlatformApplicationMisc::ClipboardPaste(ExportedText);
	//
	// 	UEdGraph* DestinationGraph = GraphHandler->GetFocusedEdGraph();
	// 	FEdGraphUtilities::ImportNodesFromText(DestinationGraph, ExportedText, /*out*/ PastedNodes);
	//
	// 	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	// 	{
	// 		if (UEdGraphNode* Node = *It)
	// 		{
	// 			Node->CreateNewGuid();
	// 			FVector2D MousePos = FBAUtils::ScreenSpaceToPanelCoord(GraphPanel, FSlateApplication::Get().GetCursorPos());
	// 			Node->NodePosX = MousePos.X; 
	// 			Node->NodePosY = MousePos.Y;
	//
	// 			// Update the selected node
	// 			UK2Node* K2Node = Cast<UK2Node>(Node);
	// 			if (K2Node != nullptr && K2Node->NodeCausesStructuralBlueprintChange())
	// 			{
	// 				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GraphHandler->GetBlueprint());
	// 			}
	// 			else
	// 			{
	// 				FBlueprintEditorUtils::MarkBlueprintAsModified(GraphHandler->GetBlueprint());
	// 			}
	//
	// 			// TODO why doesn't this work?
	// 			// DestinationGraph->AddNode(Node, true, true);
	// 			if (TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel))
	// 			{
	// 				GraphHandler->NodeToReplace = HoveredNode->GetNodeObj();
	// 				GraphHandler->SetReplaceNewNodeTransaction(Transaction);
	// 			}
	//
	// 			
	// 		}
	//
	// 		FNotificationInfo Notification(FText::FromString(FString::Printf(TEXT("Pasted Node"))));
	// 		Notification.ExpireDuration = 1.0f;
	// 		FSlateNotificationManager::Get().AddNotification(Notification);
	// 	}
	//
	// 	DestinationGraph->NotifyGraphChanged();
	//
	// 	return true;
	// }
// }

// TODO doesn't currently update existing intances of nodes on the graph
// bool FBAInputProcessorState::TryCopyPastePinType()
// {
	// // select the hovered pin on LMB or RMB
	// UBASettings_EditorFeatures::Get().CopyPinValueChord;
	// // try copy / paste the hovered pin
	// if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	// {
	// 	
	//
	// }
	// // paste
	// if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	// {
	// 	if (SlateApp.GetModifierKeys().IsShiftDown())
	// 	{
	// 	// 	if (CopiedPinType.IsSet())
	// 	// 	{
	// 	// 		if (TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel))
	// 	// 		{
	// 	// 			if (auto Var = Cast<UK2Node_Variable>(FBAUtils::GetHoveredNode(GraphPanel)))
	// 	// 			{
	// 	// 				FBlueprintEditorUtils::ChangeMemberVariableType(GraphHandler->GetBlueprint(), HoveredPinObj->GetFName(), CopiedPinType.GetValue());
	// 	// 				HoveredNode->GetNodeObj()->ReconstructNode();
	// 	// 				HoveredNode->UpdateGraphNode();
	// 	// 				return true;
	// 	// 			}
	// 	//
	// 	// 			if (UK2Node_EditablePinBase* HoveredEventNode = Cast<UK2Node_EditablePinBase>(FBAUtils::GetHoveredNode(GraphPanel)))
	// 	// 			{
	// 	// 				// if (UFunction* LocalVariableScope = VarProp->GetOwner<UFunction>())
	// 	// 				// {
	// 	// 				// 	FBlueprintEditorUtils::ChangeLocalVariableType(GraphHandler->GetBlueprint(), LocalVariableScope, HoveredPinObj->GetFName(), CopiedPinType.GetValue());
	// 	// 				// }
	// 	// 				// else
	// 	// 				{
	// 	//
	// 	// 					{
	// 	// 						auto PinName = HoveredPinObj->GetFName();
	// 	// 						auto Node = HoveredEventNode;
	// 	// 						auto PinType = CopiedPinType.GetValue();
	// 	// 						if (Node)
	// 	// 						{
	// 	// 							TSharedPtr<FUserPinInfo>* UDPinPtr = Node->UserDefinedPins.FindByPredicate([PinName](TSharedPtr<FUserPinInfo>& UDPin)
	// 	// 							{
	// 	// 								return UDPin.IsValid() && (UDPin->PinName == PinName);
	// 	// 							});
	// 	// 							if (UDPinPtr)
	// 	// 							{
	// 	// 								Node->Modify();
	// 	// 								(*UDPinPtr)->PinType = PinType;
	// 	//
	// 	// 								// Inputs flagged as pass-by-reference will also be flagged as 'const' here to conform to the expected native C++
	// 	// 								// declaration of 'const Type&' for input reference parameters on functions with no outputs (i.e. events). Array
	// 	// 								// types are also flagged as 'const' here since they will always be implicitly passed by reference, regardless of
	// 	// 								// the checkbox setting. See UEditablePinBase::PostLoad() for more details.
	// 	// 								if(!PinType.bIsConst && Node->ShouldUseConstRefParams())
	// 	// 								{
	// 	// 									(*UDPinPtr)->PinType.bIsConst = PinType.IsArray() || PinType.bIsReference;
	// 	// 								}
	// 	//
	// 	// 								// Reset default value, it probably doesn't match
	// 	// 								(*UDPinPtr)->PinDefaultValue.Reset();
	// 	//
	// 	// 								if (auto BP = GraphHandler->GetBlueprint())
	// 	// 								{
	// 	// 									auto BlueprintEditor = FBAMiscUtils::GetEditorFromActiveTabCasted<UBlueprint, FBlueprintEditor>();
	// 	// 									// Auto-import the underlying type object's default namespace set into the current editor context.
	// 	// 									const UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();
	// 	// 									if (PinSubCategoryObject && BlueprintEditor)
	// 	// 									{
	// 	// 										FBlueprintEditor::FImportNamespaceExParameters Params;
	// 	// 										FBlueprintNamespaceUtilities::GetDefaultImportsForObject(PinSubCategoryObject, Params.NamespacesToImport);
	// 	// 										BlueprintEditor->ImportNamespaceEx(Params);
	// 	// 									}
	// 	// 								}
	// 	// 							}
	// 	// 						}
	// 	// 					}
	// 	// 				}
	// 	//
	// 	// 				HoveredEventNode->ReconstructNode();
	// 	// 				HoveredNode->UpdateGraphNode();
	// 	// 			}
	// 	// 		}
	// 	// 	}
	// 	// }
	// }
// 	return false;
// }
