// Copyright (c) 2024 Damian Nowakowski. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ECFHandle.h"
#include "ECFHandleBP.generated.h"

ECF_PRAGMA_DISABLE_OPTIMIZATION

/**
 * ECF动作句柄
 * 用于在蓝图中控制和追踪ECF动作的执行状态
 * 每个动作都有一个唯一的句柄，可用于暂停、恢复或停止该动作
 */
USTRUCT(BlueprintType, meta=(DisplayName="ECF动作句柄", Tooltip="用于控制和追踪ECF动作的句柄"))
struct ENHANCEDCODEFLOW_API FECFHandleBP
{
	GENERATED_BODY()

	/** 内部句柄实现 */
	FECFHandle Handle;

	/** 默认构造函数 */
	FECFHandleBP()
	{}

	/** 从内部句柄构造 */
	FECFHandleBP(const FECFHandle& InHandle) :
		Handle(InHandle)
	{}

	/** 从内部句柄移动构造 */
	FECFHandleBP(FECFHandle&& InHandle) :
		Handle(MoveTemp(InHandle))
	{}

	/** 拷贝构造函数 */
	FECFHandleBP(const FECFHandleBP& Other) :
		Handle(Other.Handle)
	{}

	/** 移动构造函数 */
	FECFHandleBP(FECFHandleBP&& Other) :
		Handle(MoveTemp(Other.Handle))
	{}

	/** 赋值运算符 */
	FECFHandleBP& operator=(const FECFHandleBP& Other)
	{
		Handle = Other.Handle;
		return *this;
	}

	/** 移动赋值运算符 */
	FECFHandleBP& operator=(FECFHandleBP&& Other)
	{
		Handle = Forward<FECFHandle>(Other.Handle);
		return *this;
	}
};

ECF_PRAGMA_ENABLE_OPTIMIZATION