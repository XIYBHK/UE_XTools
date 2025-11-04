#include "Features/SupportSystemLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"

TArray<FTransform> USupportSystemLibrary::GetLocalFulcrumTransform(const UPrimitiveComponent* TargetComponent, const FVector& PlaneBase)
{
    TArray<FTransform> FulcrumTransformArray;
    if (!TargetComponent) return FulcrumTransformArray;

    FVector ComponentExtent = TargetComponent->Bounds.BoxExtent;
    float BottomZ = PlaneBase.Z; // Use the Z coordinate of PlaneBase

    // Calculate the positions of the four fulcrums in local space
    FulcrumTransformArray.Add(FTransform(FVector(ComponentExtent.X, ComponentExtent.Y, BottomZ)));
    FulcrumTransformArray.Add(FTransform(FVector(-ComponentExtent.X, ComponentExtent.Y, BottomZ)));
    FulcrumTransformArray.Add(FTransform(FVector(ComponentExtent.X, -ComponentExtent.Y, BottomZ)));
    FulcrumTransformArray.Add(FTransform(FVector(-ComponentExtent.X, -ComponentExtent.Y, BottomZ)));

    return FulcrumTransformArray;
}

TArray<FTransform> USupportSystemLibrary::GetWorldFulcrumTransform(const FTransform& ObjectTransform, const TArray<FTransform>& FulcrumTransformArray)
{
    TArray<FTransform> WorldTransforms;

    for (const FTransform& Transform : FulcrumTransformArray)
    {
        WorldTransforms.Add(Transform * ObjectTransform);
    }

    return WorldTransforms;
}

