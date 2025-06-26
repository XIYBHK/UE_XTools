// Fill out your copyright notice in the Description page of Project Settings.

#include "XToolsLibrary.h"
#include "XToolsPrivatePCH.h"
#include "RandomShuffleArrayLibrary.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionShape.h"

/**
 * 在组件层级中查找匹配指定类和标签的父Actor
 * 
 * @param Component 起始组件，必须是SceneComponent（因为需要访问GetAttachParent()来遍历组件层级）
 * @param ActorClass 要查找的Actor类（可选）
 * @param ActorTag 要匹配的标签（可选）
 * @return 找到的父Actor，未找到返回nullptr
 * 
 * 查找规则：
 * - 同时指定类和标签时，返回第一个同时匹配的父级
 * - 只指定类时，返回最高级匹配的父级
 * - 只指定标签时，返回第一个匹配的父级
 * - 都未指定时，返回最顶层的父级
 * 
 * 注意：最大查找深度为XTOOLS_MAX_PARENT_DEPTH（默认100层）
 */
AActor* UXToolsLibrary::GetTopmostAttachedActor(USceneComponent* StartComponent, TSubclassOf<AActor> ActorClass, FName ActorTag)
{
    if (!StartComponent)
    {
        UE_LOG(LogXTools, Warning, TEXT("GetTopmostAttachedActor: 提供的起始组件无效 (StartComponent is null)."));
        return nullptr;
    }

    AActor* HighestMatchingActor = nullptr;
    // 从起始组件的直接父级开始向上查找
    USceneComponent* CurrentComponent = StartComponent->GetAttachParent();
    int32 Iterations = 0;

    // 持续向上遍历，直到没有父级或达到最大深度
    while (CurrentComponent && Iterations < XTOOLS_MAX_PARENT_DEPTH)
    {
        AActor* OwnerActor = CurrentComponent->GetOwner();
        if (OwnerActor)
        {
            // 条件1: 检查类是否匹配 (如果ActorClass被指定)
            const bool bClassMatches = !ActorClass || OwnerActor->IsA(ActorClass);

            // 条件2: 检查标签是否匹配 (如果ActorTag被指定)
            const bool bTagMatches = ActorTag.IsNone() || OwnerActor->ActorHasTag(ActorTag);

            if (bClassMatches && bTagMatches)
            {
                // 这是一个有效的匹配项，记录下来。
                // 由于我们持续向上查找，这个变量会被任何更高层级的匹配项覆盖。
                HighestMatchingActor = OwnerActor;
            }
        }

        CurrentComponent = CurrentComponent->GetAttachParent();
        Iterations++;
    }

    return HighestMatchingActor;
}

