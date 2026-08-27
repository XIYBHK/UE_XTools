// Copyright 2023 Tomasz Klin. All Rights Reserved.

#pragma once

/**
 * 组件时间轴库
 * 提供了一系列用于操作和管理组件时间轴的蓝图函数
 */

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ComponentTimelineLibrary.generated.h"

/**
 * 组件时间轴绑定判定
 */
namespace ComponentTimeline
{
	/** 时间轴重复初始化判定结果 */
	enum class ETimelineBindDecision
	{
		Create,       // 属性为空：创建并绑定新时间轴
		Skip,         // 属性持有有效时间轴：幂等跳过
		RebindStale   // 属性持有失效/pending-kill 时间轴：先清空属性再重新创建绑定
	};

	/**
	 * 时间轴重复初始化判定 - 纯函数，便于确定性测试。
	 *
	 * @param   CurrentValue    蓝图中 UTimelineComponent* 属性的当前值（可为空）
	 * @return  空指针返回 Create；有效对象返回 Skip；已失效或 pending-kill 返回 RebindStale
	 */
	ETimelineBindDecision ResolveTimelineBindDecision(const UObject* CurrentValue);
}

/**
 * 组件时间轴功能库类
 * 包含了一系列静态函数，用于初始化和管理组件时间轴
 */
UCLASS(BlueprintType)
class XTOOLS_COMPONENTTIMELINERUNTIME_API UComponentTimelineLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/**
	 * 初始化指定组件的所有时间轴
	 * @param Component - 需要初始化时间轴的目标组件
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|Timeline", 
		meta = (DisplayName = "初始化组件时间轴", 
		Keywords = "Initialize Component Timelines",
		UnsafeDuringActorConstruction = "true", 
		DefaultToSelf="Component"))
	static void InitializeComponentTimelines(UActorComponent* Component);

	/**
	 * 初始化蓝图对象的所有时间轴
	 * @param BlueprintOwner - 拥有时间轴的蓝图对象
	 * @param ActorOwner - 关联的Actor对象
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|Timeline", 
		meta = (DisplayName = "初始化对象时间轴", 
		Keywords = "Initialize Object Timelines",
		UnsafeDuringActorConstruction = "true", 
		DefaultToSelf="BlueprintOwner"))
	static void InitializeTimelines(UObject* BlueprintOwner, AActor* ActorOwner);
};
