/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "K2Nodes/K2Node_MapFindRef.h"
#include "K2Nodes/K2Node_MultiBranch.h"
#include "K2Nodes/K2Node_MultiConditionalSelect.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintExtensions_DetachedNodesRemainEditable,
	"XTools.BlueprintExtensions.Nodes.DetachedNodesRemainEditable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintExtensions_DetachedNodesRemainEditable::RunTest(const FString& Parameters)
{
	UK2Node_MultiBranch* MultiBranch = NewObject<UK2Node_MultiBranch>(GetTransientPackage());
	UEdGraphNode* MultiBranchGraphNode = MultiBranch;
	MultiBranchGraphNode->AllocateDefaultPins();
	const int32 InitialBranchCount = MultiBranch->GetCasePinCount();
	MultiBranch->AddCasePinLast();
	TestEqual(TEXT("孤立多分支节点应能增加 Case"),
		MultiBranch->GetCasePinCount(), InitialBranchCount + 1);

	UK2Node_MultiConditionalSelect* MultiSelect =
		NewObject<UK2Node_MultiConditionalSelect>(GetTransientPackage());
	UEdGraphNode* MultiSelectGraphNode = MultiSelect;
	MultiSelectGraphNode->AllocateDefaultPins();
	const int32 InitialSelectCount = MultiSelect->GetCasePinCount();
	MultiSelect->AddCasePinLast();
	TestEqual(TEXT("孤立多条件选择节点应能增加 Case"),
		MultiSelect->GetCasePinCount(), InitialSelectCount + 1);

	UK2Node_MapFindRef* MapFind = NewObject<UK2Node_MapFindRef>(GetTransientPackage());
	MapFind->AllocateDefaultPins();
	MapFind->PostReconstructNode();
	TestNotNull(TEXT("孤立 Map 查找节点重建后应保留 Map 引脚"), MapFind->GetMapPin());
	TestNotNull(TEXT("孤立 Map 查找节点重建后应保留 Value 引脚"), MapFind->GetValuePin());
	return true;
}

#endif