FVector UXToolsLibrary::CalculateBezierPoint(const UObject* Context,const TArray<FVector>& Points, float Progress, bool bShowDebug, float Duration, FBezierDebugColors DebugColors, FBezierSpeedOptions SpeedOptions)
{
    UWorld* World = GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return FVector::ZeroVector;
    }
    
    // 参数验证
    if (Points.Num() < 2)
    {
        return Points.Num() == 1 ? Points[0] : FVector::ZeroVector;
    }

    // 确保Progress在[0,1]范围内
    Progress = FMath::Clamp(Progress, 0.0f, 1.0f);

    FVector ResultPoint;
    TArray<FVector> WorkPoints;
    
    if (SpeedOptions.SpeedMode == EBezierSpeedMode::Constant)
    {
        // --- 优化后的匀速模式 ---

        // 在匀速模式下应用速率曲线
        if (SpeedOptions.SpeedCurve)
        {
            Progress = SpeedOptions.SpeedCurve->GetFloatValue(Progress);
        }

        // 1. 一次性采样曲线，计算总长度和各分段长度
        const int32 Segments = 100; // 采样分段数，可以根据精度需求调整
        TArray<float> SegmentLengths;
        SegmentLengths.Reserve(Segments);
        float TotalLength = 0.0f;
        
        FVector PrevPoint = CalculatePointAtParameter(Points, 0.0f, WorkPoints);

        for (int32 i = 1; i <= Segments; ++i)
        {
            const float t = static_cast<float>(i) / Segments;
            const FVector CurrentPoint = CalculatePointAtParameter(Points, t, WorkPoints);
            const float SegmentLength = FVector::Distance(PrevPoint, CurrentPoint);
            SegmentLengths.Add(SegmentLength);
            TotalLength += SegmentLength;

            // 【新增】在调试模式下，绘制构成曲线的采样线段
            if (bShowDebug)
            {
                DrawDebugLine(World, PrevPoint, CurrentPoint, DebugColors.IntermediateLineColor.ToFColor(true), false, Duration);
            }
            
            PrevPoint = CurrentPoint;
        }

        if (FMath::IsNearlyZero(TotalLength))
        {
            ResultPoint = Points[0];
        }
        else
        {
            // 2. 根据总长度和进度计算目标距离
            const float TargetDistance = TotalLength * Progress;
            float AccumulatedLength = 0.0f;
            float Parameter = 1.0f; // 默认参数为1

            // 3. 从预计算的长度表中查找对应的参数t
            for (int32 i = 0; i < Segments; ++i)
            {
                if (AccumulatedLength + SegmentLengths[i] >= TargetDistance)
                {
                    const float ExcessLength = (AccumulatedLength + SegmentLengths[i]) - TargetDistance;
                    // SegmentLengths[i]为0时可能导致除零，增加检查
                    const float SegmentProgress = (SegmentLengths[i] > KINDA_SMALL_NUMBER) ? 1.0f - (ExcessLength / SegmentLengths[i]) : 1.0f;
                    
                    const float PrevT = static_cast<float>(i) / Segments;
                    const float CurrentT = static_cast<float>(i + 1) / Segments;
                    Parameter = FMath::Lerp(PrevT, CurrentT, SegmentProgress);
                    break;
                }
                AccumulatedLength += SegmentLengths[i];
            }

            // 4. 计算最终点
            // 注意：如果需要调试绘制中间点，这里的WorkPoints需要重新计算
            // 但由于性能优化是首要目标，我们只计算最终结果
            ResultPoint = CalculatePointAtParameter(Points, Parameter, WorkPoints);
        }
    }
    else
    {
        // 默认模式（直接使用参数t，不应用速率曲线）
        ResultPoint = CalculatePointAtParameter(Points, Progress, WorkPoints);
    }

    // 如果开启调试，绘制控制点和连线
    if (bShowDebug)
    {
        // 绘制控制点
        for (const FVector& Point : Points)
        {
            DrawDebugSphere(World, Point, 8.0f, 8, DebugColors.ControlPointColor.ToFColor(true), false, Duration);
        }

        // 绘制控制点之间的连线
        for (int32 i = 0; i < Points.Num() - 1; ++i)
        {
            DrawDebugLine(World, Points[i], Points[i + 1], DebugColors.ControlLineColor.ToFColor(true), false, Duration);
        }

        // 绘制中间计算过程
        const int32 PointCount = Points.Num();
        int32 CurrentIndex = PointCount;
        for (int32 Level = 1; Level < PointCount; ++Level)
        {
            const int32 LevelPoints = PointCount - Level;
            for (int32 i = 0; i < LevelPoints; ++i)
            {
                const FVector& P1 = WorkPoints[CurrentIndex - LevelPoints - 1];
                const FVector& P2 = WorkPoints[CurrentIndex - LevelPoints];
                
                // 绘制中间点
                DrawDebugPoint(World, WorkPoints[CurrentIndex], 4.0f, 
                    DebugColors.IntermediatePointColor.ToFColor(true), false, Duration);
                // 绘制中间连线
                DrawDebugLine(World, P1, P2, 
                    DebugColors.IntermediateLineColor.ToFColor(true), false, Duration);
                
                CurrentIndex++;
            }
        }

        // 绘制结果点（显示时间更长）
        const float ResultPointDuration = Duration * 5.0f;
        DrawDebugPoint(World, ResultPoint, 20.0f, DebugColors.ResultPointColor.ToFColor(true), false, ResultPointDuration);
    }

    return ResultPoint;
}

