// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 组件时间轴库实现文件
 * 提供了组件时间轴功能的核心实现
 */

#include "ComponentTimelineLibrary.h"
#include "Engine/TimelineTemplate.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/TimelineComponent.h"
#include "Engine/World.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"

DEFINE_LOG_CATEGORY_STATIC(LogComponentTimelineRuntime, Log, All);

namespace
{
	/**
	 * 绑定时间轴到指定对象
	 * @param TimelineTemplate - 时间轴模板
	 * @param BlueprintOwner - 拥有时间轴的蓝图对象
	 * @param ActorOwner - 关联的Actor对象
	 */
	void BindTimeline(UTimelineTemplate* TimelineTemplate, UObject* BlueprintOwner, AActor* ActorOwner)
	{
		// 验证输入参数的有效性
		if (!IsValid(BlueprintOwner)
			|| !IsValid(ActorOwner)
			|| !TimelineTemplate
			|| BlueprintOwner->IsTemplate())
		{
			return;
		}

		FName NewName = TimelineTemplate->GetVariableName();

		// 查找与模板同名的属性并将新的Timeline赋值给它
		UClass* ComponentClass = BlueprintOwner->GetClass();
		FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(ComponentClass, TimelineTemplate->GetVariableName());
		if (Prop)
		{
			// 检查TimelineComponent是否已创建，防止多次调用InitializeTimeLineComponent
			UObject* CurrentValue = Prop->GetObjectPropertyValue_InContainer(BlueprintOwner);
			if (CurrentValue) 
				return;
		}

		// 创建唯一的时间轴组件名称
		FName Name = MakeUniqueObjectName(ActorOwner,  UTimelineComponent::StaticClass(), *FString::Printf(TEXT("%s_%s"), *BlueprintOwner->GetName(), *TimelineTemplate->GetVariableName().ToString()));
		UTimelineComponent* NewTimeline = NewObject<UTimelineComponent>(ActorOwner, Name);
		NewTimeline->CreationMethod = EComponentCreationMethod::UserConstructionScript; // 标记为蓝图创建，以便在重新运行构造脚本时清理
		ActorOwner->BlueprintCreatedComponents.Add(NewTimeline); // 添加到数组以便保存
		NewTimeline->SetNetAddressable();	// 设置组件具有可用于复制的稳定名称

		// 设置时间轴的基本属性
		NewTimeline->SetPropertySetObject(BlueprintOwner); // 设置时间轴应该驱动哪个对象的属性
		NewTimeline->SetDirectionPropertyName(TimelineTemplate->GetDirectionPropertyName());
		NewTimeline->SetTimelineLength(TimelineTemplate->TimelineLength); // 复制长度
		NewTimeline->SetTimelineLengthMode(TimelineTemplate->LengthMode);
		NewTimeline->PrimaryComponentTick.TickGroup = TimelineTemplate->TimelineTickGroup;

		if (Prop)
		{
			Prop->SetObjectPropertyValue_InContainer(BlueprintOwner, NewTimeline);
		}

		// 事件轨道处理
		// 在模板中每个函数都有一个轨道，但在运行时Timeline中每个键都有自己的委托，所以我们将它们合并在一起
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->EventTracks.Num(); TrackIdx++)
		{
			const FTTEventTrack* EventTrackTemplate = &TimelineTemplate->EventTracks[TrackIdx];
			if (EventTrackTemplate->CurveKeys != nullptr)
			{
				// 为该轨道的所有键创建委托
				FScriptDelegate EventDelegate;
				EventDelegate.BindUFunction(BlueprintOwner, EventTrackTemplate->GetFunctionName());

				// 为该轨道的每个键在Events中创建一个条目
				for (auto It(EventTrackTemplate->CurveKeys->FloatCurve.GetKeyIterator()); It; ++It)
				{
					NewTimeline->AddEvent(It->Time, FOnTimelineEvent(EventDelegate));
				}
			}
		}

