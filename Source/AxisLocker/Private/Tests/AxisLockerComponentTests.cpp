/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AxisLockLibrary.h"
#include "AxisLockerComponent.h"
#include "AxisLockerComponentTestTypes.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/Package.h"

namespace AxisLockerComponentTests
{
	/** 构造未注册、无 World 的测试组件（ResolveTarget 仅依赖 GetOwner/GetComponents/GetAttachParent）。*/
	UAxisLockerComponent* NewLocker(AActor* Owner)
	{
		UAxisLockerComponent* Locker = NewObject<UAxisLockerComponent>(Owner, TEXT("AxisLocker"));
		Owner->AddOwnedComponent(Locker);
		return Locker;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAxisLockerComponent_ResolveTargetStatusIsDeterministic,
	"XTools.AxisLocker.Component.ResolveTargetStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAxisLockerComponent_ResolveTargetStatusIsDeterministic::RunTest(const FString& Parameters)
{
	using namespace AxisLockerComponentTests;

	AActor* Actor = NewObject<AActor>(GetTransientPackage());
	UAxisLockerComponent* Locker = NewLocker(Actor);
	UPrimitiveComponent* OutTarget = nullptr;

	// 1) 无覆盖、无名称、无挂载父级
	EAxisLockTargetStatus Status = Locker->GetTargetResolveStatus(OutTarget);
	TestTrue(TEXT("无覆盖无名称无父级应为 NoTargetAvailable"),
		Status == EAxisLockTargetStatus::NoTargetAvailable);
	TestNull(TEXT("无可用目标时 OutTarget 应为空"), OutTarget);

	// 2) 名称不存在
	Locker->TargetComponentName = TEXT("MissingComponent");
	Status = Locker->GetTargetResolveStatus(OutTarget);
	TestTrue(TEXT("名称不存在应为 NameNotFound"),
		Status == EAxisLockTargetStatus::NameNotFound);
	TestNull(TEXT("名称未找到时 OutTarget 应为空"), OutTarget);

	// 3) 名称命中但无 BodyInstance
	UAxisLockerTestNullBodyComponent* NullBody = NewObject<UAxisLockerTestNullBodyComponent>(Actor, TEXT("NullBodyMesh"));
	Actor->AddOwnedComponent(NullBody);
	Locker->TargetComponentName = TEXT("NullBodyMesh");
	Status = Locker->GetTargetResolveStatus(OutTarget);
	TestTrue(TEXT("名称命中但无 BodyInstance 应为 NoBodyInstance"),
		Status == EAxisLockTargetStatus::NoBodyInstance);
	TestTrue(TEXT("NoBodyInstance 时 OutTarget 应为命中的组件"),
		OutTarget == static_cast<UPrimitiveComponent*>(NullBody));

	// 4) 正常目标（名称命中且有 BodyInstance）
	UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("BoxMesh"));
	Actor->AddOwnedComponent(Box);
	Locker->TargetComponentName = TEXT("BoxMesh");
	Status = Locker->GetTargetResolveStatus(OutTarget);
	TestTrue(TEXT("名称命中且有 BodyInstance 应为 Ready"),
		Status == EAxisLockTargetStatus::Ready);
	TestTrue(TEXT("Ready 时 OutTarget 应为命中的组件"),
		OutTarget == static_cast<UPrimitiveComponent*>(Box));

	// 5) 有效 TargetComponentOverride 优先于名称
	UBoxComponent* OverrideBox = NewObject<UBoxComponent>(Actor, TEXT("OverrideBox"));
	Actor->AddOwnedComponent(OverrideBox);
	Locker->SetTargetComponent(OverrideBox);
	Status = Locker->GetTargetResolveStatus(OutTarget);
	TestTrue(TEXT("有效覆盖应为 Ready"),
		Status == EAxisLockTargetStatus::Ready);
	TestTrue(TEXT("覆盖生效时 OutTarget 应为覆盖组件而非名称命中组件"),
		OutTarget == static_cast<UPrimitiveComponent*>(OverrideBox));

	// 6) 覆盖失效：pending-kill 后弱引用失效，回退到名称目标并报告 TargetOverrideInvalid
	OverrideBox->MarkAsGarbage();
	Status = Locker->GetTargetResolveStatus(OutTarget);
	TestTrue(TEXT("覆盖失效应为 TargetOverrideInvalid"),
		Status == EAxisLockTargetStatus::TargetOverrideInvalid);
	TestTrue(TEXT("覆盖失效时 OutTarget 应为回退解析结果（名称命中的 Box）"),
		OutTarget == static_cast<UPrimitiveComponent*>(Box));

	// 7) 无名称但挂载父级是 PrimitiveComponent：回退解析 Ready
	AActor* ParentActor = NewObject<AActor>(GetTransientPackage());
	UBoxComponent* ParentBox = NewObject<UBoxComponent>(ParentActor, TEXT("ParentBox"));
	ParentActor->AddOwnedComponent(ParentBox);
	UAxisLockerComponent* AttachedLocker = NewLocker(ParentActor);
	AttachedLocker->SetupAttachment(ParentBox);
	Status = AttachedLocker->GetTargetResolveStatus(OutTarget);
	TestTrue(TEXT("挂载父级为 PrimitiveComponent 时应回退为 Ready"),
		Status == EAxisLockTargetStatus::Ready);
	TestTrue(TEXT("回退解析的 OutTarget 应为挂载父级"),
		OutTarget == static_cast<UPrimitiveComponent*>(ParentBox));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAxisLockerComponent_RestoresStateToOriginalTarget,
	"XTools.AxisLocker.Component.RestoresStateToOriginalTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAxisLockerComponent_RestoresStateToOriginalTarget::RunTest(const FString& Parameters)
{
	using namespace AxisLockerComponentTests;

	AActor* Actor = NewObject<AActor>(GetTransientPackage());
	UAxisLockerComponent* Locker = NewLocker(Actor);
	UBoxComponent* OriginalTarget = NewObject<UBoxComponent>(Actor, TEXT("OriginalTarget"));
	UBoxComponent* NewTarget = NewObject<UBoxComponent>(Actor, TEXT("NewTarget"));
	Actor->AddOwnedComponent(OriginalTarget);
	Actor->AddOwnedComponent(NewTarget);

	FBodyInstance* OriginalBody = OriginalTarget->GetBodyInstance();
	FBodyInstance* NewBody = NewTarget->GetBodyInstance();
	if (!TestNotNull(TEXT("Original target body must exist"), OriginalBody)
		|| !TestNotNull(TEXT("New target body must exist"), NewBody))
	{
		return false;
	}

	OriginalBody->bLockXTranslation = true;
	OriginalBody->bLockYTranslation = false;
	OriginalBody->DOFMode = EDOFMode::None;
	Locker->SetTargetComponent(OriginalTarget);
	Locker->PushLockState();

	OriginalBody->bLockXTranslation = false;
	NewBody->bLockXTranslation = false;
	NewBody->bLockYTranslation = true;
	Locker->SetTargetComponent(NewTarget);
	AddExpectedError(TEXT("目标组件未开启物理模拟"), EAutomationExpectedErrorFlags::Contains, 2);
	Locker->PopLockState();

	TestTrue(TEXT("Saved state must be restored to the original target"), OriginalBody->bLockXTranslation);
	TestFalse(TEXT("The original target must restore its saved Y state"), OriginalBody->bLockYTranslation);
	TestFalse(TEXT("The new target must not receive the original target X state"), NewBody->bLockXTranslation);
	TestTrue(TEXT("The new target must keep its own Y state"), NewBody->bLockYTranslation);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
