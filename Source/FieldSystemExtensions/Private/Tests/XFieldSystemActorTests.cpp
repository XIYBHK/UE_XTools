/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "XFieldSystemActor.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FXFieldSystemActor_FiltersActorsByClassAndTag,
	"XTools.FieldSystem.Actor.FiltersActorsByClassAndTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXFieldSystemActor_FiltersActorsByClassAndTag::RunTest(const FString& Parameters)
{
	AXFieldSystemActor* FieldActor = NewObject<AXFieldSystemActor>(GetTransientPackage());
	AActor* TargetActor = NewObject<AActor>(GetTransientPackage());
	TestNotNull(TEXT("测试用Field Actor应创建成功"), FieldActor);
	TestNotNull(TEXT("测试用目标Actor应创建成功"), TargetActor);
	if (!FieldActor || !TargetActor)
	{
		return false;
	}

	TestTrue(TEXT("未启用筛选时应影响所有Actor"), FieldActor->ShouldAffectActor(TargetActor));
	TestTrue(TEXT("空Actor输入在未配置筛选时应安全通过"), FieldActor->ShouldAffectActor(nullptr));

	FieldActor->bEnableFiltering = true;
	FieldActor->bEnableActorClassFilter = true;
	FieldActor->ExcludeActorClasses = { AActor::StaticClass() };
	TestFalse(TEXT("排除类应拒绝其子类Actor"), FieldActor->ShouldAffectActor(TargetActor));

	FieldActor->ExcludeActorClasses.Reset();
	FieldActor->IncludeActorClasses = { AActor::StaticClass() };
	TestTrue(TEXT("包含类应接受匹配Actor"), FieldActor->ShouldAffectActor(TargetActor));

	FieldActor->bEnableActorClassFilter = false;
	FieldActor->bEnableActorTagFilter = true;
	FieldActor->IncludeActorTags = { TEXT("Affected") };
	TestFalse(TEXT("缺少包含标签时应拒绝Actor"), FieldActor->ShouldAffectActor(TargetActor));
	TargetActor->Tags.Add(TEXT("Affected"));
	TestTrue(TEXT("匹配包含标签时应接受Actor"), FieldActor->ShouldAffectActor(TargetActor));
	FieldActor->ExcludeActorTags = { TEXT("Affected") };
	TestFalse(TEXT("排除标签应优先拒绝Actor"), FieldActor->ShouldAffectActor(TargetActor));

	return true;
}

#endif