		// 浮点数轨道处理
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->FloatTracks.Num(); TrackIdx++)
		{
			const FTTFloatTrack* FloatTrackTemplate = &TimelineTemplate->FloatTracks[TrackIdx];
			if (FloatTrackTemplate->CurveFloat != NULL)
			{
				NewTimeline->AddInterpFloat(FloatTrackTemplate->CurveFloat, FOnTimelineFloat(), FloatTrackTemplate->GetPropertyName(), FloatTrackTemplate->GetTrackName());
			}
		}

		// 向量轨道处理
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->VectorTracks.Num(); TrackIdx++)
		{
			const FTTVectorTrack* VectorTrackTemplate = &TimelineTemplate->VectorTracks[TrackIdx];
			if (VectorTrackTemplate->CurveVector != NULL)
			{
				NewTimeline->AddInterpVector(VectorTrackTemplate->CurveVector, FOnTimelineVector(), VectorTrackTemplate->GetPropertyName(), VectorTrackTemplate->GetTrackName());
			}
		}

		// 线性颜色轨道处理
		for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->LinearColorTracks.Num(); TrackIdx++)
		{
			const FTTLinearColorTrack* LinearColorTrackTemplate = &TimelineTemplate->LinearColorTracks[TrackIdx];
			if (LinearColorTrackTemplate->CurveLinearColor != NULL)
			{
				NewTimeline->AddInterpLinearColor(LinearColorTrackTemplate->CurveLinearColor, FOnTimelineLinearColor(), LinearColorTrackTemplate->GetPropertyName(), LinearColorTrackTemplate->GetTrackName());
			}
		}

		// 设置在所有属性更新后调用的委托
		FScriptDelegate UpdateDelegate;
		UpdateDelegate.BindUFunction(BlueprintOwner, TimelineTemplate->GetUpdateFunctionName());
		NewTimeline->SetTimelinePostUpdateFunc(FOnTimelineEvent(UpdateDelegate));

		// 设置完成时调用的委托
		FScriptDelegate FinishedDelegate;
		FinishedDelegate.BindUFunction(BlueprintOwner, TimelineTemplate->GetFinishedFunctionName());
		NewTimeline->SetTimelineFinishedFunc(FOnTimelineEvent(FinishedDelegate));

		// 注册组件
		NewTimeline->RegisterComponent();

		// 如果需要，立即开始播放
		if (TimelineTemplate->bAutoPlay)
		{
			// 在烘焙版本中需要为自动播放时间轴设置，因为它们不会通过下面的Play调用来调用Activate
			NewTimeline->bAutoActivate = true;
			NewTimeline->Play();
		}

		// 如果需要，设置循环
		if (TimelineTemplate->bLoop)
		{
			NewTimeline->SetLooping(true);
		}

		// 如果需要，设置复制
		if (TimelineTemplate->bReplicated)
		{
			NewTimeline->SetIsReplicated(true);
		}

		// 如果需要，设置忽略时间膨胀
		if (TimelineTemplate->bIgnoreTimeDilation)
		{
			NewTimeline->SetIgnoreTimeDilation(true);
		}
	}
}

/**
 * 初始化组件的时间轴
 * @param Component - 需要初始化时间轴的组件
 */
void UComponentTimelineLibrary::InitializeComponentTimelines(UActorComponent* Component)
{
    if (!IsValid(Component))
    {
        UE_LOG(LogComponentTimelineRuntime, Error, TEXT("InitializeComponentTimelines: 无效的组件对象"));
        return;
    }

	InitializeTimelines(Component, Component->GetOwner());
}

/**
 * 初始化对象的时间轴
 * @param BlueprintOwner - 拥有时间轴的蓝图对象
 * @param ActorOwner - 关联的Actor对象
 */
void UComponentTimelineLibrary::InitializeTimelines(UObject* BlueprintOwner, AActor* ActorOwner)
{
    if (!IsValid(BlueprintOwner) || !IsValid(ActorOwner))
    {
        UE_LOG(LogComponentTimelineRuntime, Error, TEXT("InitializeTimelines: BlueprintOwner 或 ActorOwner 无效"));
        return;
    }

	// 生成该Actor的父蓝图层次结构，以便我们可以按顺序运行所有构造脚本
	TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
	const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(BlueprintOwner->GetClass(), ParentBPClassStack);

	// 如果这个Actor有蓝图继承关系，从最不派生到最派生的顺序运行构造脚本
	if ((ParentBPClassStack.Num() > 0))
	{
		if (bErrorFree)
		{
			// 防止用户在用户构造脚本中生成Actor
			FGuardValue_Bitfield(BlueprintOwner->GetWorld()->bIsRunningConstructionScript, true);
			for (int32 i = ParentBPClassStack.Num() - 1; i >= 0; i--)
			{
				const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
				check(CurrentBPGClass);

				if (const UBlueprintGeneratedClass* BPGC = Cast<const UBlueprintGeneratedClass>(CurrentBPGClass))
				{
					for (UTimelineTemplate* TimelineTemplate : BPGC->Timelines)
					{
						// 如果为NULL不会导致致命错误，但不应该发生，如果没有在图表中连接则会被忽略
						if (TimelineTemplate)
						{
							BindTimeline(TimelineTemplate, BlueprintOwner, ActorOwner);
						}
					}
				}
			}
		}
	}
}