FVector UXToolsLibrary::CalculatePointAtParameter(const TArray<FVector>& Points, float t, TArray<FVector>& OutWorkPoints)
{
    const int32 PointCount = Points.Num();

    // -- 优化：为最常见的二阶和三阶曲线提供快速计算路径 --
    if (PointCount == 3) // 二阶 (Quadratic)
    {
        OutWorkPoints.Reset(6);
        OutWorkPoints.Append(Points);
        
        const FVector P01 = FMath::Lerp(Points[0], Points[1], t);
        const FVector P12 = FMath::Lerp(Points[1], Points[2], t);
        const FVector Result = FMath::Lerp(P01, P12, t);

        OutWorkPoints.Add(P01);
        OutWorkPoints.Add(P12);
        OutWorkPoints.Add(Result);
        
        return Result;
    }
    if (PointCount == 4) // 三阶 (Cubic)
    {
        OutWorkPoints.Reset(10);
        OutWorkPoints.Append(Points);

        const FVector P01 = FMath::Lerp(Points[0], Points[1], t);
        const FVector P12 = FMath::Lerp(Points[1], Points[2], t);
        const FVector P23 = FMath::Lerp(Points[2], Points[3], t);
        const FVector P012 = FMath::Lerp(P01, P12, t);
        const FVector P123 = FMath::Lerp(P12, P23, t);
        const FVector Result = FMath::Lerp(P012, P123, t);

        OutWorkPoints.Add(P01);
        OutWorkPoints.Add(P12);
        OutWorkPoints.Add(P23);
        OutWorkPoints.Add(P012);
        OutWorkPoints.Add(P123);
        OutWorkPoints.Add(Result);

        return Result;
    }

    // -- 回退到通用算法，处理其他阶数的曲线 --
    const int32 TotalLevels = PointCount - 1;
    const int32 TotalPoints = (PointCount * (PointCount + 1)) / 2;

    OutWorkPoints.Reset(TotalPoints);
    OutWorkPoints.Append(Points);

    for (int32 i = PointCount; i < TotalPoints; ++i)
    {
        OutWorkPoints.Add(FVector::ZeroVector);
    }

    int32 CurrentIndex = PointCount;
    for (int32 Level = 1; Level <= TotalLevels; ++Level)
    {
        const int32 LevelPoints = PointCount - Level;
        for (int32 i = 0; i < LevelPoints; ++i)
        {
            const FVector& P1 = OutWorkPoints[CurrentIndex - LevelPoints - 1];
            const FVector& P2 = OutWorkPoints[CurrentIndex - LevelPoints];
            OutWorkPoints[CurrentIndex++] = FMath::Lerp(P1, P2, t);
        }
    }

    return OutWorkPoints[TotalPoints - 1];
}

