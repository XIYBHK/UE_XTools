/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AxisLockerComponent.h"
#include "AxisLockLibrary.h"
#include "AxisLockerAPI.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "XToolsErrorReporter.h"
#include "XToolsVersionCompat.h"

UAxisLockerComponent::UAxisLockerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAxisLockerComponent::SetTargetComponent(UPrimitiveComponent* InTarget)
{
	TargetComponentOverride = InTarget;
	bTargetOverrideWasSet = (InTarget != nullptr);
}

UPrimitiveComponent* UAxisLockerComponent::ResolveTargetWithStatus(EAxisLockTargetStatus& OutStatus) const
{
	// 1) 运行时覆盖优先
	if (TargetComponentOverride.IsValid())
	{
		UPrimitiveComponent* OverrideTarget = TargetComponentOverride.Get();
		OutStatus = OverrideTarget->GetBodyInstance()
			? EAxisLockTargetStatus::Ready
			: EAxisLockTargetStatus::NoBodyInstance;
		return OverrideTarget;
	}

	// 2) 编辑器显式目标名称（名称已设置但未命中时不回退挂载父级，与既有行为一致）
	if (!TargetComponentName.IsNone())
	{
		UPrimitiveComponent* NamedTarget = nullptr;
		if (AActor* Owner = GetOwner())
		{
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
			for (UPrimitiveComponent* Component : PrimitiveComponents)
			{
				if (IsValid(Component) && Component->GetFName() == TargetComponentName)
				{
					NamedTarget = Component;
					break;
				}
			}
		}

		if (!NamedTarget)
		{
			OutStatus = EAxisLockTargetStatus::NameNotFound;
			return nullptr;
		}

		OutStatus = bTargetOverrideWasSet
			? EAxisLockTargetStatus::TargetOverrideInvalid
			: (NamedTarget->GetBodyInstance() ? EAxisLockTargetStatus::Ready : EAxisLockTargetStatus::NoBodyInstance);
		return NamedTarget;
	}

	// 3) 回退到挂载父级（与参考插件等价）
	UPrimitiveComponent* ParentTarget = Cast<UPrimitiveComponent>(GetAttachParent());
	if (!ParentTarget)
	{
		OutStatus = bTargetOverrideWasSet
			? EAxisLockTargetStatus::TargetOverrideInvalid
			: EAxisLockTargetStatus::NoTargetAvailable;
		return nullptr;
	}

	OutStatus = bTargetOverrideWasSet
		? EAxisLockTargetStatus::TargetOverrideInvalid
		: (ParentTarget->GetBodyInstance() ? EAxisLockTargetStatus::Ready : EAxisLockTargetStatus::NoBodyInstance);
	return ParentTarget;
}

UPrimitiveComponent* UAxisLockerComponent::ResolveTarget() const
{
	EAxisLockTargetStatus Status = EAxisLockTargetStatus::NoTargetAvailable;
	UPrimitiveComponent* Target = ResolveTargetWithStatus(Status);

	// 保留既有行为：指定了名称但未命中时输出 Warning（含屏幕提示）
	if (Status == EAxisLockTargetStatus::NameNotFound)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, FString::Printf(TEXT("轴向锁定组件：指定目标组件不存在（%s）"), *TargetComponentName.ToString()), NAME_None, true, 5.0f);
	}

	return Target;
}

EAxisLockTargetStatus UAxisLockerComponent::GetTargetResolveStatus(UPrimitiveComponent*& OutTarget) const
{
	EAxisLockTargetStatus Status = EAxisLockTargetStatus::NoTargetAvailable;
	OutTarget = ResolveTargetWithStatus(Status);
	return Status;
}

TArray<FString> UAxisLockerComponent::GetTargetComponentNameOptions() const
{
	TArray<FString> Options;
	Options.Add(FName(NAME_None).ToString());

	auto AddComponentOption = [&Options](const UActorComponent* Component, const FName PreferredName = NAME_None)
	{
		if (!Component || !Component->IsA<UPrimitiveComponent>())
		{
			return;
		}

		const FName OptionName = PreferredName.IsNone() ? Component->GetFName() : PreferredName;
		if (!OptionName.IsNone())
		{
			Options.AddUnique(OptionName.ToString());
		}
	};

	if (const AActor* Owner = GetOwner())
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (const UPrimitiveComponent* Component : PrimitiveComponents)
		{
			AddComponentOption(Component);
		}
	}

	const UBlueprintGeneratedClass* BlueprintClass = nullptr;
	for (const UObject* Outer = this; Outer && !BlueprintClass; Outer = Outer->GetOuter())
	{
		if (const UBlueprintGeneratedClass* OuterBlueprintClass = Cast<UBlueprintGeneratedClass>(Outer))
		{
			BlueprintClass = OuterBlueprintClass;
		}
		else if (const USCS_Node* Node = Cast<USCS_Node>(Outer))
		{
			BlueprintClass = Cast<UBlueprintGeneratedClass>(Node->GetSCS()->GetOwnerClass());
		}
		else if (const USimpleConstructionScript* SimpleConstructionScript = Cast<USimpleConstructionScript>(Outer))
		{
			BlueprintClass = Cast<UBlueprintGeneratedClass>(SimpleConstructionScript->GetOwnerClass());
		}
	}

	if (BlueprintClass)
	{
		if (const USimpleConstructionScript* SimpleConstructionScript = BlueprintClass->SimpleConstructionScript)
		{
			for (const USCS_Node* Node : SimpleConstructionScript->GetAllNodes())
			{
				AddComponentOption(Node ? Node->ComponentTemplate.Get() : nullptr, Node ? Node->GetVariableName() : NAME_None);
			}
		}
	}

	return Options;
}

void UAxisLockerComponent::ApplyConfiguredLock()
{
	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		if (TargetComponentName.IsNone())
		{
			XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("轴向锁定组件：未能解析到目标物理组件（请挂到 PrimitiveComponent 下或设置目标组件）"), NAME_None, true, 5.0f);
		}
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
		if (TargetComponentName.IsNone())
		{
			XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("压入锁定状态失败：未能解析到目标物理组件"), NAME_None, true, 5.0f);
		}
		return;
	}

	if (!Target->GetBodyInstance())
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("压入锁定状态失败：目标组件没有有效的 BodyInstance"), NAME_None, true, 5.0f);
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

	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		if (TargetComponentName.IsNone())
		{
			XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("恢复锁定状态失败：未能解析到目标物理组件"), NAME_None, true, 5.0f);
		}
		return;
	}

	const FAxisLockState State = LockStateStack.Last();
	if (UAxisLockLibrary::ApplyLockState(Target, State))
	{
#if XTOOLS_ENGINE_5_8_OR_LATER
		LockStateStack.Pop(EAllowShrinking::No);
#else
		LockStateStack.Pop(false);
#endif
	}
}
