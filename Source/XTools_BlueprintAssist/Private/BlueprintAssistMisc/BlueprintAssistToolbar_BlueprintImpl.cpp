#include "BlueprintAssistMisc/BlueprintAssistToolbar_BlueprintImpl.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_Knot.h"
#include "MessageLogModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Modules/ModuleManager.h"

void FBAToolbar_BlueprintImpl::DetectUnusedNodes()
{
	struct FLocal
	{
		static void DeleteNode(const TSharedRef<IMessageToken>& Token)
		{
			const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(Token);
			if (UObjectToken->GetObject().IsValid())
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(UObjectToken->GetObject().Get()))
				{
					FScopedTransaction Transaction(INVTEXT("Delete Unused Node"));
					FBAUtils::DeleteNode(Node);
				}
			}
		}

		static void JumpToNode(const TSharedRef<IMessageToken>& Token)
		{
			const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(Token);
			if (UObjectToken->GetObject().IsValid())
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(UObjectToken->GetObject().Get()))
				{
					FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node, false);
				}
			}
		}
	};
	
	if (TSharedPtr<FBAGraphHandler> GraphHandler = FBAUtils::GetCurrentGraphHandler())
	{
		UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();

		if (!Graph || FBlueprintEditorUtils::IsGraphReadOnly(Graph))
		{
			return;
		}

		TSet<UEdGraphNode*> UnusedNodes(Graph->Nodes);

		TArray<UEdGraphNode*> EventNodes = Graph->Nodes.FilterByPredicate([](UEdGraphNode* Node) { return FBAUtils::IsEventNode(Node, EGPD_Output); });

		for (UEdGraphNode* RootNode : EventNodes)
		{
			TArray<UEdGraphNode*> ExecTree = FBAUtils::GetExecTree(RootNode, EGPD_Output).Array();
			for (UEdGraphNode* Node : ExecTree)
			{
				// remove the exec nodes from the tree
				UnusedNodes.Remove(Node);

				// find all input parameters and remove them (output params count as unused, if they aren't connected to anything)
				TSet<UEdGraphNode*> InputParams = FBAUtils::GetParameterTree(Node, EGPD_Input);
				for (UEdGraphNode* Param : InputParams)
				{
					UnusedNodes.Remove(Param);
				}
			}
		}

		// check the knot nodes
		TArray<UEdGraphNode*> KnotNodes = Graph->Nodes.FilterByPredicate(FBAUtils::IsKnotNode);
		for (UEdGraphNode* Node : KnotNodes)
		{
			if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(Node))
			{
				// is used if the knot is missing links in the input or output direction, then add it back to the unused nodes
				if (FBAUtils::GetPinLinkedToIgnoringKnots(KnotNode->GetOutputPin()).Num() == 0 ||
					FBAUtils::GetPinLinkedToIgnoringKnots(KnotNode->GetInputPin()).Num() == 0)
				{
					UnusedNodes.Add(KnotNode);
				}
			}
		}

		for (UEdGraphNode* Node : UnusedNodes)
		{
			if (!Node->CanUserDeleteNode() || Node->IsA<UEdGraphNode_Comment>())
			{
				continue;
			}

			FMessageLog MessageLog("BlueprintAssist");
#if BA_UE_VERSION_OR_LATER(5, 1)
			MessageLog.SetCurrentPage(INVTEXT("Unused Nodes"));
#else
			MessageLog.NewPage(INVTEXT("Unused Nodes"));
#endif

			TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Info);

			Message->AddToken(FTextToken::Create(FText::Format(INVTEXT("Unused node {0} {1}"),
				FText::FromString(FBAUtils::GetNodeName(Node)),
				FText::FromString(Node->NodeGuid.ToString())
			)));

			Message->AddToken(FUObjectToken::Create(Node, INVTEXT("Navigate"))
				->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(FLocal::JumpToNode)));

			Message->AddToken(FUObjectToken::Create(Node, INVTEXT("Delete"))
				->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(FLocal::DeleteNode)));

			MessageLog.AddMessage(Message);

			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			MessageLogModule.OpenMessageLog("BlueprintAssist");
		}
	}
}