void USupportSystemLibrary::StabilizeHeight(
    UObject* WorldContextObject,
    UPrimitiveComponent* TargetComponent,
    const TArray<FTransform>& WorldFulcrumTransform, 
    float StableHeight,
    float GripHeight,
    float GripStrength,
    float DeltaTime,
    float ErrorRange,
    TArray<float>& LastError,
    TArray<float>& IntegralError,
    float Kp,
    float Ki,
    float Kd,
    ETraceTypeQuery ChannelType,
    float& AveragePressureFactor, 
    FVector& AverageImpactNormal, 
    bool DrawDebug)
{
    // 检查输入参数的有效性
    if (!TargetComponent || !WorldContextObject || WorldFulcrumTransform.Num() == 0) return;

    // 计算每个支点的质量
    const float Mass = TargetComponent->GetMass() / WorldFulcrumTransform.Num();

    // 获取世界对象和碰撞通道
    UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
    ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ChannelType);

    // 确保LastError和IntegralError的大小与支点数量一致
    LastError.SetNum(WorldFulcrumTransform.Num());
    IntegralError.SetNum(WorldFulcrumTransform.Num());

    // 标记所有LineTrace是否命中
    bool bAllHit = true;
    TArray<FHitResult> HitResults;
    HitResults.SetNum(WorldFulcrumTransform.Num());

    // 存储所有PressureFactor的总和和有效计数
    float TotalPressureFactor = 0.0f;
    int32 ValidPressureCount = 0;

    // 存储所有ImpactNormal的总和
    FVector TotalImpactNormal(0.0f, 0.0f, 0.0f);

    // 遍历所有支点进行LineTrace
    for (int32 i = 0; i < WorldFulcrumTransform.Num(); ++i)
    {
        const FTransform& FulcrumTransform = WorldFulcrumTransform[i];
        FVector Start = FulcrumTransform.GetLocation();
        FVector End = Start - FulcrumTransform.GetUnitAxis(EAxis::Z) * (StableHeight + GripHeight);
        FCollisionQueryParams CollisionParams;
        bool bHit = World->LineTraceSingleByChannel(HitResults[i], Start, End, CollisionChannel, CollisionParams);
        bAllHit &= bHit;

        // 绘制Trace起始点到终点的向量
        if (DrawDebug)
        {
            DrawDebugLine(World, Start, End, FColor::Yellow, false, 0.0f, 0, 1.0f);
        }

        // 计算PressureFactor
        if (bHit)
        {
            float Error = FVector::DotProduct(HitResults[i].ImpactPoint - (FulcrumTransform.GetLocation() - FulcrumTransform.GetUnitAxis(EAxis::Z) * StableHeight), FVector(0.0f, 0.0f, 1.0f));
            Error = FMath::Clamp(Error, -ErrorRange, ErrorRange);
            float PressureFactor = FMath::Clamp(Error / ErrorRange, -1.0f, 1.0f);
            TotalPressureFactor += PressureFactor;
            ValidPressureCount++;

            // 累加ImpactNormal
            TotalImpactNormal += HitResults[i].ImpactNormal;
        }
        else
        {
            TotalPressureFactor += -1.0f;

            // 如果未命中，使用FulcrumTransform的上方向
            TotalImpactNormal += FulcrumTransform.GetUnitAxis(EAxis::Z);
        }
    }

    // 计算AveragePressureFactor
    AveragePressureFactor = (ValidPressureCount > 0) ? FMath::Clamp(TotalPressureFactor / ValidPressureCount, -1.0f, 1.0f) : -1.0f;

    // 计算AverageImpactNormal
    AverageImpactNormal = TotalImpactNormal.GetSafeNormal();

    // 遍历所有支点应用力
    for (int32 i = 0; i < WorldFulcrumTransform.Num(); ++i)
    {
        const FTransform& FulcrumTransform = WorldFulcrumTransform[i];
        const FVector StableLocation = FulcrumTransform.GetLocation() - FulcrumTransform.GetUnitAxis(EAxis::Z) * StableHeight;
        const FHitResult& HitResult = HitResults[i];

        if (HitResult.bBlockingHit)
        {
            // 绘制ImpactPoint的位置
            if (DrawDebug)
            {
                DrawDebugPoint(World, HitResult.ImpactPoint, 10.0f, FColor::Red, false, 0.0f);
            }

            // 计算误差
            float Error = FVector::DotProduct(HitResult.ImpactPoint - StableLocation, FVector(0.0f, 0.0f, 1.0f));
            Error = FMath::Clamp(Error, -ErrorRange, ErrorRange);

            // 计算NormalFactor
            float Angle = FMath::Acos(FVector::DotProduct(FulcrumTransform.GetUnitAxis(EAxis::Z), AverageImpactNormal));
            float NormalFactor = FMath::Clamp(1.0f - (Angle / (PI / 4.0f)), 0.0f, 1.0f);

            // 绘制StableLocation到ImpactPoint的向量
            if (DrawDebug)
            {
                DrawDebugLine(World, StableLocation, HitResult.ImpactPoint, FColor::Blue, false, 0.0f, 0, 1.0f);
            }

            // 使用指数衰减函数计算Alpha，DeltaTime越大，Alpha越小
            const float Alpha = FMath::Exp(-DeltaTime);

            // 低通滤波器平滑误差信号
            const float SmoothedError = Alpha * Error + (1.0f - Alpha) * LastError[i];

            // 积分项计算
            IntegralError[i] += SmoothedError * DeltaTime;

            // 微分项计算
            const float Derivative = (SmoothedError - LastError[i]) / DeltaTime;

            // PID输出
            float Output = Kp * SmoothedError + Ki * IntegralError[i] + Kd * Derivative;

            // 根据Error和AveragePressureFactor的值决定是否施加作用力
            if (Error > 0.0f)
            {
                // 提供向上的作用力
                const FVector Force = FulcrumTransform.GetUnitAxis(EAxis::Z) * Output * Mass * NormalFactor;
                TargetComponent->AddForceAtLocation(Force, FulcrumTransform.GetLocation());
            }
            else if (Error < 0.0f)
            {
                // 计算GripPressure
                float GripPressure = FMath::Clamp(AveragePressureFactor + 1.0f, 0.0f, 1.0f);
                // 提供向下的作用力，受GripStrength和GripPressure系数控制
                Output *= GripStrength * GripPressure;
                const FVector Force = FulcrumTransform.GetUnitAxis(EAxis::Z) * Output * Mass * NormalFactor;
                TargetComponent->AddForceAtLocation(Force, FulcrumTransform.GetLocation());
            }

            // 更新上一次的误差
            LastError[i] = SmoothedError;
        }
        else
        {
            // 清空缓存数据
            IntegralError[i] = 0.0f;
            LastError[i] = 0.0f;
        }

        // 绘制StableLocation的位置
        if (DrawDebug)
        {
            DrawDebugPoint(World, StableLocation, 10.0f, FColor::Green, false, 0.0f);
        }
    }
}