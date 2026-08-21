/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ComponentTimelineLibrary.h"
#include "Components/TimelineComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComponentTimeline_BindDecisionHandlesNullValidAndStale,
	"XTools.ComponentTimeline.BindDecision.NullValidAndStale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FComponentTimeline_BindDecisionHandlesNullValidAndStale::RunTest(const FString& Parameters)
{
	using ComponentTimeline::ETimelineBindDecision;
	using ComponentTimeline::ResolveTimelineBindDecision;

	// 空指针：允许创建并绑定新时间轴
	TestTrue(TEXT("空属性值应判定为创建"),
		ResolveTimelineBindDecision(nullptr) == ETimelineBindDecision::Create);

	// 有效时间轴组件：重复初始化幂等跳过，不重建
	UTimelineComponent* LiveTimeline = NewObject<UTimelineComponent>(GetTransientPackage());
	TestTrue(TEXT("前置条件：新建的时间轴组件应有效"), IsValid(LiveTimeline));
	TestTrue(TEXT("有效时间轴应判定为幂等跳过"),
		ResolveTimelineBindDecision(LiveTimeline) == ETimelineBindDecision::Skip);

	// 失效（pending-kill/垃圾）时间轴：先清空属性再允许重新创建绑定
	LiveTimeline->MarkAsGarbage();
	TestFalse(TEXT("前置条件：标记垃圾后时间轴应失效"), IsValid(LiveTimeline));
	TestTrue(TEXT("失效时间轴应判定为清空后重建"),
		ResolveTimelineBindDecision(LiveTimeline) == ETimelineBindDecision::RebindStale);

	return true;
}

#endif
