/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

// 说明：本测试类不在编译期剔除（不加 #if WITH_EDITOR 守卫）——
// UHT 对 Game 目标生成的 .gen.cpp 不会继承该守卫，守卫反而导致打包构建
// 引用未声明类型而失败。类本身惰性无副作用，仅被编辑器自动化测试引用。
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
