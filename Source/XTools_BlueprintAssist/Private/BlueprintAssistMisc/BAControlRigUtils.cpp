// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistMisc/BAControlRigUtils.h"

#include "NiagaraEditorCommon.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "EdGraph/RigVMEdGraphSchema.h"

bool FBAControlRigUtils::IsControlRigGraph(UEdGraph* Graph)
{
	return Graph ? Graph->IsA<URigVMEdGraph>() : false;
}

URigVMPin* FBAControlRigUtils::GetVMPin(UEdGraphPin* Pin)
{
	if (Pin)
	{
		if (URigVMEdGraphNode* Node = Cast<URigVMEdGraphNode>(Pin->GetOwningNode()))
		{
			return Node->FindModelPinFromGraphPin(Pin);
		}
	}

	return nullptr;
}

bool FBAControlRigUtils::IsVMPinVisible(URigVMPin* Pin)
{
	URigVMPin* Parent = Pin->GetParentPin();
	while (Parent)
	{
		if (!Parent->IsExpanded())
		{
			return false;
		}

		Parent = Parent->GetParentPin();
	}

	return true;
}

URigVMNode* FBAControlRigUtils::GetVMNode(UEdGraphNode* Node)
{
	if (const URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(Node))
	{
		return RigNode->GetModelNode();
	}

	return nullptr;
}

URigVMController* FBAControlRigUtils::GetVMController(UEdGraphPin* Pin)
{
	if (Pin)
	{
		if (URigVMEdGraphNode* Node = Cast<URigVMEdGraphNode>(Pin->GetOwningNode()))
		{
			return Node->GetController();
		}
	}

	return nullptr;
}

URigVMController* FBAControlRigUtils::GetVMController(UEdGraph* Graph)
{
	if (URigVMEdGraph* VMGraph = Cast<URigVMEdGraph>(Graph))
	{
		return VMGraph->GetController();
	}

	return nullptr;
}

const URigVMEdGraphSchema* FBAControlRigUtils::GetVMSchema(UEdGraphNode* Node)
{
	return Node ? Cast<URigVMEdGraphSchema>(Node->GetSchema()) : nullptr;
}

const URigVMEdGraphSchema* FBAControlRigUtils::GetVMSchema(UEdGraphPin* Pin)
{
	return Pin ? GetVMSchema(Pin->GetOwningNode()) : nullptr;
}