// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ECFInstanceId.h"
#include "ECFInstanceIdBP.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

/**
 * ECF实例ID
 * 用于在蓝图中标识和管理ECF动作实例
 * 可以用来管理多个相关动作，或者控制动作的重复执行
 */
USTRUCT(BlueprintType, meta=(DisplayName="ECF实例ID", Tooltip="用于标识和管理ECF动作实例的唯一标识符"))
struct ENHANCEDCODEFLOW_API FECFInstanceIdBP
{
	GENERATED_BODY()

	/** 内部实例ID实现 */
	FECFInstanceId InstanceId;

	/** 默认构造函数 */
	FECFInstanceIdBP()
	{}

	/** 从内部实例ID构造 */
	FECFInstanceIdBP(const FECFInstanceId& InInstanceId) :
		InstanceId(InInstanceId)
	{}

	/** 从内部实例ID移动构造 */
	FECFInstanceIdBP(FECFInstanceId&& InInstanceId) :
		InstanceId(MoveTemp(InInstanceId))
	{}

	/** 拷贝构造函数 */
	FECFInstanceIdBP(const FECFInstanceIdBP& Other) :
		InstanceId(Other.InstanceId)
	{}

	/** 移动构造函数 */
	FECFInstanceIdBP(FECFInstanceIdBP&& Other) :
		InstanceId(MoveTemp(Other.InstanceId))
	{}

	/** 赋值运算符 */
	FECFInstanceIdBP& operator=(const FECFInstanceIdBP& Other)
	{
		InstanceId = Other.InstanceId;
		return *this;
	}

	/** 移动赋值运算符 */
	FECFInstanceIdBP& operator=(FECFInstanceIdBP&& Other)
	{
		InstanceId = Forward<FECFInstanceId>(Other.InstanceId);
		return *this;
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION