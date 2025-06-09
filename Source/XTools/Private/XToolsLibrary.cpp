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
AActor* UXToolsLibrary::FindParentComponentByClass(UActorComponent* Component, TSubclassOf<AActor> ActorClass, const FString& ActorTag)
{
    if (!Component)
    {
        UE_LOG(LogTemp, Warning, TEXT("提供的组件无效"));
        return nullptr;
    }

    // 确保ActorClass是有效的Actor类
    if (ActorClass && !ActorClass->IsChildOf(AActor::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("提供的ActorClass无效"));
        return nullptr;
    }

    USceneComponent* SceneComp = Cast<USceneComponent>(Component);
    if (!SceneComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("组件不是SceneComponent类型"));
        return nullptr;
    }

    // 设置最大深度，默认100层
    const int32 MaxIterations = XTOOLS_MAX_PARENT_DEPTH;
    int32 IterationCount = 0;
    FName TagName = FName(*ActorTag);
    AActor* HighestParent = nullptr;
    USceneComponent* ParentComp = SceneComp->GetAttachParent();
    
    // 调试信息
        UE_LOG(LogTemp, Log, TEXT("开始从组件查找父级: %s"), *Component->GetName());
    
    // 遍历所有父组件
    while (ParentComp && IterationCount < MaxIterations)
    {
        IterationCount++;
        
        AActor* ParentActor = ParentComp->GetOwner();
        
        // 如果两者都为空，直接记录当前父级并继续向上查找
        if (!ActorClass && ActorTag.IsEmpty())
        {
            HighestParent = ParentActor;
            ParentComp = ParentComp->GetAttachParent();
            continue;
        }
        
        // 如果指定了ActorClass，检查是否匹配
        if (ActorClass && (!ParentActor || !ParentActor->IsA(ActorClass)))
        {
            ParentComp = ParentComp->GetAttachParent();
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("正在检查父级Actor: %s"), *ParentActor->GetName());

        // 如果指定了ActorClass，检查是否匹配
        if (ActorClass && ParentActor->IsA(ActorClass))
        {
            // 记录当前匹配的父级
            HighestParent = ParentActor;
            UE_LOG(LogTemp, Log, TEXT("记录匹配类的父级: %s"), *ParentActor->GetName());

            // 如果同时指定了ActorTag且匹配，立即返回
            if (!ActorTag.IsEmpty() && ParentActor->Tags.Contains(TagName))
            {
                UE_LOG(LogTemp, Log, TEXT("找到匹配类和标签的父级: %s"), *ActorTag);
                return ParentActor;
            }
            
            // 即使Tag不匹配，也继续查找更高层级的父级
            // 最终会返回最高级匹配的父级
        }
        // 如果只指定了ActorTag，检查是否匹配
        else if (!ActorTag.IsEmpty() && ParentActor->Tags.Contains(TagName))
        {
            UE_LOG(LogTemp, Log, TEXT("找到匹配标签的父级: %s"), *ActorTag);
            return ParentActor; // 找到匹配Tag的父级，立即返回
        }
        
        ParentComp = ParentComp->GetAttachParent();
    }

    if (IterationCount >= MaxIterations)
    {
        UE_LOG(LogTemp, Error, TEXT("查找父级Actor时达到最大迭代次数"));
    }
    
    // 返回最高级匹配的父级
    return HighestParent;
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
        // 匀速模式
        // 在匀速模式下应用速率曲线
        if (SpeedOptions.SpeedCurve)
        {
            Progress = SpeedOptions.SpeedCurve->GetFloatValue(Progress);
        }
        
        const float TotalLength = CalculateCurveLength(Points);
        const float TargetDistance = TotalLength * Progress;
        const float Parameter = GetParameterByDistance(Points, TargetDistance, TotalLength);
        ResultPoint = CalculatePointAtParameter(Points, Parameter, WorkPoints);
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

float UXToolsLibrary::CalculateCurveLength(const TArray<FVector>& Points, int32 Segments)
{
    float Length = 0.0f;
    TArray<FVector> WorkPoints;
    FVector PrevPoint = CalculatePointAtParameter(Points, 0.0f, WorkPoints);

    for (int32 i = 1; i <= Segments; ++i)
    {
        const float t = static_cast<float>(i) / Segments;
        const FVector CurrentPoint = CalculatePointAtParameter(Points, t, WorkPoints);
        Length += FVector::Distance(PrevPoint, CurrentPoint);
        PrevPoint = CurrentPoint;
    }

    return Length;
}

float UXToolsLibrary::GetParameterByDistance(const TArray<FVector>& Points, float TargetDistance, float TotalLength, int32 Segments)
{
    float AccumulatedLength = 0.0f;
    TArray<FVector> WorkPoints;
    FVector PrevPoint = CalculatePointAtParameter(Points, 0.0f, WorkPoints);

    for (int32 i = 1; i <= Segments; ++i)
    {
        const float t = static_cast<float>(i) / Segments;
        const FVector CurrentPoint = CalculatePointAtParameter(Points, t, WorkPoints);
        const float SegmentLength = FVector::Distance(PrevPoint, CurrentPoint);
        
        if (AccumulatedLength + SegmentLength >= TargetDistance)
        {
            const float ExcessLength = AccumulatedLength + SegmentLength - TargetDistance;
            const float SegmentProgress = 1.0f - (ExcessLength / SegmentLength);
            const float PrevT = static_cast<float>(i - 1) / Segments;
            return FMath::Lerp(PrevT, t, SegmentProgress);
        }

        AccumulatedLength += SegmentLength;
        PrevPoint = CurrentPoint;
    }

    return 1.0f;
}

