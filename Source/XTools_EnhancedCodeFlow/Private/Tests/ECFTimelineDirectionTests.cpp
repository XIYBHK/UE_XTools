// Copyright (c) 2026 Damian Nowakowski. All rights reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "EnhancedCodeFlow.h"
#include "ECFSubsystem.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tickable.h"
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

	return true;
}

#endif
