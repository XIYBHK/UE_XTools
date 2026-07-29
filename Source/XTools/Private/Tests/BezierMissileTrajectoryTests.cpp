/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_DEV_AUTOMATION_TESTS

#include "XToolsLibrary.h"
#include "Curves/CurveFloat.h"
#include "Math/RotationMatrix.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

namespace
{
    FBezierNoiseOptions MakeDisabledNoise()
    {
        FBezierNoiseOptions Options;
        Options.Amplitude = FVector2D::ZeroVector;
        return Options;
    }

    bool IsFiniteVector(const FVector& Vector)
    {
        return !Vector.ContainsNaN();
    }

    FVector EvaluateBezierReference(const TArray<FVector>& Points, float Parameter)
    {
        if (Points.IsEmpty())
        {
            return FVector::ZeroVector;
        }

        TArray<FVector> WorkingPoints = Points;
        for (int32 RemainingPoints = WorkingPoints.Num() - 1; RemainingPoints > 0; --RemainingPoints)
        {
            for (int32 Index = 0; Index < RemainingPoints; ++Index)
            {
                WorkingPoints[Index] = FMath::Lerp(
                    WorkingPoints[Index], WorkingPoints[Index + 1], Parameter);
            }
        }
        return WorkingPoints[0];
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_ControlPointConstruction,
    "XTools.Bezier.MissileTrajectory.ControlPointConstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_ControlPointConstruction::RunTest(const FString& Parameters)
{
    const FVector StartLocation(10.0, 20.0, 30.0);
    const FVector TargetLocation(100.0, 200.0, 300.0);
    const TArray<FVector> Points = UXToolsLibrary::BuildBezierMissileControlPoints(
        StartLocation, FVector(2.0, 0.0, 0.0),
        TargetLocation, FVector(0.0, 0.0, 5.0),
        25.0f, 40.0f);

    TestEqual(TEXT("应生成四个三次贝塞尔控制点"), Points.Num(), 4);
    if (Points.Num() == 4)
    {
        TestTrue(TEXT("P0应等于起点"), Points[0].Equals(StartLocation));
        TestTrue(TEXT("P1应沿单位化起点方向放置"),
            Points[1].Equals(StartLocation + FVector::ForwardVector * 25.0));
        TestTrue(TEXT("P2应沿单位化目标法线放置"),
            Points[2].Equals(TargetLocation + FVector::UpVector * 40.0));
        TestTrue(TEXT("P3应等于目标点"), Points[3].Equals(TargetLocation));
    }

    const FVector CoincidentLocation(50.0, -20.0, 10.0);
    const TArray<FVector> DegeneratePoints = UXToolsLibrary::BuildBezierMissileControlPoints(
        CoincidentLocation, FVector::ZeroVector,
        CoincidentLocation, FVector::ZeroVector,
        100.0f, 100.0f);
    TestEqual(TEXT("退化方向仍应生成四个控制点"), DegeneratePoints.Num(), 4);
    for (const FVector& Point : DegeneratePoints)
    {
        TestTrue(TEXT("退化方向回退后的控制点应保持有限"), IsFiniteVector(Point));
    }

    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const TArray<FVector> InvalidInputPoints = UXToolsLibrary::BuildBezierMissileControlPoints(
        StartLocation, FVector(NaN, 0.0, 0.0),
        TargetLocation, FVector::ZeroVector,
        -10.0f, std::numeric_limits<float>::infinity());
    TestTrue(TEXT("无效起点控制距离应收缩到起点"), InvalidInputPoints[1].Equals(StartLocation));
    TestTrue(TEXT("无效目标控制距离应收缩到目标点"), InvalidInputPoints[2].Equals(TargetLocation));
    for (const FVector& Point : InvalidInputPoints)
    {
        TestTrue(TEXT("非有限方向和距离不应污染控制点"), IsFiniteVector(Point));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_StateReset,
    "XTools.Bezier.MissileTrajectory.StateReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_StateReset::RunTest(const FString& Parameters)
{
    FBezierRotationFrameState Frame;
    Frame.bInitialized = true;
    Frame.PreviousCurvePosition = FVector(10.0, 20.0, 30.0);
    Frame.PreviousTangent = FVector::UpVector;
    Frame.PreviousNormal = FVector::RightVector;
    Frame.PreviousOutputPosition = FVector(40.0, 50.0, 60.0);
    Frame.CachedArcLengthControlPoints.Add(FVector::ZeroVector);
    Frame.CachedCumulativeArcLengths.Add(0.0f);
    Frame.CachedTotalArcLength = 100.0f;
    Frame.ArcLengthCacheBuildGeneration = 1;

    UXToolsLibrary::ResetBezierMissileTrajectoryState(Frame, FVector(0.0, 3.0, 4.0));
    TestFalse(TEXT("重置后状态应标记为未初始化"), Frame.bInitialized);
    TestTrue(TEXT("重置后上一曲线位置应清零"), Frame.PreviousCurvePosition.IsZero());
    TestTrue(TEXT("重置后上一切线应回到前方向"), Frame.PreviousTangent.Equals(FVector::ForwardVector));
    TestTrue(TEXT("重置后上一最终位置应清零"), Frame.PreviousOutputPosition.IsZero());
    TestTrue(TEXT("重置后应清空匀速控制点缓存"), Frame.CachedArcLengthControlPoints.IsEmpty());
    TestTrue(TEXT("重置后应清空累计弧长缓存"), Frame.CachedCumulativeArcLengths.IsEmpty());
    TestEqual(TEXT("重置后总弧长应清零"), Frame.CachedTotalArcLength, 0.0f);
    TestEqual(TEXT("重置后缓存构建代数应清零"), Frame.ArcLengthCacheBuildGeneration, 0);
    TestTrue(TEXT("自定义初始上方向应单位化"),
        Frame.PreviousNormal.Equals(FVector(0.0, 0.6, 0.8), UE_KINDA_SMALL_NUMBER));

    UXToolsLibrary::ResetBezierMissileTrajectoryState(Frame, FVector::ZeroVector);
    TestTrue(TEXT("零初始上方向应回退到世界上方向"), Frame.PreviousNormal.Equals(FVector::UpVector));

    const double NaN = std::numeric_limits<double>::quiet_NaN();
    UXToolsLibrary::ResetBezierMissileTrajectoryState(Frame, FVector(NaN, 0.0, 0.0));
    TestTrue(TEXT("非有限初始上方向应回退到世界上方向"), Frame.PreviousNormal.Equals(FVector::UpVector));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_ConstantSpeedCacheLifecycle,
    "XTools.Bezier.MissileTrajectory.ConstantSpeedCacheLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_ConstantSpeedCacheLifecycle::RunTest(const FString& Parameters)
{
    const TArray<FVector> Points = {
        FVector(0.0, 0.0, 0.0),
        FVector(250.0, 100.0, 300.0),
        FVector(750.0, -100.0, 300.0),
        FVector(1000.0, 0.0, 0.0)
    };
    FBezierSpeedOptions ConstantSpeedOptions;
    ConstantSpeedOptions.SpeedMode = EBezierSpeedMode::Constant;
    const FBezierDebugColors DebugColors;
    FBezierRotationFrameState Frame;
    FVector Position;
    FVector Tangent;
    FRotator Rotation;

    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.25f, 0.25f, Frame,
        Position, Tangent, Rotation,
        MakeDisabledNoise(), ConstantSpeedOptions, false, 0.03f, DebugColors);
    TestEqual(TEXT("首次匀速计算应建立一次缓存"), Frame.ArcLengthCacheBuildGeneration, 1);
    TestEqual(TEXT("缓存应包含首尾在内的101个累计弧长样本"),
        Frame.CachedCumulativeArcLengths.Num(), 101);
    TestTrue(TEXT("缓存应记录当前控制点"), Frame.CachedArcLengthControlPoints == Points);
    TestTrue(TEXT("缓存总弧长应为有限正数"),
        FMath::IsFinite(Frame.CachedTotalArcLength) && Frame.CachedTotalArcLength > 0.0f);

    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.75f, 0.75f, Frame,
        Position, Tangent, Rotation,
        MakeDisabledNoise(), ConstantSpeedOptions, false, 0.03f, DebugColors);
    TestEqual(TEXT("控制点不变时应复用缓存"), Frame.ArcLengthCacheBuildGeneration, 1);

    TArray<FVector> ChangedPoints = Points;
    ChangedPoints[2].Z += 50.0;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, ChangedPoints, 0.75f, 0.75f, Frame,
        Position, Tangent, Rotation,
        MakeDisabledNoise(), ConstantSpeedOptions, false, 0.03f, DebugColors);
    TestEqual(TEXT("控制点变化时应重建缓存"), Frame.ArcLengthCacheBuildGeneration, 2);
    TestTrue(TEXT("重建后应记录新的控制点"), Frame.CachedArcLengthControlPoints == ChangedPoints);

    FBezierRotationFrameState DefaultModeFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.5f, 0.5f, DefaultModeFrame,
        Position, Tangent, Rotation,
        MakeDisabledNoise(), FBezierSpeedOptions(), false, 0.03f, DebugColors);
    TestEqual(TEXT("默认速度模式不应建立弧长缓存"),
        DefaultModeFrame.ArcLengthCacheBuildGeneration, 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_FastEvaluationAndDefaultProgressCurve,
    "XTools.Bezier.MissileTrajectory.FastEvaluationAndDefaultProgressCurve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_FastEvaluationAndDefaultProgressCurve::RunTest(const FString& Parameters)
{
    const TArray<TArray<FVector>> Curves = {
        { FVector(0.0, 0.0, 0.0), FVector(100.0, 20.0, 10.0) },
        { FVector(0.0, 0.0, 0.0), FVector(40.0, 80.0, 20.0), FVector(100.0, 0.0, 50.0) },
        { FVector(0.0, 0.0, 0.0), FVector(20.0, 100.0, 30.0), FVector(80.0, -50.0, 70.0), FVector(100.0, 0.0, 0.0) },
        { FVector(0.0, 0.0, 0.0), FVector(10.0, 80.0, 20.0), FVector(40.0, -30.0, 60.0), FVector(70.0, 40.0, 20.0), FVector(100.0, 0.0, 0.0) }
    };
    const TArray<float> SampleParameters = { 0.0f, 0.17f, 0.5f, 0.83f, 1.0f };
    const FBezierDebugColors DebugColors;
    for (const TArray<FVector>& Curve : Curves)
    {
        for (const float Parameter : SampleParameters)
        {
            const FVector Actual = UXToolsLibrary::CalculateBezierPoint(
                Curve, Parameter, FBezierSpeedOptions());
            const FVector Expected = EvaluateBezierReference(Curve, Parameter);
            TestTrue(TEXT("无调试快速求值应与De Casteljau参考结果一致"),
                Actual.Equals(Expected, UE_KINDA_SMALL_NUMBER));
        }
    }

    UCurveFloat* ProgressCurve = NewObject<UCurveFloat>();
    ProgressCurve->FloatCurve.AddKey(0.0f, 0.0f);
    ProgressCurve->FloatCurve.AddKey(0.5f, 0.25f);
    ProgressCurve->FloatCurve.AddKey(1.0f, 1.0f);

    FBezierSpeedOptions DefaultSpeedOptions;
    DefaultSpeedOptions.SpeedCurve = ProgressCurve;
    const FVector MappedPoint = UXToolsLibrary::CalculateBezierPoint(
        Curves[3], 0.5f, DefaultSpeedOptions);
    TestTrue(TEXT("基础节点在默认速度模式下应应用进度映射曲线"),
        MappedPoint.Equals(EvaluateBezierReference(Curves[3], 0.25f), UE_KINDA_SMALL_NUMBER));

    const float NaN = std::numeric_limits<float>::quiet_NaN();
    TestTrue(TEXT("非有限进度应安全回退到曲线起点"),
        UXToolsLibrary::CalculateBezierPoint(Curves[3], NaN, FBezierSpeedOptions()).Equals(Curves[3][0]));
    TestEqual(TEXT("空控制点应返回零向量"),
        UXToolsLibrary::CalculateBezierPoint({}, 0.5f, FBezierSpeedOptions()), FVector::ZeroVector);
    const TArray<FVector> SinglePoint = { FVector(12.0, 34.0, 56.0) };
    TestEqual(TEXT("单控制点应返回该控制点"),
        UXToolsLibrary::CalculateBezierPoint(SinglePoint, 0.5f, FBezierSpeedOptions()), SinglePoint[0]);

    FBezierRotationFrameState Frame;
    FVector Position;
    FVector Tangent;
    FRotator Rotation;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Curves[3], 0.5f, 0.5f, Frame,
        Position, Tangent, Rotation,
        MakeDisabledNoise(), DefaultSpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("默认速度模式也应将进度映射曲线应用于贝塞尔参数"),
        Position.Equals(EvaluateBezierReference(Curves[3], 0.25f), UE_KINDA_SMALL_NUMBER));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierPoint_BlueprintNodeSemantics,
    "XTools.Bezier.Point.BlueprintNodeSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierPoint_BlueprintNodeSemantics::RunTest(const FString& Parameters)
{
    const UFunction* PureFunction = UXToolsLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UXToolsLibrary, CalculateBezierPoint));
    TestNotNull(TEXT("应找到贝塞尔纯计算函数"), PureFunction);
    if (PureFunction)
    {
        TestTrue(TEXT("计算节点应为BlueprintPure"),
            PureFunction->HasAnyFunctionFlags(FUNC_BlueprintPure));
#if WITH_EDITOR
        TestFalse(TEXT("计算节点不应声明WorldContext"),
            PureFunction->HasMetaData(TEXT("WorldContext")));
#endif
    }

    const UFunction* DebugFunction = UXToolsLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UXToolsLibrary, CalculateAndDrawBezierPoint));
    TestNotNull(TEXT("应找到贝塞尔调试绘制函数"), DebugFunction);
    if (DebugFunction)
    {
        TestTrue(TEXT("调试节点应为BlueprintCallable"),
            DebugFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
        TestFalse(TEXT("调试节点不应为BlueprintPure"),
            DebugFunction->HasAnyFunctionFlags(FUNC_BlueprintPure));
#if WITH_EDITOR
        TestEqual(TEXT("调试节点应声明Context为WorldContext"),
            DebugFunction->GetMetaData(TEXT("WorldContext")), FString(TEXT("Context")));
#endif
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_BaseCurveDeterminismAndEndpoints,
    "XTools.Bezier.MissileTrajectory.BaseCurveDeterminismAndEndpoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_BaseCurveDeterminismAndEndpoints::RunTest(const FString& Parameters)
{
    const TArray<FVector> Points = {
        FVector(0.0, 0.0, 0.0),
        FVector(300.0, 100.0, 200.0),
        FVector(700.0, -150.0, 400.0),
        FVector(1000.0, 0.0, 0.0)
    };
    const FBezierSpeedOptions SpeedOptions;
    const FBezierDebugColors DebugColors;

    FVector BasePosition;
    FVector BaseTangent;
    FRotator BaseRotation;
    FBezierRotationFrameState BaseFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.37f, 1.25f, BaseFrame,
        BasePosition, BaseTangent, BaseRotation,
        MakeDisabledNoise(), SpeedOptions, false, 0.03f, DebugColors);

    const FVector ExistingPosition = UXToolsLibrary::CalculateBezierPoint(
        Points, 0.37f, SpeedOptions);
    TestTrue(TEXT("关闭噪声时应与现有贝塞尔节点位置一致"),
        BasePosition.Equals(ExistingPosition, UE_KINDA_SMALL_NUMBER));

    FBezierNoiseOptions NoiseOptions;
    FVector FirstPosition;
    FVector FirstTangent;
    FRotator FirstRotation;
    FBezierRotationFrameState FirstFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.37f, 1.25f, FirstFrame,
        FirstPosition, FirstTangent, FirstRotation,
        NoiseOptions, SpeedOptions, false, 0.03f, DebugColors);

    FVector SecondPosition;
    FVector SecondTangent;
    FRotator SecondRotation;
    FBezierRotationFrameState SecondFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.37f, 1.25f, SecondFrame,
        SecondPosition, SecondTangent, SecondRotation,
        NoiseOptions, SpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("相同时间、种子和状态应生成确定性位置"), FirstPosition.Equals(SecondPosition));
    TestTrue(TEXT("相同输入应生成确定性标架法线"),
        FirstFrame.PreviousNormal.Equals(SecondFrame.PreviousNormal));

    FVector EndpointPosition;
    FVector EndpointTangent;
    FRotator EndpointRotation;
    FBezierRotationFrameState EndpointFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.0f, 3.0f, EndpointFrame,
        EndpointPosition, EndpointTangent, EndpointRotation,
        NoiseOptions, SpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("起点噪声包络应严格为零"), EndpointPosition.Equals(Points[0]));

    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 1.0f, 3.0f, EndpointFrame,
        EndpointPosition, EndpointTangent, EndpointRotation,
        NoiseOptions, SpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("终点噪声包络应严格为零"), EndpointPosition.Equals(Points.Last()));

    const TArray<FVector> HigherOrderPoints = {
        FVector(0.0, 0.0, 0.0),
        FVector(100.0, 250.0, 50.0),
        FVector(400.0, -100.0, 300.0),
        FVector(700.0, 200.0, 100.0),
        FVector(1000.0, 0.0, 0.0)
    };
    FBezierSpeedOptions ConstantSpeedOptions;
    ConstantSpeedOptions.SpeedMode = EBezierSpeedMode::Constant;
    BaseFrame = FBezierRotationFrameState();
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, HigherOrderPoints, 0.42f, 1.0f, BaseFrame,
        BasePosition, BaseTangent, BaseRotation,
        MakeDisabledNoise(), ConstantSpeedOptions, false, 0.03f, DebugColors);
    const FVector ExistingConstantSpeedPosition = UXToolsLibrary::CalculateBezierPoint(
        HigherOrderPoints, 0.42f, ConstantSpeedOptions);
    TestTrue(TEXT("任意阶匀速模式应与现有节点位置一致"),
        BasePosition.Equals(ExistingConstantSpeedPosition, UE_KINDA_SMALL_NUMBER));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_ConstantSpeedNoiseEnvelopeUsesMotionProgress,
    "XTools.Bezier.MissileTrajectory.ConstantSpeedNoiseEnvelopeUsesMotionProgress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_ConstantSpeedNoiseEnvelopeUsesMotionProgress::RunTest(const FString& Parameters)
{
    const TArray<FVector> Points = {
        FVector(0.0, 0.0, 0.0),
        FVector(10000.0, 0.0, 0.0),
        FVector(10000.0, 0.0, 0.0),
        FVector(11000.0, 0.0, 0.0)
    };

    UCurveFloat* SpeedCurve = NewObject<UCurveFloat>();
    SpeedCurve->FloatCurve.AddKey(0.0f, 0.0f);
    SpeedCurve->FloatCurve.AddKey(0.25f, 0.5f);
    SpeedCurve->FloatCurve.AddKey(1.0f, 1.0f);

    FBezierSpeedOptions SpeedOptions;
    SpeedOptions.SpeedMode = EBezierSpeedMode::Constant;
    SpeedOptions.SpeedCurve = SpeedCurve;

    const float ElapsedTime = 0.37f;
    const FBezierDebugColors DebugColors;
    FVector BasePosition;
    FVector Tangent;
    FRotator Rotation;
    FBezierRotationFrameState BaseFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.25f, ElapsedTime, BaseFrame,
        BasePosition, Tangent, Rotation,
        MakeDisabledNoise(), SpeedOptions, false, 0.03f, DebugColors);

    FBezierNoiseOptions NoiseOptions = MakeDisabledNoise();
    NoiseOptions.Amplitude.X = 100.0;
    NoiseOptions.Frequency = 2.0f;
    NoiseOptions.SeedX = 17;
    FVector NoisyPosition;
    FBezierRotationFrameState NoisyFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.25f, ElapsedTime, NoisyFrame,
        NoisyPosition, Tangent, Rotation,
        NoiseOptions, SpeedOptions, false, 0.03f, DebugColors);

    const FVector FrameRight = FVector::CrossProduct(
        BaseFrame.PreviousNormal, BaseFrame.PreviousTangent).GetSafeNormal();
    const float NoiseSample = FMath::PerlinNoise2D(
        FVector2D(NoiseOptions.Frequency * ElapsedTime, static_cast<float>(NoiseOptions.SeedX)));
    const double ExpectedOffset = NoiseOptions.Amplitude.X * NoiseSample;
    const double ActualOffset = FVector::DotProduct(NoisyPosition - BasePosition, FrameRight);

    TestTrue(TEXT("测试噪声采样应非零"), FMath::Abs(ExpectedOffset) > 1.0);
    TestTrue(TEXT("速率曲线后的运动中点应使用完整噪声包络"),
        FMath::IsNearlyEqual(ActualOffset, ExpectedOffset, 1.e-3));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_NoisyRotationFollowsMovement,
    "XTools.Bezier.MissileTrajectory.NoisyRotationFollowsMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_NoisyRotationFollowsMovement::RunTest(const FString& Parameters)
{
    const TArray<FVector> Points = {
        FVector(0.0, 0.0, 0.0),
        FVector(300.0, 0.0, 0.0),
        FVector(700.0, 0.0, 0.0),
        FVector(1000.0, 0.0, 0.0)
    };
    FBezierNoiseOptions NoiseOptions = MakeDisabledNoise();
    NoiseOptions.Amplitude.X = 500.0;
    NoiseOptions.Frequency = 2.0f;
    NoiseOptions.SeedX = 17;

    const FBezierSpeedOptions SpeedOptions;
    const FBezierDebugColors DebugColors;
    FBezierRotationFrameState Frame;
    FVector PreviousPosition;
    FVector Tangent;
    FRotator Rotation;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.40f, 0.37f, Frame,
        PreviousPosition, Tangent, Rotation,
        NoiseOptions, SpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("首帧应使用基准曲线切线"),
        FVector::DotProduct(Tangent, FVector::ForwardVector) > 0.9999);

    FVector CurrentPosition;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, Points, 0.42f, 0.50f, Frame,
        CurrentPosition, Tangent, Rotation,
        NoiseOptions, SpeedOptions, false, 0.03f, DebugColors);

    const FVector ActualMovementDirection = (CurrentPosition - PreviousPosition).GetSafeNormal();
    const FVector RotationForward = Rotation.Vector();
    const FVector RotationUp = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
    TestTrue(TEXT("测试轨迹应产生偏离基准曲线的实际移动方向"),
        FVector::DotProduct(ActualMovementDirection, FVector::ForwardVector) < 0.999);
    TestTrue(TEXT("输出切线应跟随最终噪声位置的帧间移动方向"),
        FVector::DotProduct(Tangent, ActualMovementDirection) > 0.9999);
    TestTrue(TEXT("输出旋转前向应跟随最终噪声位置的帧间移动方向"),
        FVector::DotProduct(RotationForward, ActualMovementDirection) > 0.9999);
    TestTrue(TEXT("输出旋转上方向应与实际移动方向正交"),
        FMath::Abs(FVector::DotProduct(RotationUp, ActualMovementDirection)) < 1.e-4);
    TestTrue(TEXT("状态应保存本帧最终噪声位置"),
        Frame.PreviousOutputPosition.Equals(CurrentPosition));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_IndependentNoiseAxesAndVerticalFrame,
    "XTools.Bezier.MissileTrajectory.IndependentNoiseAxesAndVerticalFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_IndependentNoiseAxesAndVerticalFrame::RunTest(const FString& Parameters)
{
    const TArray<FVector> VerticalPoints = {
        FVector(0.0, 0.0, 0.0),
        FVector(0.0, 0.0, 300.0),
        FVector(20.0, 0.0, 700.0),
        FVector(0.0, 0.0, 1000.0)
    };
    const FBezierSpeedOptions SpeedOptions;
    const FBezierDebugColors DebugColors;
    FBezierRotationFrameState Frame;
    FVector PreviousNormal = FVector::ZeroVector;

    for (int32 Index = 0; Index <= 20; ++Index)
    {
        FVector Position;
        FVector Tangent;
        FRotator Rotation;
        const bool bHadPreviousFrame = Frame.bInitialized;
        UXToolsLibrary::CalculateBezierMissileTrajectory(
            nullptr, VerticalPoints, static_cast<float>(Index) / 20.0f,
            static_cast<float>(Index) * 0.05f, Frame,
            Position, Tangent, Rotation,
            MakeDisabledNoise(), SpeedOptions, false, 0.03f, DebugColors);

        TestTrue(TEXT("垂直曲线位置应保持有限"), IsFiniteVector(Position));
        TestTrue(TEXT("垂直曲线切线应保持有限且单位化"),
            IsFiniteVector(Tangent) && Tangent.IsNormalized());
        TestTrue(TEXT("RMF法线应保持有限且垂直于切线"),
            IsFiniteVector(Frame.PreviousNormal)
                && FMath::Abs(FVector::DotProduct(Tangent, Frame.PreviousNormal)) < 1.e-4);
        if (bHadPreviousFrame)
        {
            TestTrue(TEXT("连续RMF法线不应发生180度翻转"),
                FVector::DotProduct(PreviousNormal, Frame.PreviousNormal) > 0.0);
        }

        PreviousNormal = Frame.PreviousNormal;
    }

    const float TestProgress = 0.41f;
    const float TestElapsedTime = 0.37f;
    FVector BasePosition;
    FVector Tangent;
    FRotator Rotation;
    FBezierRotationFrameState BaseFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, VerticalPoints, TestProgress, TestElapsedTime, BaseFrame,
        BasePosition, Tangent, Rotation,
        MakeDisabledNoise(), SpeedOptions, false, 0.03f, DebugColors);

    FBezierNoiseOptions HorizontalNoise = MakeDisabledNoise();
    HorizontalNoise.Amplitude.X = 100.0;
    FVector HorizontalPosition;
    FBezierRotationFrameState HorizontalFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, VerticalPoints, TestProgress, TestElapsedTime, HorizontalFrame,
        HorizontalPosition, Tangent, Rotation,
        HorizontalNoise, SpeedOptions, false, 0.03f, DebugColors);

    FBezierNoiseOptions VerticalNoise = MakeDisabledNoise();
    VerticalNoise.Amplitude.Y = 100.0;
    FVector VerticalPosition;
    FBezierRotationFrameState VerticalFrame;
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, VerticalPoints, TestProgress, TestElapsedTime, VerticalFrame,
        VerticalPosition, Tangent, Rotation,
        VerticalNoise, SpeedOptions, false, 0.03f, DebugColors);

    const FVector FrameRight = FVector::CrossProduct(BaseFrame.PreviousNormal, BaseFrame.PreviousTangent).GetSafeNormal();
    const FVector HorizontalOffset = HorizontalPosition - BasePosition;
    const FVector VerticalOffset = VerticalPosition - BasePosition;
    TestTrue(TEXT("横向噪声通道应只沿RMF右轴偏移"),
        !HorizontalOffset.IsNearlyZero()
            && FMath::Abs(FVector::DotProduct(HorizontalOffset.GetSafeNormal(), FrameRight)) > 0.999);
    TestTrue(TEXT("纵向噪声通道应只沿RMF上轴偏移"),
        !VerticalOffset.IsNearlyZero()
            && FMath::Abs(FVector::DotProduct(VerticalOffset.GetSafeNormal(), BaseFrame.PreviousNormal)) > 0.999);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBezierMissileTrajectory_DegenerateInputsRemainFinite,
    "XTools.Bezier.MissileTrajectory.DegenerateInputsRemainFinite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBezierMissileTrajectory_DegenerateInputsRemainFinite::RunTest(const FString& Parameters)
{
    const FBezierSpeedOptions SpeedOptions;
    const FBezierDebugColors DebugColors;
    FBezierRotationFrameState Frame;
    FVector Position;
    FVector Tangent;
    FRotator Rotation;

    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, {}, 0.5f, 1.0f, Frame,
        Position, Tangent, Rotation,
        FBezierNoiseOptions(), SpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("空控制点数组应回退到原点"), Position.Equals(FVector::ZeroVector));
    TestTrue(TEXT("空控制点数组的标架应保持有限"),
        IsFiniteVector(Tangent) && IsFiniteVector(Frame.PreviousNormal));

    const TArray<FVector> SinglePoint = { FVector(10.0, 20.0, 30.0) };
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, SinglePoint, 0.5f, 1.0f, Frame,
        Position, Tangent, Rotation,
        FBezierNoiseOptions(), SpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("单控制点应保持现有节点回退位置"), Position.Equals(SinglePoint[0]));

    const TArray<FVector> RepeatedPoints = {
        FVector(100.0, 100.0, 100.0),
        FVector(100.0, 100.0, 100.0),
        FVector(100.0, 100.0, 100.0),
        FVector(100.0, 100.0, 100.0)
    };
    UXToolsLibrary::CalculateBezierMissileTrajectory(
        nullptr, RepeatedPoints, 0.5f, 1.0f, Frame,
        Position, Tangent, Rotation,
        FBezierNoiseOptions(), SpeedOptions, false, 0.03f, DebugColors);
    TestTrue(TEXT("全退化曲线位置和标架应保持有限"),
        IsFiniteVector(Position) && IsFiniteVector(Tangent) && IsFiniteVector(Frame.PreviousNormal));

    return true;
}

#endif
