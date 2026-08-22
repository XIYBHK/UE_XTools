/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "AxisLockerComponentTestTypes.generated.h"

/**
 * 测试专用：GetBodyInstance 恒为空的 PrimitiveComponent。
 * 默认 UPrimitiveComponent::GetBodyInstance 返回成员地址永不为空，
 * 只有覆写该虚函数才能确定性模拟“目标无 BodyInstance”场景（无需真实物理场景）。
 */
UCLASS()
class AXISLOCKER_API UAxisLockerTestNullBodyComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override { return nullptr; }

	virtual FBodyInstance* GetBodyInstance(FName BoneName = NAME_None, bool bGetWelded = true, int32 Index = INDEX_NONE) const override
	{
		return nullptr;
	}
};

#endif // WITH_EDITOR
