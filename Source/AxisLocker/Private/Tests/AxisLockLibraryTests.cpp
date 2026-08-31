/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AxisLockLibrary.h"

#include "Components/BoxComponent.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodyInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAxisLockLibrary_MapsPresetsToExpectedAxes,
	"XTools.AxisLocker.Library.MapsPresetsToExpectedAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAxisLockLibrary_MapsPresetsToExpectedAxes::RunTest(const FString& Parameters)
{
	const FAxisLockState None = UAxisLockLibrary::PresetToState(EAxisLockPreset::None);
	TestFalse(TEXT("无预设不应锁定任何轴"),
		None.bLockPositionX || None.bLockPositionY || None.bLockPositionZ ||
		None.bLockRotationX || None.bLockRotationY || None.bLockRotationZ);

	const FAxisLockState StayUpright = UAxisLockLibrary::PresetToState(EAxisLockPreset::StayUpright);
	TestTrue(TEXT("站立不倒应锁定X/Y旋转"), StayUpright.bLockRotationX && StayUpright.bLockRotationY);
	TestFalse(TEXT("站立不倒不应锁定Z旋转或位移"),
		StayUpright.bLockRotationZ || StayUpright.bLockPositionX || StayUpright.bLockPositionY || StayUpright.bLockPositionZ);

	const FAxisLockState YawOnly = UAxisLockLibrary::PresetToState(EAxisLockPreset::YawOnly);
	TestTrue(TEXT("只绕Z轴应锁定全部位移和X/Y旋转"),
		YawOnly.bLockPositionX && YawOnly.bLockPositionY && YawOnly.bLockPositionZ &&
		YawOnly.bLockRotationX && YawOnly.bLockRotationY);
	TestFalse(TEXT("只绕Z轴不应锁定Z旋转"), YawOnly.bLockRotationZ);

	const FAxisLockState HorizontalOnly = UAxisLockLibrary::PresetToState(EAxisLockPreset::HorizontalOnly);
	TestTrue(TEXT("只能水平移动应锁定Z位移"), HorizontalOnly.bLockPositionZ);
	TestFalse(TEXT("只能水平移动不应锁定其他轴"),
		HorizontalOnly.bLockPositionX || HorizontalOnly.bLockPositionY ||
		HorizontalOnly.bLockRotationX || HorizontalOnly.bLockRotationY || HorizontalOnly.bLockRotationZ);

	const FAxisLockState FreezeAll = UAxisLockLibrary::PresetToState(EAxisLockPreset::FreezeAll);
	TestTrue(TEXT("完全冻结应锁定全部六轴"),
		FreezeAll.bLockPositionX && FreezeAll.bLockPositionY && FreezeAll.bLockPositionZ &&
		FreezeAll.bLockRotationX && FreezeAll.bLockRotationY && FreezeAll.bLockRotationZ);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAxisLockLibrary_InvalidTargetHasNoLockedAxes,
	"XTools.AxisLocker.Library.InvalidTargetHasNoLockedAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAxisLockLibrary_InvalidTargetHasNoLockedAxes::RunTest(const FString& Parameters)
{
	const FAxisLockState State = UAxisLockLibrary::GetLockState(nullptr);
	TestFalse(TEXT("无效组件查询应返回未锁定状态"),
		State.bLockPositionX || State.bLockPositionY || State.bLockPositionZ ||
		State.bLockRotationX || State.bLockRotationY || State.bLockRotationZ);
	TestFalse(TEXT("无效组件不应报告任意轴被锁定"), UAxisLockLibrary::IsAnyAxisLocked(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAxisLockLibrary_NoneModeIgnoresStaleAxisFlags,
	"XTools.AxisLocker.Library.NoneModeIgnoresStaleAxisFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAxisLockLibrary_NoneModeIgnoresStaleAxisFlags::RunTest(const FString& Parameters)
{
	UBoxComponent* Target = NewObject<UBoxComponent>();
	FBodyInstance* BodyInstance = Target->GetBodyInstance();
	if (!TestNotNull(TEXT("测试组件应具有BodyInstance"), BodyInstance))
	{
		return false;
	}

	BodyInstance->bLockXTranslation = true;
	BodyInstance->DOFMode = EDOFMode::SixDOF;
	TestTrue(TEXT("SixDOF模式应报告遗留X位移锁"), UAxisLockLibrary::IsAnyAxisLocked(Target));

	BodyInstance->DOFMode = EDOFMode::None;
	TestFalse(TEXT("None模式应忽略未清理的六轴标志"), UAxisLockLibrary::IsAnyAxisLocked(Target));

	return true;
}

#endif
