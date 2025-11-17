// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistMisc/BlueprintAssistInputProcessorState.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistInputProcessor.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistTabHandler.h"
#include "BlueprintAssistUtils.h"
#include "ContentBrowserDataSource.h"
#include "IContentBrowserDataModule.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Variable.h"
#include "SGraphActionMenu.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "Editor/ContentBrowser/Private/SContentBrowser.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#if BA_UE_VERSION_OR_LATER(5, 3)
#include "PropertyEditorClipboard.h"
#endif

class UK2Node_CallFunction;
class UK2Node_MacroInstance;

bool FBAInputProcessorState::OnKeyOrMouseDown(const FKey& Key)
{
	if (TryCopyPastePinValue())
	{
		bConsumeMouseUp = true;
		return true;
	}

	if (ProcessContentBrowserInput())
	{
		return true;
	}

	TryFocusInDetailPanel();

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
	const bool bCopy = FBAInputProcessor::Get().IsInputChordDown(UBASettings_EditorFeatures::Get().CopyPinValueChord);
	const bool bPaste = FBAInputProcessor::Get().IsInputChordDown(UBASettings_EditorFeatures::Get().PastePinValueChord);
	if (!bCopy && !bPaste)
	{
		return false;
	}

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

	UEdGraphPin* PinObj  = FBAUtils::GetHoveredPin(GraphPanel);
	if (!PinObj)
	{
		return false;
	}

	// only unlinked input pins are viable for copy paste default value 
	if (PinObj->LinkedTo.Num() > 0 || PinObj->Direction != EGPD_Input)
	{
		return false;
	}

	if (bPaste)
	{
		FString ClipboardValue;
#if BA_UE_VERSION_OR_LATER(5, 3)
		FPropertyEditorClipboard::ClipboardPaste(ClipboardValue);
#else
		FPlatformApplicationMisc::ClipboardPaste(ClipboardValue);
#endif

		if (!ClipboardValue.IsEmpty())
		{
			FScopedTransaction Transaction(INVTEXT("Paste pin value"));
			PinObj->Modify();

			if (FBAUtils::TrySetDefaultPinValuesFromString(PinObj, ClipboardValue))
			{
				const FText Message = INVTEXT("Pasted pin value");
				FNotificationInfo Notification(Message);
				Notification.ExpireDuration = 1.0f;
				FSlateNotificationManager::Get().AddNotification(Notification);

				if (TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel))
				{
					// select the pin
					GraphHandler->SetSelectedPin(PinObj);

					HoveredNode->UpdateGraphNode();
				}
			}
		}

		return true;
	}

	if (bCopy)
	{
		const FString PinDefault = FBAUtils::GetDefaultPinValue(PinObj);
#if BA_UE_VERSION_OR_LATER(5, 3)
		FPropertyEditorClipboard::ClipboardCopy(*PinDefault);
#else
		FPlatformApplicationMisc::ClipboardCopy(*PinDefault);
#endif

		if (!PinDefault.IsEmpty())
		{
			FNotificationInfo Notification(FText::FromString(FString::Printf(TEXT("Copied pin value: %s"), *PinDefault)));
			Notification.ExpireDuration = 1.0f;
			FSlateNotificationManager::Get().AddNotification(Notification);

			// select the pin
			GraphHandler->SetSelectedPin(PinObj);
			return true;
		}
	}

	return false;
}

bool FBAInputProcessorState::TryFocusInDetailPanel()
{
	if (!FBAInputProcessor::Get().IsInputChordDown(UBASettings_EditorFeatures::Get().FocusInDetailsPanelChord))
	{
		return false;
	}

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

	TSharedPtr<SGraphNode> HoveredNode = FBAUtils::GetHoveredGraphNode(GraphPanel);
	if (!HoveredNode)
	{
		return false;
	}

	// get the hovered graph node
	if (UEdGraphNode* NodeObj = HoveredNode->GetNodeObj())
	{
		FName ItemName;

		if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(NodeObj))
		{
			ItemName = VariableNode->GetVarName();
		}
		else if (UK2Node_CallFunction* FunctionCall = Cast<UK2Node_CallFunction>(NodeObj))
		{
			ItemName = FunctionCall->FunctionReference.GetMemberName();
		}
		else if (UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(NodeObj))
		{
			ItemName = Macro->GetMacroGraph()->GetFName();
		}

		if (!ItemName.IsNone())
		{
			if (TSharedPtr<SGraphActionMenu> ActionMenu = FBAUtils::GetGraphActionMenu())
			{
				ActionMenu->SelectItemByName(ItemName, ESelectInfo::OnKeyPress);
				return true;
			}
		}
	}

	return false;
}