TArray<int32> UXToolsLibrary::TestPRDDistribution(float BaseChance)
{
	// 当基础概率小于或等于0时，测试将永远不会成功，导致无限循环。
	// 我们在此处添加一个检查来防止这种情况发生。
	if (BaseChance <= 0.0f)
	{
		UE_LOG(LogXTools, Warning, TEXT("TestPRDDistribution: 基础概率 (BaseChance) 必须大于0。测试无法在概率为0或负数的情况下运行，因为这会导致无限循环。"));
		TArray<int32> Distribution;
		Distribution.Init(0, 13); // 返回一个空的分布数组
		return Distribution;
	}

    // 初始化结果数组，大小为13（0-12）
    TArray<int32> Distribution;
    Distribution.Init(0, 13);

    // 记录当前失败次数
    int32 CurrentFailureCount = 0;
    float ActualChance = 0.f;
    int32 TotalSuccesses = 0;
    int32 TotalTests = 0;
    TArray<int32> FailureTests;
    FailureTests.Init(0, 13);

    // 持续测试直到获得10000次成功
    while (TotalSuccesses < 10000)
    {
        TotalTests++;

        // 调用PRD函数 - 使用高级版本进行测试
        int32 NextFailureCount = 0;
        const bool bSuccess = URandomShuffleArrayLibrary::PseudoRandomBoolAdvanced(
            BaseChance,
            NextFailureCount,  // 输出的新失败次数
            ActualChance,
            "测试",  // StateID
            CurrentFailureCount);  // 输入的当前失败次数

        // 记录当前失败次数下的结果
        if (CurrentFailureCount <= 12)
        {
            FailureTests[CurrentFailureCount]++;
            if (bSuccess)
            {
                Distribution[CurrentFailureCount]++;
                TotalSuccesses++;
            }
        }

        // 更新失败次数 - 高级版本允许用户选择是否使用输出的失败次数
        CurrentFailureCount = NextFailureCount;
    }

    // 输出统计结果到日志
    UE_LOG(LogXTools, Log, TEXT("PRD Distribution Test Results (BaseChance = %.2f):"), BaseChance);
    UE_LOG(LogXTools, Log, TEXT("Total Tests: %d"), TotalTests);
    UE_LOG(LogXTools, Log, TEXT("失败次数 | 成功次数 | 实际成功率 | 理论成功率 | 测试次数"));
    UE_LOG(LogXTools, Log, TEXT("----------------------------------------"));
    
    for (int32 i = 0; i <= 12; ++i)
    {
        float TestActualChance = 0.f;
        int32 TestFailureCount = i;  // 设置为当前失败次数

        // 获取当前失败次数的理论概率 - 使用高级版本进行测试
        // 注意：这里我们只是想获取理论概率，不执行实际的随机判定
        // 所以我们创建一个临时的失败计数器
        int32 TempOutFailureCount = 0;
        URandomShuffleArrayLibrary::PseudoRandomBoolAdvanced(BaseChance, TempOutFailureCount, TestActualChance, "理论测试", TestFailureCount);
        const float ExpectedChance = TestActualChance;

        // 计算实际成功率
        const float SuccessRate = FailureTests[i] > 0 ? static_cast<float>(Distribution[i]) / FailureTests[i] : 0.0f;

        UE_LOG(LogXTools, Log, TEXT("%d次 | %d | %.2f%% | %.2f%% | %d"),
            i,
            Distribution[i],
            SuccessRate * 100.0f,
            ExpectedChance * 100.0f,
            FailureTests[i]);
    }
    UE_LOG(LogXTools, Log, TEXT("Total Successes: %d"), TotalSuccesses);

    return Distribution;
}