TArray<int32> UXToolsLibrary::TestPRDDistribution(float BaseChance)
{
    // 初始化结果数组，大小为13（0-12）
    TArray<int32> Distribution;
    Distribution.Init(0, 13);

    // 记录当前失败次数
    int32 CurrentFailureCount = 1;
    float ActualChance = 0.f;
    int32 TotalSuccesses = 0;
    int32 TotalTests = 0;
    TArray<int32> FailureTests;
    FailureTests.Init(0, 13);

    // 持续测试直到获得10000次成功
    while (TotalSuccesses < 10000)
    {
        int32 NextFailureCount = 0;
        TotalTests++;
        
        // 调用PRD函数
        const bool bSuccess = URandomShuffleArrayLibrary::PseudoRandomBool(
            BaseChance, 
            NextFailureCount, 
            ActualChance, 
            CurrentFailureCount);

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

        // 更新失败次数
        CurrentFailureCount = NextFailureCount;
    }

    // 输出统计结果到日志
    UE_LOG(LogTemp, Log, TEXT("PRD Distribution Test Results (BaseChance = %.2f):"), BaseChance);
    UE_LOG(LogTemp, Log, TEXT("Total Tests: %d"), TotalTests);
    UE_LOG(LogTemp, Log, TEXT("失败次数 | 成功次数 | 实际成功率 | 理论成功率 | 测试次数"));
    UE_LOG(LogTemp, Log, TEXT("----------------------------------------"));
    
    for (int32 i = 0; i <= 12; ++i)
    {
        float TestChance = 0.f;
        float TestActualChance = 0.f;
        int32 TestFailureCount = 0;
        
        // 获取当前失败次数的理论概率
        URandomShuffleArrayLibrary::PseudoRandomBool(BaseChance, TestFailureCount, TestActualChance, i);
        const float ExpectedChance = TestActualChance;
        
        // 计算实际成功率
        const float SuccessRate = FailureTests[i] > 0 ? static_cast<float>(Distribution[i]) / FailureTests[i] : 0.0f;
        
        UE_LOG(LogTemp, Log, TEXT("%d次 | %d | %.2f%% | %.2f%% | %d"), 
            i, 
            Distribution[i],
            SuccessRate * 100.0f,
            ExpectedChance * 100.0f,
            FailureTests[i]);
    }
    UE_LOG(LogTemp, Log, TEXT("Total Successes: %d"), TotalSuccesses);

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
    bool& bSuccess)
{
    // --- 重构后的实现 ---
    OutPoints.Empty();
    bSuccess = false;

    // 步骤一：输入验证
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("在模型中生成点阵: WorldContextObject is invalid."));
        return;
    }
    
    if (!TargetActor)
    {
        UE_LOG(LogTemp, Error, TEXT("在模型中生成点阵: TargetActor is nullptr."));
        return;
    }
    
    if (!BoundingBox)
    {
        UE_LOG(LogTemp, Error, TEXT("在模型中生成点阵: BoundingBox is nullptr."));
        return;
    }

    UStaticMeshComponent* TargetMeshComponent = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!TargetMeshComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("在模型中生成点阵: 在目标Actor'%s'上未找到静态网格体组件。"), *TargetActor->GetName());
        return;
    }

    if (GridSpacing <= 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("在模型中生成点阵: 点阵间距必须大于零。"));
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
        UE_LOG(LogTemp, Warning, TEXT("在模型中生成点阵: BoundingBox的某个轴缩放接近于零导致计算出无效的步长，无法生成点阵。"));
        return;
    }

    if (GridSpacing == 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("在模型中生成点阵: GridSpacing 为 0，无法生成点阵。"));
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
    const AActor* WorldContextActor = Cast<const AActor>(WorldContextObject);
    if (WorldContextActor && WorldContextActor != TargetActor)
    {
        ActorsToIgnore.Add(const_cast<AActor*>(WorldContextActor));
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
                            true, // 必须为true以使用复杂（逐多边形）碰撞检测
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
            UE_LOG(LogTemp, Warning, TEXT("实体填充采样(Voxelize)模式尚未实现。"));
            bSuccess = false;
            break;

        default:
            UE_LOG(LogTemp, Error, TEXT("在模型中生成点阵: 未知的采样模式。"));
            bSuccess = false;
            break;
    }
    
    if (bEnableBoundsCulling)
    {
        UE_LOG(LogTemp, Log, TEXT("共检测 %d 个点, 其中 %d 个点被包围盒剔除, 最终在 %s 内部生成 %d 个点。"), TotalPointsChecked, CulledPoints, *TargetActor->GetName(), OutPoints.Num());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("共检测 %d 个点, 在 %s 内部生成 %d 个点。"), TotalPointsChecked, *TargetActor->GetName(), OutPoints.Num());
    }
}
