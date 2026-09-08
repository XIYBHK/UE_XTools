// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "EnhancedCodeFlow.h"
#include "ECFSubsystem.h"
#include "BP/Actions/ECFCustomTimelineBP.h"
#include "BP/Actions/ECFCustomTimelineLinearColorBP.h"
#include "BP/Actions/ECFCustomTimelineVectorBP.h"
#include "BP/Actions/ECFTimelineBP.h"
#include "BP/Actions/ECFTimelineLinearColorBP.h"
#include "BP/Actions/ECFTimelineVectorBP.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Tickable.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	class FScopedECFTestWorld
	{
	public:
		explicit FScopedECFTestWorld(FName WorldName)
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(WorldName);
			World = GameInstance->GetWorld();
			Subsystem = GameInstance->GetSubsystem<UECFSubsystem>();
		}

		~FScopedECFTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		bool IsValid() const
		{
			return World != nullptr && Subsystem != nullptr;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

		void Tick(float DeltaTime) const
		{
			static_cast<FTickableGameObject*>(Subsystem)->Tick(DeltaTime);
		}

	private:
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		UECFSubsystem* Subsystem = nullptr;
	};

	void AddLinearKeys(UCurveFloat* Curve)
	{
		Curve->FloatCurve.AddKey(0.f, 0.f);
		Curve->FloatCurve.AddKey(1.f, 1.f);
	}

	void AddLinearKeys(UCurveVector* Curve)
	{
		for (FRichCurve& RichCurve : Curve->FloatCurves)
		{
			RichCurve.AddKey(0.f, 0.f);
			RichCurve.AddKey(1.f, 1.f);
		}
	}

	void AddLinearKeys(UCurveLinearColor* Curve)
	{
		for (FRichCurve& RichCurve : Curve->FloatCurves)
		{
			RichCurve.AddKey(0.f, 0.f);
			RichCurve.AddKey(1.f, 1.f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFActionLookupLifecycleTest,
	"XTools.EnhancedCodeFlow.Subsystem.LookupLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFActionLookupLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedECFTestWorld TestWorld(TEXT("ECFLookupLifecycle"));
	if (!TestTrue(TEXT("Test world is valid"), TestWorld.IsValid())) { return false; }
	UWorld* World = TestWorld.GetWorld();
	const FECFInstanceId Id = FECFInstanceId::NewId();
	FECFHandle Inner;
	FECFHandle Outer = FFlow::DoOnce(World, [&]()
	{
		Inner = FFlow::DoOnce(World, []() {}, Id);
	}, Id);
	TestTrue(TEXT("Reentrant initialization creates distinct handles"), Outer != Inner);
	TestTrue(TEXT("Pending lookup returns first registered instance"), FFlow::DoOnce(World, []() {}, Id) == Inner);
	TestWorld.Tick(0.01f);
	TestTrue(TEXT("Active lookup preserves registration order"), FFlow::DoOnce(World, []() {}, Id) == Inner);
	const FECFHandle StoppedInner = Inner;
	FFlow::StopAction(World, Inner);
	TestFalse(TEXT("Finished action disappears before pruning"), FFlow::IsActionRunning(World, StoppedInner));
	TestTrue(TEXT("Next same-ID action remains available"), FFlow::DoOnce(World, []() {}, Id) == Outer);
	FFlow::StopAction(World, Outer);
	FECFHandle Replacement = FFlow::DoOnce(World, []() {}, Id);
	TestWorld.Tick(0.01f);
	TestTrue(TEXT("Pruning old instances retains replacement"), FFlow::DoOnce(World, []() {}, Id) == Replacement);
	FFlow::StopInstancedAction(World, Id);
	TestFalse(TEXT("Instance stop invalidates indexed lookup"), FFlow::IsActionRunning(World, Replacement));

	FECFHandle Child;
	FECFHandle Parent = FFlow::AddTicker(World, 0.01f, [](float) {}, [&]()
	{
		Child = FFlow::DoOnce(World, []() {}, FECFInstanceId::NewId());
	});
	TestWorld.Tick(0.1f);
	TestFalse(TEXT("Naturally completed action is unavailable"), FFlow::IsActionRunning(World, Parent));
	TestTrue(TEXT("Action created during Tick is immediately findable"), FFlow::IsActionRunning(World, Child));
	TestWorld.Tick(0.01f);
	TestTrue(TEXT("Pending child survives next Tick"), FFlow::IsActionRunning(World, Child));
	FFlow::StopAction(World, Child);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFActionLookupScaleTest,
	"XTools.EnhancedCodeFlow.Subsystem.LookupScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFActionLookupScaleTest::RunTest(const FString& Parameters)
{
	FScopedECFTestWorld TestWorld(TEXT("ECFLookupScale"));
	if (!TestTrue(TEXT("Test world is valid"), TestWorld.IsValid())) { return false; }
	UWorld* World = TestWorld.GetWorld();
	constexpr int32 Count = 4096;
	TArray<FECFHandle> Handles;
	TArray<FECFInstanceId> Ids;
	Handles.Reserve(Count);
	Ids.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Ids.Add(FECFInstanceId::NewId());
		Handles.Add(FFlow::DoOnce(World, []() {}, Ids.Last()));
	}
	TestWorld.Tick(0.01f);
	const double Start = FPlatformTime::Seconds();
	int32 Found = 0;
	for (int32 Repeat = 0; Repeat < 4; ++Repeat)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Found += FFlow::IsActionRunning(World, Handles[Index]) ? 1 : 0;
			Found += FFlow::DoOnce(World, []() {}, Ids[Index]) == Handles[Index] ? 1 : 0;
		}
	}
	AddInfo(FString::Printf(TEXT("LookupScale: 32768 handle/instance queries, %.3f ms"), (FPlatformTime::Seconds() - Start) * 1000.0));
	TestEqual(TEXT("Every handle and instance lookup succeeds"), Found, Count * 8);
	for (FECFHandle& Handle : Handles) { FFlow::StopAction(World, Handle); }
	TestWorld.Tick(0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFTimelineValueConsistencyTest,
	"XTools.EnhancedCodeFlow.Timeline.ValueConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFTimelineValueConsistencyTest::RunTest(const FString& Parameters)
{
	FScopedECFTestWorld TestWorld(TEXT("ECFTimelineValueConsistency"));
	if (!TestTrue(TEXT("Test world is valid"), TestWorld.IsValid())) { return false; }
	for (EECFBlendFunc Blend : {EECFBlendFunc::ECFBlend_Linear, EECFBlendFunc::ECFBlend_Cubic,
		EECFBlendFunc::ECFBlend_EaseIn, EECFBlendFunc::ECFBlend_EaseOut, EECFBlendFunc::ECFBlend_EaseInOut})
	{
		for (EECFPlayDirection Direction : {EECFPlayDirection::Forward, EECFPlayDirection::Reverse})
		{
			float Scalar = 0.f;
			FVector Vector = FVector::ZeroVector;
			FLinearColor Color = FLinearColor::Transparent;
			FECFHandle ScalarHandle = FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
				[&](float Value, float) { Scalar = Value; }, [](float, float, bool) {}, Blend, 2.f, 1.f, {}, Direction);
			FECFHandle VectorHandle = FFlow::AddTimelineVector(TestWorld.GetWorld(), FVector::ZeroVector, FVector::OneVector, 1.f,
				[&](FVector Value, float) { Vector = Value; }, [](FVector, float, bool) {}, Blend, 2.f, 1.f, {}, Direction);
			FECFHandle ColorHandle = FFlow::AddTimelineLinearColor(TestWorld.GetWorld(), FLinearColor::Transparent, FLinearColor::White, 1.f,
				[&](FLinearColor Value, float) { Color = Value; }, [](FLinearColor, float, bool) {}, Blend, 2.f, 1.f, {}, Direction);
			const auto CheckValues = [&](float Alpha)
			{
				float Expected = Alpha;
				switch (Blend)
				{
				case EECFBlendFunc::ECFBlend_Cubic: Expected = Alpha * Alpha * (3.f - 2.f * Alpha); break;
				case EECFBlendFunc::ECFBlend_EaseIn: Expected = Alpha * Alpha; break;
				case EECFBlendFunc::ECFBlend_EaseOut: Expected = FMath::Sqrt(Alpha); break;
				case EECFBlendFunc::ECFBlend_EaseInOut: Expected = Alpha < 0.5f ? 2.f * Alpha * Alpha : 1.f - 2.f * (1.f - Alpha) * (1.f - Alpha); break;
				default: break;
				}
				TestTrue(TEXT("Scalar follows the expected blend"), FMath::IsNearlyEqual(Scalar, Expected, 0.00001f));
				TestTrue(TEXT("Vector follows the same blend"), Vector.Equals(FVector(Expected), 0.00001));
				TestTrue(TEXT("All four color channels follow the same blend"), Color.Equals(FLinearColor(Expected, Expected, Expected, Expected), 0.00001f));
			};
			TestWorld.Tick(0.01f);
			CheckValues(Direction == EECFPlayDirection::Reverse ? 1.f : 0.f);
			TestWorld.Tick(0.25f);
			CheckValues(Direction == EECFPlayDirection::Reverse ? 0.75f : 0.25f);
			for (float Time : {0.25f, 0.75f})
			{
				TestTrue(TEXT("Scalar seek succeeds"), FFlow::SetActionTime(TestWorld.GetWorld(), ScalarHandle, Time, true));
				TestTrue(TEXT("Vector seek succeeds"), FFlow::SetActionTime(TestWorld.GetWorld(), VectorHandle, Time, true));
				TestTrue(TEXT("Color seek succeeds"), FFlow::SetActionTime(TestWorld.GetWorld(), ColorHandle, Time, true));
				CheckValues(Time);
			}
			FFlow::StopAction(TestWorld.GetWorld(), ScalarHandle);
			FFlow::StopAction(TestWorld.GetWorld(), VectorHandle);
			FFlow::StopAction(TestWorld.GetWorld(), ColorHandle);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFTimelineBlueprintOutputContractTest,
	"XTools.EnhancedCodeFlow.Timeline.BlueprintOutputContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFTimelineBlueprintOutputContractTest::RunTest(const FString& Parameters)
{
	const UClass* ProxyClasses[] =
	{
		UECFTimelineBP::StaticClass(),
		UECFTimelineVectorBP::StaticClass(),
		UECFTimelineLinearColorBP::StaticClass(),
		UECFCustomTimelineBP::StaticClass(),
		UECFCustomTimelineVectorBP::StaticClass(),
		UECFCustomTimelineLinearColorBP::StaticClass()
	};
	const FName FactoryFunctionNames[] =
	{
		GET_FUNCTION_NAME_CHECKED(UECFTimelineBP, ECFTimeline),
		GET_FUNCTION_NAME_CHECKED(UECFTimelineVectorBP, ECFTimelineVector),
		GET_FUNCTION_NAME_CHECKED(UECFTimelineLinearColorBP, ECFTimelineLinearColor),
		GET_FUNCTION_NAME_CHECKED(UECFCustomTimelineBP, ECFCustomTimeline),
		GET_FUNCTION_NAME_CHECKED(UECFCustomTimelineVectorBP, ECFCustomTimelineVector),
		GET_FUNCTION_NAME_CHECKED(UECFCustomTimelineLinearColorBP, ECFCustomTimelineLinearColor)
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ProxyClasses); ++Index)
	{
		const UClass* ProxyClass = ProxyClasses[Index];
		const FMulticastDelegateProperty* OnTick = FindFProperty<FMulticastDelegateProperty>(ProxyClass, TEXT("OnTick"));
		const FMulticastDelegateProperty* OnFinished = FindFProperty<FMulticastDelegateProperty>(ProxyClass, TEXT("OnFinished"));
		const FMulticastDelegateProperty* OnEvent = FindFProperty<FMulticastDelegateProperty>(ProxyClass, TEXT("OnEvent"));
		const UFunction* FactoryFunction = ProxyClass->FindFunctionByName(FactoryFunctionNames[Index]);
		const FString ClassName = ProxyClass->GetName();

		TestNotNull(*FString::Printf(TEXT("%s 应暴露 OnTick 委托"), *ClassName), OnTick);
		TestNotNull(*FString::Printf(TEXT("%s 应暴露 OnFinished 委托"), *ClassName), OnFinished);
		TestNotNull(*FString::Printf(TEXT("%s 应暴露 OnEvent 委托"), *ClassName), OnEvent);
		TestNotNull(*FString::Printf(TEXT("%s 应暴露时间轴工厂函数"), *ClassName), FactoryFunction);
		if (FactoryFunction)
		{
			const FArrayProperty* EventsProperty = FindFProperty<FArrayProperty>(FactoryFunction, TEXT("Events"));
			TestNotNull(*FString::Printf(TEXT("%s 应暴露 Events 输入"), *ClassName), EventsProperty);
			if (EventsProperty)
			{
				TestTrue(*FString::Printf(TEXT("%s 的 Events 输入应默认折叠"), *ClassName),
					EventsProperty->HasAnyPropertyFlags(CPF_AdvancedDisplay));
				TestTrue(*FString::Printf(TEXT("%s 的 Events 输入应为引用参数"), *ClassName),
					EventsProperty->HasAnyPropertyFlags(CPF_ReferenceParm));
				TestTrue(*FString::Printf(TEXT("%s 的 Events 输入应为只读参数"), *ClassName),
					EventsProperty->HasAnyPropertyFlags(CPF_ConstParm));
			}
			TestEqual(*FString::Printf(TEXT("%s 的 Events 输入应允许留空"), *ClassName),
				FactoryFunction->GetMetaData(TEXT("AutoCreateRefTerm")), FString(TEXT("Events")));
		}
		if (!OnTick || !OnFinished || !OnEvent)
		{
			continue;
		}

		TestTrue(*FString::Printf(TEXT("%s 的三个异步输出必须共享同一参数签名"), *ClassName),
			OnTick->SignatureFunction == OnFinished->SignatureFunction && OnTick->SignatureFunction == OnEvent->SignatureFunction);
		TestNotNull(*FString::Printf(TEXT("%s 应输出 EventName"), *ClassName),
			FindFProperty<FNameProperty>(OnTick->SignatureFunction, TEXT("EventName")));
		TestNull(*FString::Printf(TEXT("%s 不应输出 EventTime"), *ClassName),
			FindFProperty<FFloatProperty>(OnTick->SignatureFunction, TEXT("EventTime")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFTimelineCubicColorPreservesAlphaTest,
	"XTools.EnhancedCodeFlow.Timeline.CubicColorPreservesAlpha",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFTimelineCubicColorPreservesAlphaTest::RunTest(const FString& Parameters)
{
	FScopedECFTestWorld TestWorld(TEXT("ECFTimelineCubicColorPreservesAlphaTest"));
	if (!TestTrue(TEXT("应创建 ECF 测试世界和子系统"), TestWorld.IsValid()))
	{
		return false;
	}

	const FLinearColor QuarterColor(0.84375f, 0.f, 0.15625f, 1.f);
	const FLinearColor ThreeQuarterColor(0.15625f, 0.f, 0.84375f, 1.f);
	for (EECFPlayDirection Direction : {EECFPlayDirection::Forward, EECFPlayDirection::Reverse})
	{
		FLinearColor Value = FLinearColor::Transparent;
		FECFHandle Handle = FFlow::AddTimelineLinearColor(TestWorld.GetWorld(),
			FLinearColor::Red, FLinearColor::Blue, 1.f,
			[&Value](FLinearColor NewValue, float Time) { Value = NewValue; },
			[](FLinearColor, float, bool) {},
			EECFBlendFunc::ECFBlend_Cubic, 1.f, 1.f, {}, Direction);
		if (!TestTrue(TEXT("三次颜色时间轴应成功启动"), Handle.IsValid()))
		{
			continue;
		}

		TestWorld.Tick(0.01f); // 首帧只派发起点
		TestWorld.Tick(0.25f);
		const bool bReverse = Direction == EECFPlayDirection::Reverse;
		TestTrue(TEXT("四分之一播放时间的所有颜色分量应遵循零切线曲线"),
			Value.Equals(bReverse ? ThreeQuarterColor : QuarterColor, 0.00001f));
		TestWorld.Tick(0.5f);
		TestTrue(TEXT("四分之三播放时间应保持恒定 Alpha"),
			Value.Equals(bReverse ? QuarterColor : ThreeQuarterColor, 0.00001f));

		TestTrue(TEXT("应能设置时间到四分之一"), FFlow::SetActionTime(TestWorld.GetWorld(), Handle, 0.25f, true));
		TestTrue(TEXT("手动设时应使用同样的颜色曲线"), Value.Equals(QuarterColor, 0.00001f));
		TestTrue(TEXT("应能设置时间到四分之三"), FFlow::SetActionTime(TestWorld.GetWorld(), Handle, 0.75f, true));
		TestTrue(TEXT("手动设时不应引入透明度变化"), Value.Equals(ThreeQuarterColor, 0.00001f));
		FFlow::StopAction(TestWorld.GetWorld(), Handle, false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFTimelineReversePlaybackTest,
	"XTools.EnhancedCodeFlow.Timeline.ReversePlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFTimelineReversePlaybackTest::RunTest(const FString& Parameters)
{
	FScopedECFTestWorld TestWorld(TEXT("ECFTimelineReversePlaybackTest"));
	if (!TestTrue(TEXT("应创建 ECF 测试世界和子系统"), TestWorld.IsValid()))
	{
		return false;
	}

	float ScalarValue = -1.f;
	FVector VectorValue = FVector::ZeroVector;
	FLinearColor ColorValue = FLinearColor::Black;
	float CustomScalarValue = -1.f;
	FVector CustomVectorValue = FVector::ZeroVector;
	FLinearColor CustomColorValue = FLinearColor::Black;
	int32 FinishedCount = 0;
	bool bAnyStopped = false;

	UCurveFloat* FloatCurve = NewObject<UCurveFloat>(TestWorld.GetWorld());
	UCurveVector* VectorCurve = NewObject<UCurveVector>(TestWorld.GetWorld());
	UCurveLinearColor* ColorCurve = NewObject<UCurveLinearColor>(TestWorld.GetWorld());
	AddLinearKeys(FloatCurve);
	AddLinearKeys(VectorCurve);
	AddLinearKeys(ColorCurve);

	auto MakeScalarFinished = [&FinishedCount, &bAnyStopped]()
	{
		return TUniqueFunction<void(float, float, bool)>([&FinishedCount, &bAnyStopped](float Value, float Time, bool bStopped)
		{
			++FinishedCount;
			bAnyStopped |= bStopped;
		});
	};
	auto MakeVectorFinished = [&FinishedCount, &bAnyStopped]()
	{
		return TUniqueFunction<void(FVector, float, bool)>([&FinishedCount, &bAnyStopped](FVector Value, float Time, bool bStopped)
		{
			++FinishedCount;
			bAnyStopped |= bStopped;
		});
	};
	auto MakeColorFinished = [&FinishedCount, &bAnyStopped]()
	{
		return TUniqueFunction<void(FLinearColor, float, bool)>([&FinishedCount, &bAnyStopped](FLinearColor Value, float Time, bool bStopped)
		{
			++FinishedCount;
			bAnyStopped |= bStopped;
		});
	};

	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[&ScalarValue](float Value, float Time) { ScalarValue = Value; }, MakeScalarFinished(),
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Reverse);
	FFlow::AddTimelineVector(TestWorld.GetWorld(), FVector::ZeroVector, FVector::OneVector, 1.f,
		[&VectorValue](FVector Value, float Time) { VectorValue = Value; }, MakeVectorFinished(),
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Reverse);
	FFlow::AddTimelineLinearColor(TestWorld.GetWorld(), FLinearColor::Black, FLinearColor::White, 1.f,
		[&ColorValue](FLinearColor Value, float Time) { ColorValue = Value; }, MakeColorFinished(),
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Reverse);
	FFlow::AddCustomTimeline(TestWorld.GetWorld(), FloatCurve,
		[&CustomScalarValue](float Value, float Time) { CustomScalarValue = Value; }, MakeScalarFinished(),
		1.f, {}, EECFPlayDirection::Reverse);
	FFlow::AddCustomTimelineVector(TestWorld.GetWorld(), VectorCurve,
		[&CustomVectorValue](FVector Value, float Time) { CustomVectorValue = Value; }, MakeVectorFinished(),
		1.f, {}, EECFPlayDirection::Reverse);
	FFlow::AddCustomTimelineLinearColor(TestWorld.GetWorld(), ColorCurve,
		[&CustomColorValue](FLinearColor Value, float Time) { CustomColorValue = Value; }, MakeColorFinished(),
		1.f, {}, EECFPlayDirection::Reverse);

	TestWorld.Tick(0.1f);
	TestTrue(TEXT("标量时间轴反向首帧应输出结束值"), FMath::IsNearlyEqual(ScalarValue, 1.f));
	TestTrue(TEXT("向量时间轴反向首帧应输出结束值"), VectorValue.Equals(FVector::OneVector));
	TestTrue(TEXT("颜色时间轴反向首帧应输出结束值"), ColorValue.Equals(FLinearColor::White));
	TestTrue(TEXT("自定义标量时间轴反向首帧应输出曲线末端"), FMath::IsNearlyEqual(CustomScalarValue, 1.f));
	TestTrue(TEXT("自定义向量时间轴反向首帧应输出曲线末端"), CustomVectorValue.Equals(FVector::OneVector));
	TestTrue(TEXT("自定义颜色时间轴反向首帧应输出曲线末端"), CustomColorValue.Equals(FLinearColor::White));

	TestWorld.Tick(0.25f);
	TestTrue(TEXT("反向播放应递减标量值"), FMath::IsNearlyEqual(ScalarValue, 0.75f));
	TestTrue(TEXT("反向播放应递减自定义曲线值"), FMath::IsNearlyEqual(CustomScalarValue, 0.75f));

	TestWorld.Tick(1.f);
	TestEqual(TEXT("六类时间轴应各完成一次"), FinishedCount, 6);
	TestFalse(TEXT("自然反向完成不应报告外部停止"), bAnyStopped);
	TestTrue(TEXT("反向完成应精确输出起始值"), FMath::IsNearlyEqual(ScalarValue, 0.f));
	TestTrue(TEXT("自定义时间轴反向完成应精确输出曲线起点"), FMath::IsNearlyEqual(CustomScalarValue, 0.f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFTimelineDirectionControlAndLoopTest,
	"XTools.EnhancedCodeFlow.Timeline.DirectionControlAndLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFTimelineDirectionControlAndLoopTest::RunTest(const FString& Parameters)
{
	FScopedECFTestWorld TestWorld(TEXT("ECFTimelineDirectionControlAndLoopTest"));
	if (!TestTrue(TEXT("应创建 ECF 测试世界和子系统"), TestWorld.IsValid()))
	{
		return false;
	}

	float CurrentTime = -1.f;
	FECFHandle ControlledHandle = FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[&CurrentTime](float Value, float Time) { CurrentTime = Time; },
		[](float Value, float Time, bool bStopped) {}, EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Reverse);

	TestTrue(TEXT("设置动作时间应成功"), FFlow::SetActionTime(TestWorld.GetWorld(), ControlledHandle, 0.6f, false));
	TestWorld.Tick(0.1f);
	TestTrue(TEXT("首次 Tick 应保持显式设置的时间"), FMath::IsNearlyEqual(CurrentTime, 0.6f));
	TestWorld.Tick(0.1f);
	TestTrue(TEXT("设置时间后应保持反向方向"), FMath::IsNearlyEqual(CurrentTime, 0.5f));
	TestTrue(TEXT("运行中反转动作应成功"), FFlow::ReverseAction(TestWorld.GetWorld(), ControlledHandle));
	TestWorld.Tick(0.1f);
	TestTrue(TEXT("反转后应从当前时间正向继续"), FMath::IsNearlyEqual(CurrentTime, 0.6f));
	TestTrue(TEXT("显式设置反向方向应成功"), FFlow::SetActionPlayDirection(TestWorld.GetWorld(), ControlledHandle, EECFPlayDirection::Reverse));
	TestWorld.Tick(0.1f);
	TestTrue(TEXT("显式方向应立即用于后续 Tick"), FMath::IsNearlyEqual(CurrentTime, 0.5f));

	TArray<float> LoopTimes;
	int32 LoopFinishedCount = 0;
	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[&LoopTimes](float Value, float Time) { LoopTimes.Add(Time); },
		[&LoopFinishedCount](float Value, float Time, bool bStopped) { ++LoopFinishedCount; },
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, ECF_LOOP, EECFPlayDirection::Reverse);

	TestWorld.Tick(0.1f);
	TestWorld.Tick(1.25f);
	TestEqual(TEXT("循环反向播放不应触发完成回调"), LoopFinishedCount, 0);
	TestTrue(TEXT("循环反向播放应先精确输出 0 边界"), LoopTimes.ContainsByPredicate([](float Time) { return FMath::IsNearlyZero(Time); }));
	TestTrue(TEXT("越过 0 后应从末端消化溢出时间"), LoopTimes.ContainsByPredicate([](float Time) { return FMath::IsNearlyEqual(Time, 0.75f); }));

	TArray<float> ForwardLoopTimes;
	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[&ForwardLoopTimes](float Value, float Time) { ForwardLoopTimes.Add(Time); },
		[](float Value, float Time, bool bStopped) {},
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, ECF_LOOP, EECFPlayDirection::Forward);
	TestWorld.Tick(0.1f);
	TestWorld.Tick(1.f);
	TestWorld.Tick(0.25f);
	int32 ForwardEndTickCount = 0;
	for (const float Time : ForwardLoopTimes)
	{
		ForwardEndTickCount += FMath::IsNearlyEqual(Time, 1.f) ? 1 : 0;
	}
	TestEqual(TEXT("从精确终点继续循环时不应重复输出终点 Tick"), ForwardEndTickCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FECFTimelineEventTrackTest,
	"XTools.EnhancedCodeFlow.Timeline.EventTrack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FECFTimelineEventTrackTest::RunTest(const FString& Parameters)
{
	FScopedECFTestWorld TestWorld(TEXT("ECFTimelineEventTrackTest"));
	if (!TestTrue(TEXT("应创建 ECF 测试世界和子系统"), TestWorld.IsValid()))
	{
		return false;
	}

	TArray<FECFTimelineEvent> Events;
	Events.Add({0.25f, TEXT("Quarter")});
	Events.Add({0.75f, TEXT("ThreeQuarter")});
	TArray<FName> ForwardNames;
	TArray<float> ForwardTimes;
	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[](float Value, float Time) {},
		[](float Value, float Time, bool bStopped) {},
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Forward, Events,
		[&ForwardNames, &ForwardTimes](FName Name, float Time)
		{
			ForwardNames.Add(Name);
			ForwardTimes.Add(Time);
		});

	TestWorld.Tick(0.1f);
	TestWorld.Tick(0.8f);
	TestEqual(TEXT("单次大步进应触发所有跨越的事件"), ForwardNames.Num(), 2);
	TestTrue(TEXT("正向事件应按时间顺序触发"), ForwardNames == TArray<FName>({TEXT("Quarter"), TEXT("ThreeQuarter")}));
	TestTrue(TEXT("正向事件应传回事件时间"), ForwardTimes == TArray<float>({0.25f, 0.75f}));

	TArray<FName> ReverseNames;
	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[](float Value, float Time) {},
		[](float Value, float Time, bool bStopped) {},
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Reverse, Events,
		[&ReverseNames](FName Name, float Time) { ReverseNames.Add(Name); });
	TestWorld.Tick(0.1f);
	TestWorld.Tick(0.8f);
	TestTrue(TEXT("反向事件应按倒放顺序触发"), ReverseNames == TArray<FName>({TEXT("ThreeQuarter"), TEXT("Quarter")}));

	TArray<FName> StableNames;
	TArray<FECFTimelineEvent> StableEvents;
	StableEvents.Add({0.5f, TEXT("FirstAtHalf")});
	StableEvents.Add({0.5f, TEXT("SecondAtHalf")});
	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[](float Value, float Time) {},
		[](float Value, float Time, bool bStopped) {},
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Forward, MoveTemp(StableEvents),
		[&StableNames](FName Name, float Time) { StableNames.Add(Name); });
	TestWorld.Tick(0.1f);
	TestWorld.Tick(0.5f);
	TestWorld.Tick(0.1f);
	TestEqual(TEXT("同一时间点事件应全部触发"), StableNames.Num(), 2);
	if (StableNames.Num() == 2)
	{
		TestEqual(TEXT("同一时间点第一个事件应保持输入顺序"), StableNames[0], FName(TEXT("FirstAtHalf")));
		TestEqual(TEXT("同一时间点第二个事件应保持输入顺序"), StableNames[1], FName(TEXT("SecondAtHalf")));
	}

	TArray<FName> ReverseStableNames;
	TArray<FECFTimelineEvent> ReverseStableEvents;
	ReverseStableEvents.Add({0.5f, TEXT("FirstAtHalf")});
	ReverseStableEvents.Add({0.5f, TEXT("SecondAtHalf")});
	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[](float Value, float Time) {},
		[](float Value, float Time, bool bStopped) {},
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Reverse, MoveTemp(ReverseStableEvents),
		[&ReverseStableNames](FName Name, float Time) { ReverseStableNames.Add(Name); });
	TestWorld.Tick(0.1f);
	TestWorld.Tick(0.6f);
	TestTrue(TEXT("反向同一时间点事件也应保持输入顺序"),
		ReverseStableNames == TArray<FName>({TEXT("FirstAtHalf"), TEXT("SecondAtHalf")}));

	UCurveFloat* NonZeroCurve = NewObject<UCurveFloat>(TestWorld.GetWorld());
	NonZeroCurve->FloatCurve.AddKey(2.f, 10.f);
	NonZeroCurve->FloatCurve.AddKey(3.f, 20.f);
	int32 NonZeroEventCount = 0;
	TArray<FECFTimelineEvent> NonZeroEvents;
	NonZeroEvents.Add({2.f, TEXT("CurveStart")});
	FFlow::AddCustomTimeline(TestWorld.GetWorld(), NonZeroCurve,
		[](float Value, float Time) {},
		[](float Value, float Time, bool bStopped) {},
		1.f, {}, EECFPlayDirection::Forward, MoveTemp(NonZeroEvents),
		[&NonZeroEventCount](FName Name, float Time) { ++NonZeroEventCount; });
	TestWorld.Tick(0.1f);
	TestWorld.Tick(2.5f);
	TestEqual(TEXT("非零起始键曲线的事件应使用原生时间轴坐标"), NonZeroEventCount, 1);

	TArray<FName> MultiLoopNames;
	TArray<FECFTimelineEvent> MultiLoopEvents;
	MultiLoopEvents.Add({0.25f, TEXT("LoopQuarter")});
	FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[](float Value, float Time) {},
		[](float Value, float Time, bool bStopped) {},
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, ECF_LOOP, EECFPlayDirection::Forward, MoveTemp(MultiLoopEvents),
		[&MultiLoopNames](FName Name, float Time) { MultiLoopNames.Add(Name); });
	TestWorld.Tick(0.1f);
	TestWorld.Tick(0.1f);
	TestWorld.Tick(2.5f);
	TestEqual(TEXT("单次 Tick 跨越多圈时每圈均应触发事件"), MultiLoopNames.Num(), 3);

	TArray<FECFTimelineEvent> GuardedEvents;
	GuardedEvents.Add({0.25f, TEXT("Guarded")});
	ECFPrepareTimelineEvents(GuardedEvents);
	int32 GuardedDispatchCount = 0;
	FECFTimelineEventFunc GuardedEventFunc = [&GuardedDispatchCount](FName Name, float Time)
	{
		++GuardedDispatchCount;
	};
	const EECFTimelineEventDispatchResult GuardedResult =
		ECFDispatchTimelineEvents(0.f, 2000.f, 1.f, true, false, GuardedEvents, GuardedEventFunc);
	TestTrue(TEXT("极端步进应明确报告达到分段上限"),
		GuardedResult == EECFTimelineEventDispatchResult::SegmentLimitReached);
	TestEqual(TEXT("极端步进应限制单 Tick 循环分段数量"), GuardedDispatchCount, ECFMaxTimelineEventSegmentsPerTick);

	TArray<FECFTimelineEvent> ForwardEndpointEvents;
	ForwardEndpointEvents.Add({1.f, TEXT("ForwardEnd")});
	ECFPrepareTimelineEvents(ForwardEndpointEvents);
	int32 ForwardEndpointCount = 0;
	FECFTimelineEventFunc ForwardEndpointFunc = [&ForwardEndpointCount](FName Name, float Time)
	{
		++ForwardEndpointCount;
	};
	ECFDispatchTimelineEvents(0.f, 1.f, 1.f, true, false, ForwardEndpointEvents, ForwardEndpointFunc);
	ECFDispatchTimelineEvents(1.f, 1.25f, 1.f, true, false, ForwardEndpointEvents, ForwardEndpointFunc);
	TestEqual(TEXT("从精确终点继续正向循环时不应重复派发终点事件"), ForwardEndpointCount, 1);

	TArray<FECFTimelineEvent> ReverseEndpointEvents;
	ReverseEndpointEvents.Add({0.f, TEXT("ReverseEnd")});
	ECFPrepareTimelineEvents(ReverseEndpointEvents);
	int32 ReverseEndpointCount = 0;
	FECFTimelineEventFunc ReverseEndpointFunc = [&ReverseEndpointCount](FName Name, float Time)
	{
		++ReverseEndpointCount;
	};
	ECFDispatchTimelineEvents(1.f, 0.f, 1.f, true, true, ReverseEndpointEvents, ReverseEndpointFunc);
	ECFDispatchTimelineEvents(0.f, -0.25f, 1.f, true, true, ReverseEndpointEvents, ReverseEndpointFunc);
	TestEqual(TEXT("从精确起点继续反向循环时不应重复派发起点事件"), ReverseEndpointCount, 1);

	UCurveFloat* FinalOrderCurve = NewObject<UCurveFloat>(TestWorld.GetWorld());
	AddLinearKeys(FinalOrderCurve);
	TArray<FName> FinalOrder;
	int32 FinalTimeTickCount = 0;
	TArray<FECFTimelineEvent> FinalOrderEvents;
	FinalOrderEvents.Add({0.75f, TEXT("Event")});
	FFlow::AddCustomTimeline(TestWorld.GetWorld(), FinalOrderCurve,
		[&FinalOrder, &FinalTimeTickCount](float Value, float Time)
		{
			if (FMath::IsNearlyEqual(Time, 1.f))
			{
				++FinalTimeTickCount;
				FinalOrder.Add(TEXT("Tick"));
			}
		},
		[&FinalOrder](float Value, float Time, bool bStopped) { FinalOrder.Add(TEXT("Finished")); },
		1.f, {}, EECFPlayDirection::Forward, MoveTemp(FinalOrderEvents),
		[&FinalOrder](FName Name, float Time) { FinalOrder.Add(Name); });
	TestWorld.Tick(0.1f);
	TestWorld.Tick(1.1f);
	TestEqual(TEXT("曲线时间轴自然结束只应输出一次终点 Tick"), FinalTimeTickCount, 1);
	TestTrue(TEXT("曲线时间轴结束帧应依次输出 Tick、事件和完成回调"),
		FinalOrder == TArray<FName>({TEXT("Tick"), TEXT("Event"), TEXT("Finished")}));

	int32 ReentrantEventCount = 0;
	int32 ReentrantFinishedCount = 0;
	FECFHandle ReentrantHandle;
	TArray<FECFTimelineEvent> ReentrantEvents;
	ReentrantEvents.Add({0.25f, TEXT("Stop")});
	ReentrantEvents.Add({0.75f, TEXT("MustNotRun")});
	ReentrantHandle = FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[](float Value, float Time) {},
		[&ReentrantFinishedCount](float Value, float Time, bool bStopped) { ++ReentrantFinishedCount; },
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Forward, MoveTemp(ReentrantEvents),
		[&TestWorld, &ReentrantHandle, &ReentrantEventCount](FName Name, float Time)
		{
			++ReentrantEventCount;
			FFlow::StopAction(TestWorld.GetWorld(), ReentrantHandle, false);
		});
	TestWorld.Tick(0.1f);
	TestWorld.Tick(1.f);
	TestEqual(TEXT("事件回调取消动作后不应继续派发同 Tick 事件"), ReentrantEventCount, 1);
	TestEqual(TEXT("事件回调取消动作后不应再触发自然完成"), ReentrantFinishedCount, 0);

	int32 TickCancelledFinishedCount = 0;
	bool bCancelOnTick = false;
	FECFHandle TickCancelledHandle;
	TickCancelledHandle = FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[&TestWorld, &TickCancelledHandle, &bCancelOnTick](float Value, float Time)
		{
			if (bCancelOnTick)
			{
				FFlow::StopAction(TestWorld.GetWorld(), TickCancelledHandle, false);
			}
		},
		[&TickCancelledFinishedCount](float Value, float Time, bool bStopped) { ++TickCancelledFinishedCount; });
	TestWorld.Tick(0.1f);
	bCancelOnTick = true;
	TestWorld.Tick(1.f);
	TestEqual(TEXT("Tick 回调取消动作后不应再触发自然完成"), TickCancelledFinishedCount, 0);

	int32 JumpEventCount = 0;
	const FECFHandle JumpHandle = FFlow::AddTimeline(TestWorld.GetWorld(), 0.f, 1.f, 1.f,
		[](float Value, float Time) {},
		[](float Value, float Time, bool bStopped) {},
		EECFBlendFunc::ECFBlend_Linear, 1.f, 1.f, {}, EECFPlayDirection::Forward, Events,
		[&JumpEventCount](FName Name, float Time) { ++JumpEventCount; });
	TestTrue(TEXT("设置时间应成功"), FFlow::SetActionTime(TestWorld.GetWorld(), JumpHandle, 0.8f, false));
	TestEqual(TEXT("显式设置时间默认不触发事件"), JumpEventCount, 0);

	return true;
}

#endif