// BoxComponent便捷包装函数实现
void UXToolsLibrary::SamplePointsInsideStaticMeshWithBoxOptimized(
    const UObject* WorldContextObject,
    AActor* TargetActor,
    UBoxComponent* BoundingBox,
    EXToolsSamplingMethod Method,
    float GridSpacing,
    float Noise,
    float TraceRadius,
    bool bEnableDebugDraw,
    bool bDrawOnlySuccessfulHits,
    bool bEnableBoundsCulling,
    float DebugDrawDuration,
    TArray<FVector>& OutPoints,
    bool& bSuccess,
    bool bUseComplexCollision)
{
    // --- 重构后的实现 ---
    OutPoints.Empty();
    bSuccess = false;

    // 步骤一：输入验证
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogXTools, Error, TEXT("在模型中生成点阵: WorldContextObject is invalid."));
        return;
    }
    
    if (!TargetActor)
    {
        UE_LOG(LogXTools, Error, TEXT("在模型中生成点阵: TargetActor is nullptr."));
        return;
    }
    
    if (!BoundingBox)
    {
        UE_LOG(LogXTools, Error, TEXT("在模型中生成点阵: BoundingBox is nullptr."));
        return;
    }

    UStaticMeshComponent* TargetMeshComponent = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!TargetMeshComponent)
    {
        UE_LOG(LogXTools, Error, TEXT("在模型中生成点阵: 在目标Actor'%s'上未找到静态网格体组件。"), *TargetActor->GetName());
        return;
    }

    if (GridSpacing <= 0.0f)
    {
        UE_LOG(LogXTools, Error, TEXT("在模型中生成点阵: 点阵间距必须大于零。"));
        return;
    }

    // 步骤二：获取旋转后的盒体信息
    const FTransform BoxTransform = BoundingBox->GetComponentToWorld();
    const FVector Scale3D = BoxTransform.GetScale3D();
    const FVector ScaledBoxExtent = BoundingBox->GetScaledBoxExtent(); // 用于调试绘制
    const FVector UnscaledBoxExtent = BoundingBox->GetUnscaledBoxExtent(); // 用于点阵迭代

    // 【修复】根据世界空间的GridSpacing和组件的缩放，计算局部空间的步长
    // 以确保最终在世界空间中的点间距是恒定的
    const FVector LocalGridStep(
        FMath::Abs(Scale3D.X) > KINDA_SMALL_NUMBER ? GridSpacing / FMath::Abs(Scale3D.X) : 0.0f,
        FMath::Abs(Scale3D.Y) > KINDA_SMALL_NUMBER ? GridSpacing / FMath::Abs(Scale3D.Y) : 0.0f,
        FMath::Abs(Scale3D.Z) > KINDA_SMALL_NUMBER ? GridSpacing / FMath::Abs(Scale3D.Z) : 0.0f
    );
    
    // 如果任何一个轴的步长无效，说明该轴缩放为0或GridSpacing为0，无法迭代，直接返回
    if (!FMath::IsFinite(LocalGridStep.X) || !FMath::IsFinite(LocalGridStep.Y) || !FMath::IsFinite(LocalGridStep.Z))
    {
        UE_LOG(LogXTools, Warning, TEXT("在模型中生成点阵: BoundingBox的某个轴缩放接近于零导致计算出无效的步长，无法生成点阵。"));
        return;
    }

    if (GridSpacing == 0.0f)
    {
        UE_LOG(LogXTools, Warning, TEXT("在模型中生成点阵: GridSpacing 为 0，无法生成点阵。"));
        return;
    }
    
    // 步骤三：【修复】绘制调试盒体
    if (bEnableDebugDraw)
    {
        DrawDebugBox(
            World,
            BoxTransform.GetLocation(),
            ScaledBoxExtent,
            BoxTransform.GetRotation(),
            FColor::Green,
            false,
            DebugDrawDuration,
            0,
            2.0f);
    }

    // 步骤四：【修复】彻底重写Actor忽略逻辑
    TArray<AActor*> ActorsToIgnore;
    
    // 1. 如果世界上下文对象是一个Actor，并且不是我们的目标，就忽略它
    AActor* WorldContextActor = Cast<AActor>(const_cast<UObject*>(WorldContextObject));
    if (WorldContextActor && WorldContextActor != TargetActor)
    {
        ActorsToIgnore.Add(WorldContextActor);
    }

    // 2. 如果盒体的所有者存在，且不是目标，也不是世界上下文，就忽略它
    AActor* BoundingBoxOwner = BoundingBox->GetOwner();
    if (BoundingBoxOwner && BoundingBoxOwner != TargetActor && !ActorsToIgnore.Contains(BoundingBoxOwner))
    {
        ActorsToIgnore.Add(BoundingBoxOwner);
    }
    
    // 步骤五：采样核心逻辑
    // 使用目标组件自己的碰撞通道进行追踪
    const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { UEngineTypes::ConvertToObjectType(TargetMeshComponent->GetCollisionObjectType()) };
    // 根据新的布尔值，决定是否在Trace函数中绘制
    const EDrawDebugTrace::Type DebugDrawType = (bEnableDebugDraw && !bDrawOnlySuccessfulHits) ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;
    int32 TotalPointsChecked = 0;
    int32 CulledPoints = 0;

    switch (Method)
    {
        case EXToolsSamplingMethod::SurfaceProximity:
        {
            // --- 优化：获取目标模型的AABB用于粗筛 ---
            FBox TargetBounds(EForceInit::ForceInit);
            if (bEnableBoundsCulling)
            {
                TargetBounds = TargetMeshComponent->Bounds.GetBox();
                // 稍微扩大包围盒，以考虑追踪半径
                TargetBounds = TargetBounds.ExpandBy(TraceRadius); 
            }

            // --- 新的居中算法 ---
            // 1. 计算每个轴可以容纳的步数
            const FVector BoxSize = UnscaledBoxExtent * 2.0f;
            const int32 NumStepsX = (LocalGridStep.X > 0) ? FMath::FloorToInt(BoxSize.X / LocalGridStep.X) : 0;
            const int32 NumStepsY = (LocalGridStep.Y > 0) ? FMath::FloorToInt(BoxSize.Y / LocalGridStep.Y) : 0;
            const int32 NumStepsZ = (LocalGridStep.Z > 0) ? FMath::FloorToInt(BoxSize.Z / LocalGridStep.Z) : 0;

            // 2. 计算居中所需的起始偏移量
            const float MarginX = BoxSize.X - (NumStepsX * LocalGridStep.X);
            const float MarginY = BoxSize.Y - (NumStepsY * LocalGridStep.Y);
            const float MarginZ = BoxSize.Z - (NumStepsZ * LocalGridStep.Z);

            const FVector GridStart = FVector(
                -UnscaledBoxExtent.X + MarginX / 2.0f,
                -UnscaledBoxExtent.Y + MarginY / 2.0f,
                -UnscaledBoxExtent.Z + MarginZ / 2.0f
            );

            // 3. 使用整数索引进行迭代，以避免浮点数累积误差
            for (int32 i = 0; i <= NumStepsX; ++i)
            {
                const float X = GridStart.X + i * LocalGridStep.X;
                for (int32 j = 0; j <= NumStepsY; ++j)
                {
                    const float Y = GridStart.Y + j * LocalGridStep.Y;
                    for (int32 k = 0; k <= NumStepsZ; ++k)
                    {
                        const float Z = GridStart.Z + k * LocalGridStep.Z;
                        
                        TotalPointsChecked++;
                        FVector LocalPoint(X, Y, Z);

                        // 应用噪点偏移
                        if (Noise > 0.0f)
                        {
                            const FVector RandomOffset(
                                FMath::FRandRange(-Noise, Noise),
                                FMath::FRandRange(-Noise, Noise),
                                FMath::FRandRange(-Noise, Noise)
                            );
                            LocalPoint += RandomOffset;
                        }
                        
                        const FVector WorldPoint = BoxTransform.TransformPosition(LocalPoint);

                        // --- 优化：粗筛阶段 ---
                        if (bEnableBoundsCulling && !TargetBounds.IsInsideOrOn(WorldPoint))
                        {
                            CulledPoints++;
                            continue; // 跳过不在目标包围盒内的点
                        }
                
                        TArray<FHitResult> HitResults;
                        const bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
                            WorldContextObject,
                            WorldPoint, WorldPoint,
                            TraceRadius,
                            ObjectTypes,
                            bUseComplexCollision,
                            ActorsToIgnore,
                            DebugDrawType, // 使用条件性Debug类型
                            HitResults,
                            false, 
                            FLinearColor::Red, FLinearColor::Green,
                            DebugDrawDuration
                        );

                        bool bWasSuccessfulHit = false;
                        if(bHit)
                        {
                            for (const FHitResult& Hit : HitResults)
                            {
                                if (Hit.GetActor() == TargetActor)
                                {
                                    OutPoints.Add(WorldPoint);
                                    bWasSuccessfulHit = true;
                                    break;
                                }
                            }
                        }

                        // 如果开启了"只绘制成功点"模式，且当前点是成功的，则手动绘制一个点
                        if (bEnableDebugDraw && bDrawOnlySuccessfulHits && bWasSuccessfulHit)
                        {
                            DrawDebugSphere(World, WorldPoint, TraceRadius, 12, FColor::Green, false, DebugDrawDuration, 0, 1.0f);
                        }
                    }
                }
            }
            bSuccess = OutPoints.Num() > 0;
            break;
        }

        case EXToolsSamplingMethod::Voxelize:
            UE_LOG(LogXTools, Warning, TEXT("实体填充采样(Voxelize)模式尚未实现。"));
            bSuccess = false;
            break;

        default:
            UE_LOG(LogXTools, Error, TEXT("在模型中生成点阵: 未知的采样模式。"));
            bSuccess = false;
            break;
    }
    
    if (bEnableBoundsCulling)
    {
        UE_LOG(LogXTools, Log, TEXT("共检测 %d 个点, 其中 %d 个点被包围盒剔除, 最终在 %s 内部生成 %d 个点。"), TotalPointsChecked, CulledPoints, *TargetActor->GetName(), OutPoints.Num());
    }
    else
    {
        UE_LOG(LogXTools, Log, TEXT("共检测 %d 个点, 在 %s 内部生成 %d 个点。"), TotalPointsChecked, *TargetActor->GetName(), OutPoints.Num());
    }
}

