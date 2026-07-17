// Copyright fpwong. All Rights Reserved.

#pragma once

#include "BlueprintAssistMisc/BAControlRigUtils.h"

class URigVMPin;
class URigVMNode;
class UEdGraphNode;
class URigVMEdGraphSchema;
class UEdGraph;
class UEdGraphPin;
class URigVMController;

struct FBAControlRigUtils
{
public:
	static bool IsControlRigGraph(UEdGraph* Graph);
	static URigVMPin* GetVMPin(UEdGraphPin* Pin);
	static bool IsVMPinVisible(URigVMPin* Pin);
	static URigVMNode* GetVMNode(UEdGraphNode* Node);
	static URigVMController* GetVMController(UEdGraphPin* Pin);
	static URigVMController* GetVMController(UEdGraph* Graph);
	static const URigVMEdGraphSchema* GetVMSchema(UEdGraphNode* Node);
	static const URigVMEdGraphSchema* GetVMSchema(UEdGraphPin* Pin);
};