bool FBAInputProcessorState::ProcessContentBrowserInput()
{
	TSharedPtr<SContentBrowser> ContentBrowserWidget = FIND_PARENT_WIDGET(FSlateApplication::Get().GetUserFocusedWidget(0), SContentBrowser);
	if (!ContentBrowserWidget.IsValid())
	{
		return false;
	}

	FBAInputProcessor& InputProcessor = FBAInputProcessor::Get();

	// copy: clear the cut state
	if (InputProcessor.IsInputChordDown(FInputChord(EModifierKey::Control, EKeys::C)))
	{
		CutItems.Empty();
		return false;
	}

	// cut
	if (InputProcessor.IsInputChordDown(FInputChord(EModifierKey::Control, EKeys::X)))
	{
		if (CutSelectedAssets())
		{
			// clear whatever is under our clipboard copy
			FPlatformApplicationMisc::ClipboardCopy(TEXT(""));
			return true;
		}
	}

	// paste
	if (InputProcessor.IsInputChordDown(FInputChord(EModifierKey::Control, EKeys::V)))
	{
		if (CutItems.Num())
		{
			FText ErrorMsg(INVTEXT("Unable to paste here"));

			const FContentBrowserItemPath ContentBrowserPath = IContentBrowserSingleton::Get().GetCurrentPath();
			if (ContentBrowserPath.HasInternalPath())
			{
				if (BulkMoveItems(CutItems, ContentBrowserPath.GetVirtualPathName(), &ErrorMsg))
				{
					return true;
				}
			}

			FBAMiscUtils::ShowSimpleSlateNotification(ErrorMsg, SNotificationItem::CS_Fail);
		}
	}

	return false;
}

bool FBAInputProcessorState::CutSelectedAssets()
{
	IContentBrowserSingleton& ContentBrowser = IContentBrowserSingleton::Get();
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	CutItems.Reset();

	// cut folders
	TArray<FString> Folders;

	// if the cursor is on the path view, then cut folders from there instead
	if (TSharedPtr<SWidget> PathView = FBAUtils::GetParentWidgetOfType(FSlateApplication::Get().GetUserFocusedWidget(0), "SPathView"))
	{
		ContentBrowser.GetSelectedPathViewFolders(Folders);
	}
	else
	{
		ContentBrowser.GetSelectedFolders(Folders);
	}

	for (const FString& Folder : Folders)
	{
		FContentBrowserItem BrowserItem = ContentBrowserData->GetItemAtPath(FName(Folder), EContentBrowserItemTypeFilter::IncludeFolders);
		if (BrowserItem.IsValid())
		{
			CutItems.Add(MoveTemp(BrowserItem));
		}
	}

	// cut assets
	TArray<FAssetData> SelectedAssets;
	ContentBrowser.GetSelectedAssets(SelectedAssets);
	for (FAssetData& SelectedAsset : SelectedAssets)
	{
		FName VirtualPath;
		ContentBrowserData->ConvertInternalPathToVirtual(SelectedAsset.GetSoftObjectPath().ToString(), VirtualPath);

		FContentBrowserItem BrowserItem = ContentBrowserData->GetItemAtPath(VirtualPath, EContentBrowserItemTypeFilter::IncludeFiles);
		if (BrowserItem.IsValid())
		{
			CutItems.Add(MoveTemp(BrowserItem));
		}
	}

	return CutItems.Num() > 0;
}

/* Based off DragDropHandler::HandleDragDropMoveOrCopy */
bool FBAInputProcessorState::BulkMoveItems(const TArray<FContentBrowserItem>& InDraggedItems, FName DestPath, FText* OutError)
{
	// Batch these by their data sources
	TMap<UContentBrowserDataSource*, TArray<FContentBrowserItemData>> SourcesAndItems;
	for (const FContentBrowserItem& DraggedItem : InDraggedItems)
	{
		if (DraggedItem.CanMove(DestPath, OutError))
		{
			FContentBrowserItem::FItemDataArrayView ItemDataArray = DraggedItem.GetInternalItems();
			for (const FContentBrowserItemData& ItemData : ItemDataArray)
			{
				if (UContentBrowserDataSource* ItemDataSource = ItemData.GetOwnerDataSource())
				{
					TArray<FContentBrowserItemData>& ItemsForSource = SourcesAndItems.FindOrAdd(ItemDataSource);
					ItemsForSource.Add(ItemData);
				}
			}
		}
	}

	bool bSuccess = false;
	for (const auto& [Source, Items] : SourcesAndItems)
	{
		bSuccess |= Source->BulkMoveItems(Items, DestPath);
	}

	return bSuccess;
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