UFormationManagerComponent* UXToolsLibrary::DemoFormationTransition(
    const UObject* WorldContext,
    const TArray<AActor*>& Units,
    FVector CenterLocation,
    float UnitSpacing,
    float TransitionDuration,
    bool bShowDebug)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("DemoFormationTransition: 无效的世界上下文"));
        return nullptr;
    }

    if (Units.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DemoFormationTransition: 单位数组为空"));
        return nullptr;
    }

    // 创建一个临时Actor来承载阵型管理器组件
    AActor* FormationActor = World->SpawnActor<AActor>();
    if (!FormationActor)
    {
        UE_LOG(LogTemp, Error, TEXT("DemoFormationTransition: 无法创建阵型管理器Actor"));
        return nullptr;
    }

    FormationActor->SetActorLocation(CenterLocation);
    FormationActor->SetActorLabel(TEXT("FormationManager"));

    // 添加阵型管理器组件
    UFormationManagerComponent* FormationManager = NewObject<UFormationManagerComponent>(FormationActor);
    FormationActor->AddInstanceComponent(FormationManager);
    FormationManager->RegisterComponent();

    // 创建方形阵型（起始阵型）
    FFormationData SquareFormation = UFormationLibrary::CreateSquareFormation(
        CenterLocation,
        FRotator::ZeroRotator,
        Units.Num(),
        UnitSpacing
    );

    // 创建圆形阵型（目标阵型）
    float CircleRadius = UnitSpacing * FMath::Max(1.0f, Units.Num() / 6.28f); // 根据单位数量调整半径
    FFormationData CircleFormation = UFormationLibrary::CreateCircleFormation(
        CenterLocation,
        FRotator::ZeroRotator,
        Units.Num(),
        CircleRadius
    );

    // 配置变换参数
    FFormationTransitionConfig Config;
    Config.TransitionMode = EFormationTransitionMode::OptimizedAssignment;
    Config.Duration = TransitionDuration;
    Config.bUseEasing = true;
    Config.EasingStrength = 2.0f;
    Config.bShowDebug = bShowDebug;
    Config.DebugDuration = TransitionDuration + 2.0f;

    // 开始阵型变换
    bool bSuccess = FormationManager->StartFormationTransition(Units, SquareFormation, CircleFormation, Config);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("DemoFormationTransition: 成功开始阵型变换演示，单位数量: %d"), Units.Num());

        // 如果启用调试，绘制阵型信息
        if (bShowDebug)
        {
            UFormationLibrary::DrawFormationDebug(WorldContext, SquareFormation, TransitionDuration + 2.0f, FLinearColor::Green, 2.0f);
            UFormationLibrary::DrawFormationDebug(WorldContext, CircleFormation, TransitionDuration + 2.0f, FLinearColor::Red, 2.0f);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("DemoFormationTransition: 阵型变换启动失败"));
        FormationActor->Destroy();
        return nullptr;
    }

    return FormationManager;
}
