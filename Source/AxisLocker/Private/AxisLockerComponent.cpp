/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AxisLockerComponent.h"
#include "AxisLockLibrary.h"
#include "AxisLockerAPI.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "XToolsErrorReporter.h"

UAxisLockerComponent::UAxisLockerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAxisLockerComponent::SetTargetComponent(UPrimitiveComponent* InTarget)
{
	TargetComponentOverride = InTarget;
}

UPrimitiveComponent* UAxisLockerComponent::ResolveTarget() const
{
	// 1) 运行时覆盖优先
	if (TargetComponentOverride.IsValid())
	{
		return TargetComponentOverride.Get();
	}

	// 2) 编辑器显式引用
	if (const AActor* Owner = GetOwner())
	{
		if (UPrimitiveComponent* Resolved = Cast<UPrimitiveComponent>(TargetComponentRef.GetComponent(const_cast<AActor*>(Owner))))
		{
			return Resolved;
		}
	}

	// 3) 回退到挂载父级（与参考插件等价）
	return Cast<UPrimitiveComponent>(GetAttachParent());
}

void UAxisLockerComponent::ApplyConfiguredLock()
{
	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("轴向锁定组件：未能解析到目标物理组件（请挂到 PrimitiveComponent 下或设置目标组件）"), NAME_None, true, 5.0f);
		return;
	}

	if (Preset != EAxisLockPreset::None)
	{
		UAxisLockLibrary::ApplyPreset(Target, Preset);
	}
	else
	{
		UAxisLockLibrary::LockAxes(Target,
			bLockPositionX, bLockPositionY, bLockPositionZ,
			bLockRotationX, bLockRotationY, bLockRotationZ);
	}
}

void UAxisLockerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoApplyOnBeginPlay)
	{
		ApplyConfiguredLock();
	}
}

void UAxisLockerComponent::PushLockState()
{
	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("压入锁定状态失败：未能解析到目标物理组件"), NAME_None, true, 5.0f);
		return;
	}

	LockStateStack.Push(UAxisLockLibrary::GetLockState(Target));
}

void UAxisLockerComponent::PopLockState()
{
	if (LockStateStack.Num() == 0)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("弹出锁定状态失败：状态栈为空"), NAME_None, true, 5.0f);
		return;
	}

	const FAxisLockState State = LockStateStack.Pop();

	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("恢复锁定状态失败：未能解析到目标物理组件"), NAME_None, true, 5.0f);
		return;
	}

	UAxisLockLibrary::LockAxes(Target,
		State.bLockPositionX, State.bLockPositionY, State.bLockPositionZ,
		State.bLockRotationX, State.bLockRotationY, State.bLockRotationZ);
}
