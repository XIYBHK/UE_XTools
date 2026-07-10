/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AxisLockLibrary.h"
#include "AxisLockerAPI.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "XToolsErrorReporter.h"

FBodyInstance* UAxisLockLibrary::GetValidBodyInstance(UPrimitiveComponent* Target)
{
	if (!IsValid(Target))
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("轴向锁定失败：目标组件为空或无效"), NAME_None, true, 5.0f);
		return nullptr;
	}

	FBodyInstance* BodyInstance = Target->GetBodyInstance();
	if (!BodyInstance)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("轴向锁定失败：目标组件没有有效的 BodyInstance"), NAME_None, true, 5.0f);
		return nullptr;
	}

	if (!Target->IsSimulatingPhysics())
	{
		// 仅警告，不拦截：开启物理模拟后锁定即生效
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("目标组件未开启物理模拟，轴向锁定对其当前无效（开启模拟后生效）"), NAME_None, true, 5.0f);
	}

	return BodyInstance;
}

void UAxisLockLibrary::LockAxes(
	UPrimitiveComponent* Target,
	bool bLockPosX, bool bLockPosY, bool bLockPosZ,
	bool bLockRotX, bool bLockRotY, bool bLockRotZ)
{
	FBodyInstance* BodyInstance = GetValidBodyInstance(Target);
	if (!BodyInstance)
	{
		return;
	}

	BodyInstance->bLockXTranslation = bLockPosX;
	BodyInstance->bLockYTranslation = bLockPosY;
	BodyInstance->bLockZTranslation = bLockPosZ;
	BodyInstance->bLockXRotation = bLockRotX;
	BodyInstance->bLockYRotation = bLockRotY;
	BodyInstance->bLockZRotation = bLockRotZ;
	BodyInstance->bLockTranslation = false;
	BodyInstance->bLockRotation = false;
	BodyInstance->CustomDOFPlaneNormal = FVector::ZeroVector;

	// SixDOF 是表达任意轴组合的唯一模式；内部会 Term 旧约束并按上面 6 个开关重建
	BodyInstance->SetDOFLock(EDOFMode::SixDOF);
}

bool UAxisLockLibrary::ApplyLockState(UPrimitiveComponent* Target, const FAxisLockState& State)
{
	FBodyInstance* BodyInstance = GetValidBodyInstance(Target);
	if (!BodyInstance)
	{
		return false;
	}

	BodyInstance->bLockTranslation = State.bLockPlaneTranslation;
	BodyInstance->bLockRotation = State.bLockPlaneRotation;
	BodyInstance->bLockXTranslation = State.bLockPositionX;
	BodyInstance->bLockYTranslation = State.bLockPositionY;
	BodyInstance->bLockZTranslation = State.bLockPositionZ;
	BodyInstance->bLockXRotation = State.bLockRotationX;
	BodyInstance->bLockYRotation = State.bLockRotationY;
	BodyInstance->bLockZRotation = State.bLockRotationZ;
	BodyInstance->CustomDOFPlaneNormal = State.CustomDOFPlaneNormal;

	BodyInstance->SetDOFLock(State.DOFMode);
	return true;
}

void UAxisLockLibrary::UnlockAll(UPrimitiveComponent* Target)
{
	// 6 轴全 false + SetDOFLock：CreateDOFLock 检测到全 false 时不创建约束，等于解除
	LockAxes(Target, false, false, false, false, false, false);
}

FAxisLockState UAxisLockLibrary::GetLockState(UPrimitiveComponent* Target)
{
	FAxisLockState State; // 默认全 false

	// 查询不修改状态，无需 IsSimulatingPhysics 校验；目标无效直接返回全 false
	if (!IsValid(Target))
	{
		return State;
	}

	FBodyInstance* BodyInstance = Target->GetBodyInstance();
	if (!BodyInstance)
	{
		return State;
	}

	State.bLockPositionX = BodyInstance->bLockXTranslation;
	State.bLockPositionY = BodyInstance->bLockYTranslation;
	State.bLockPositionZ = BodyInstance->bLockZTranslation;
	State.bLockRotationX = BodyInstance->bLockXRotation;
	State.bLockRotationY = BodyInstance->bLockYRotation;
	State.bLockRotationZ = BodyInstance->bLockZRotation;
	State.bLockPlaneTranslation = BodyInstance->bLockTranslation;
	State.bLockPlaneRotation = BodyInstance->bLockRotation;
	State.DOFMode = BodyInstance->DOFMode;
	State.CustomDOFPlaneNormal = BodyInstance->CustomDOFPlaneNormal;
	return State;
}

bool UAxisLockLibrary::IsAnyAxisLocked(UPrimitiveComponent* Target)
{
	const FAxisLockState State = GetLockState(Target);
	const EDOFMode::Type ResolvedDOFMode = FBodyInstance::ResolveDOFMode(State.DOFMode);
	if (ResolvedDOFMode == EDOFMode::YZPlane
		|| ResolvedDOFMode == EDOFMode::XZPlane
		|| ResolvedDOFMode == EDOFMode::XYPlane
		|| (ResolvedDOFMode == EDOFMode::CustomPlane && (State.bLockPlaneTranslation || State.bLockPlaneRotation)))
	{
		return true;
	}

	return State.bLockPositionX || State.bLockPositionY || State.bLockPositionZ
		|| State.bLockRotationX || State.bLockRotationY || State.bLockRotationZ;
}

FAxisLockState UAxisLockLibrary::PresetToState(EAxisLockPreset Preset)
{
	FAxisLockState State; // 默认全 false（含 None）

	switch (Preset)
	{
	case EAxisLockPreset::StayUpright:
		State.bLockRotationX = true;
		State.bLockRotationY = true;
		break;

	case EAxisLockPreset::YawOnly:
		State.bLockPositionX = true;
		State.bLockPositionY = true;
		State.bLockPositionZ = true;
		State.bLockRotationX = true;
		State.bLockRotationY = true;
		break;

	case EAxisLockPreset::HorizontalOnly:
		State.bLockPositionZ = true;
		break;

	case EAxisLockPreset::FreezeAll:
		State.bLockPositionX = true;
		State.bLockPositionY = true;
		State.bLockPositionZ = true;
		State.bLockRotationX = true;
		State.bLockRotationY = true;
		State.bLockRotationZ = true;
		break;

	case EAxisLockPreset::None:
	default:
		break;
	}

	return State;
}

void UAxisLockLibrary::ApplyPreset(UPrimitiveComponent* Target, EAxisLockPreset Preset)
{
	const FAxisLockState State = PresetToState(Preset);
	LockAxes(Target,
		State.bLockPositionX, State.bLockPositionY, State.bLockPositionZ,
		State.bLockRotationX, State.bLockRotationY, State.bLockRotationZ);
}
