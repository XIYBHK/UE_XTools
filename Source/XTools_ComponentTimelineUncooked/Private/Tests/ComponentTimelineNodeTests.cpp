/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "K2Node_ComponentTimeline.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComponentTimeline_DetachedNodeIsSafe,
	"XTools.ComponentTimeline.Editor.DetachedNodeIsSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComponentTimeline_DetachedNodeIsSafe::RunTest(const FString& Parameters)
{
	UK2Node_ComponentTimeline* Node = NewObject<UK2Node_ComponentTimeline>(GetTransientPackage());
	if (!TestNotNull(TEXT("组件时间轴节点应创建成功"), Node))
	{
		return false;
	}

	Node->AllocateDefaultPins();
	TestTrue(TEXT("孤立节点仍应分配基础引脚"), Node->Pins.Num() > 0);
	TestFalse(TEXT("孤立节点标题不应为空"), Node->GetNodeTitle(ENodeTitleType::ListView).IsEmpty());
	TestNull(TEXT("孤立节点不应提供时间轴跳转目标"), Node->GetJumpTargetForDoubleClick());

	Node->PreloadRequiredAssets();
	Node->PrepareForCopying();
	return true;
}

#endif
