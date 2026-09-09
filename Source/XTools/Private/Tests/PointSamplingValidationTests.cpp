/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "XToolsLibrary.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

#include <limits>

namespace
{
    class FScopedPointSamplingTestWorld
    {
    public:
        explicit FScopedPointSamplingTestWorld(FName WorldName)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, WorldName);
            if (World)
            {
                FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
                WorldContext.SetCurrentWorld(World);

                FURL URL;
                World->InitializeActorsForPlay(URL);
            }
        }

        ~FScopedPointSamplingTestWorld()
        {
            if (World)
            {
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
                World = nullptr;
            }
        }

        UWorld* Get() const { return World; }

    private:
        UWorld* World = nullptr;
    };

    UBoxComponent* AddBoundingBox(AActor* Actor)
    {
        UBoxComponent* BoundingBox = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(BoundingBox);
        BoundingBox->SetBoxExtent(FVector(10.0));
        BoundingBox->RegisterComponent();
        return BoundingBox;
    }

    void RunSampling(
        UWorld* World,
        AActor* TargetActor,
        UBoxComponent* BoundingBox,
        const FPointSamplingConfig& Config,
        bool& bSuccess)
    {
        TArray<FVector> Points;
        bSuccess = true;
        UXToolsLibrary::SamplePointsInsideMesh(
            World, TargetActor, BoundingBox, Config, Points, bSuccess);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPointSamplingRejectsInvalidRuntimeInputs,
    "XTools.PointSampling.Validation.RejectsInvalidRuntimeInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPointSamplingRejectsInvalidRuntimeInputs::RunTest(const FString& Parameters)
{
    FScopedPointSamplingTestWorld TestWorld(TEXT("XToolsPointSamplingValidationTest"));
    UWorld* World = TestWorld.Get();
    if (!TestNotNull(TEXT("应创建点采样测试世界"), World))
    {
        return false;
    }

    AActor* TargetActor = World->SpawnActor<AActor>();
    UBoxComponent* BoundingBox = TargetActor ? AddBoundingBox(TargetActor) : nullptr;
    if (!TestNotNull(TEXT("应创建目标Actor"), TargetActor)
        || !TestNotNull(TEXT("应创建边界框组件"), BoundingBox))
    {
        return false;
    }

    FPointSamplingConfig Config;
    bool bSuccess = true;

    AddExpectedError(TEXT("采样失败: 网格间距必须是大于0的有限值"),
        EAutomationExpectedErrorFlags::Contains, 2);
    Config.GridSpacing = std::numeric_limits<float>::quiet_NaN();
    RunSampling(World, TargetActor, BoundingBox, Config, bSuccess);
    TestFalse(TEXT("非有限网格间距应在计算网格前失败"), bSuccess);

    AddExpectedError(TEXT("采样失败: 噪声偏移必须是大于等于0的有限值"),
        EAutomationExpectedErrorFlags::Contains, 2);
    Config.GridSpacing = 10.0f;
    Config.Noise = -1.0f;
    RunSampling(World, TargetActor, BoundingBox, Config, bSuccess);
    TestFalse(TEXT("负噪声偏移应在碰撞查询前失败"), bSuccess);

    AddExpectedError(TEXT("采样失败: 检测半径必须是大于0的有限值"),
        EAutomationExpectedErrorFlags::Contains, 2);
    Config.Noise = 0.0f;
    Config.TraceRadius = 0.0f;
    RunSampling(World, TargetActor, BoundingBox, Config, bSuccess);
    TestFalse(TEXT("非正检测半径应在碰撞查询前失败"), bSuccess);

    AddExpectedError(TEXT("采样失败: 调试持续时间必须是大于0的有限值"),
        EAutomationExpectedErrorFlags::Contains, 2);
    Config.TraceRadius = 5.0f;
    Config.bEnableDebugDraw = true;
    Config.DebugDrawDuration = std::numeric_limits<float>::infinity();
    RunSampling(World, TargetActor, BoundingBox, Config, bSuccess);
    TestFalse(TEXT("非有限调试时长应在调试绘制前失败"), bSuccess);

    AActor* DestroyedActor = World->SpawnActor<AActor>();
    UBoxComponent* DestroyedActorBounds = DestroyedActor ? AddBoundingBox(DestroyedActor) : nullptr;
    if (TestNotNull(TEXT("应创建待销毁目标Actor"), DestroyedActor)
        && TestNotNull(TEXT("应创建待销毁Actor的边界框"), DestroyedActorBounds))
    {
        DestroyedActor->Destroy();
        Config.bEnableDebugDraw = false;

        AddExpectedError(TEXT("采样失败: 目标Actor无效"),
            EAutomationExpectedErrorFlags::Contains, 2);
        RunSampling(World, DestroyedActor, DestroyedActorBounds, Config, bSuccess);
        TestFalse(TEXT("已销毁Actor应在组件访问前失败"), bSuccess);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointSamplingKismetEquivalence,
    "XTools.PointSampling.Surface.KismetEquivalence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPointSamplingKismetEquivalence::RunTest(const FString& Parameters)
{
    FScopedPointSamplingTestWorld TestWorld(TEXT("XToolsSurfaceEquivalence"));
    UWorld* World = TestWorld.Get();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("World"), World) || !TestNotNull(TEXT("Cube collision fixture"), Cube))
    {
        return false;
    }
    AActor* Target = World->SpawnActor<AActor>();
    UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Target);
    Target->SetRootComponent(Mesh);
    Mesh->SetStaticMesh(Cube);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    Mesh->RegisterComponent();
    AActor* BoundsOwner = World->SpawnActor<AActor>();
    UBoxComponent* Bounds = AddBoundingBox(BoundsOwner);
    Bounds->SetBoxExtent(FVector(60));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AActor* Obstacle = World->SpawnActor<AActor>();
    UBoxComponent* ObstacleBox = AddBoundingBox(Obstacle);
    ObstacleBox->SetBoxExtent(FVector(55));
    ObstacleBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ObstacleBox->SetCollisionResponseToAllChannels(ECR_Block);
    const TArray<AActor*> IgnoredActors{BoundsOwner, Obstacle};

    for (int32 Variant = 0; Variant < 8; ++Variant)
    {
        FPointSamplingConfig Config;
        Config.GridSpacing = 6;
        Config.bUseComplexCollision = (Variant & 1) != 0;
        Config.bEnableBoundsCulling = (Variant & 2) != 0;
        Config.bEnableDebugDraw = Variant == 7;
        Config.bDrawOnlySuccessfulHits = false;
        const ECollisionChannel Channel = (Variant & 4) ? ECC_WorldDynamic : ECC_WorldStatic;
        Mesh->SetCollisionObjectType(Channel);
        Bounds->SetWorldRotation((Variant & 4) ? FRotator(0, 30, 0) : FRotator::ZeroRotator);
        TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes{UEngineTypes::ConvertToObjectType(Channel)};
        TArray<FVector> Expected;
        const FBox CullBounds = Mesh->Bounds.GetBox().ExpandBy(Config.TraceRadius);
        const double ReferenceStart = FPlatformTime::Seconds();
        for (int32 X = 0; X <= 20; ++X)
        for (int32 Y = 0; Y <= 20; ++Y)
        for (int32 Z = 0; Z <= 20; ++Z)
        {
            const FVector Point = Bounds->GetComponentTransform().TransformPosition(
                FVector(-60 + X * 6, -60 + Y * 6, -60 + Z * 6));
            if (Config.bEnableBoundsCulling && !CullBounds.IsInsideOrOn(Point))
            {
                continue;
            }
            FHitResult Hit;
            if (UKismetSystemLibrary::SphereTraceSingleForObjects(World, Point, Point,
                Config.TraceRadius, ObjectTypes, Config.bUseComplexCollision, IgnoredActors,
                EDrawDebugTrace::None, Hit, true)
                && Hit.GetActor() == Target && Hit.GetComponent() == Mesh)
            {
                Expected.Add(Point);
            }
        }
        const double ReferenceMs = (FPlatformTime::Seconds() - ReferenceStart) * 1000;
        TArray<FVector> Actual;
        bool bSuccess = false;
        const double ActualStart = FPlatformTime::Seconds();
        UXToolsLibrary::SamplePointsInsideMesh(World, Target, Bounds, Config, Actual, bSuccess);
        const double ActualMs = (FPlatformTime::Seconds() - ActualStart) * 1000;
        TestTrue(TEXT("Sampling succeeds"), bSuccess);
        TestTrue(TEXT("Fixture produces collision hits"), Expected.Num() > 0);
        TestTrue(FString::Printf(TEXT("Kismet point order and hits variant %d"), Variant), Actual == Expected);
        AddInfo(FString::Printf(TEXT("Surface variant=%d points=%d Kismet=%.3fms sampling=%.3fms debug=%d"),
            Variant, Actual.Num(), ReferenceMs, ActualMs, Config.bEnableDebugDraw));
    }
    return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
