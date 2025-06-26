#include "FormationTestActor.h"
#include "FormationSystem.h"
#include "FormationLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"

AFormationTestActor::AFormationTestActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建根组件
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    // 创建阵型管理器组件
    FormationManager = CreateDefaultSubobject<UFormationManagerComponent>(TEXT("FormationManager"));
}

void AFormationTestActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 延迟初始化，确保所有组件都已准备好
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
    {
        if (!bInitialized)
        {
            InitializePredefinedFormations();
            bInitialized = true;
            
            // 如果没有测试单位，自动创建一些
            if (TestUnits.Num() == 0)
            {
                CreateTestUnits(16);
            }
        }
    });
}

void AFormationTestActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bAutoLoop && bInitialized)
    {
        UpdateAutoLoop(DeltaTime);
    }
}

void AFormationTestActor::InitializePredefinedFormations()
{
    PredefinedFormations.Empty();
    FVector CenterLocation = GetActorLocation();

    // 1. 方形阵型
    FFormationData SquareFormation = UFormationLibrary::CreateSquareFormation(
        CenterLocation, FRotator::ZeroRotator, 16, UnitSpacing
    );
    PredefinedFormations.Add(SquareFormation);

    // 2. 圆形阵型
    float CircleRadius = UnitSpacing * 2.5f;
    FFormationData CircleFormation = UFormationLibrary::CreateCircleFormation(
        CenterLocation, FRotator::ZeroRotator, 16, CircleRadius
    );
    PredefinedFormations.Add(CircleFormation);

    // 3. 线形阵型（水平）
    FFormationData LineFormation = UFormationLibrary::CreateLineFormation(
        CenterLocation, FRotator::ZeroRotator, 16, UnitSpacing, false
    );
    PredefinedFormations.Add(LineFormation);

    // 4. 线形阵型（垂直）
    FFormationData VerticalLineFormation = UFormationLibrary::CreateLineFormation(
        CenterLocation, FRotator::ZeroRotator, 16, UnitSpacing, true
    );
    PredefinedFormations.Add(VerticalLineFormation);

    // 5. 三角形阵型
    FFormationData TriangleFormation = UFormationLibrary::CreateTriangleFormation(
        CenterLocation, FRotator::ZeroRotator, 16, UnitSpacing, false
    );
    PredefinedFormations.Add(TriangleFormation);

    // 6. 倒三角形阵型
    FFormationData InvertedTriangleFormation = UFormationLibrary::CreateTriangleFormation(
        CenterLocation, FRotator::ZeroRotator, 16, UnitSpacing, true
    );
    PredefinedFormations.Add(InvertedTriangleFormation);

    UE_LOG(LogTemp, Log, TEXT("FormationTestActor: 初始化了 %d 个预定义阵型"), PredefinedFormations.Num());
}

void AFormationTestActor::SwitchToNextFormation()
{
    if (PredefinedFormations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("FormationTestActor: 没有可用的预定义阵型"));
        return;
    }

    int32 NextIndex = (CurrentFormationIndex + 1) % PredefinedFormations.Num();
    SwitchToFormation(NextIndex);
}

void AFormationTestActor::SwitchToFormation(int32 FormationIndex)
{
    if (!PredefinedFormations.IsValidIndex(FormationIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("FormationTestActor: 无效的阵型索引 %d"), FormationIndex);
        return;
    }

    if (TestUnits.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("FormationTestActor: 没有测试单位"));
        return;
    }

    if (FormationManager->IsTransitioning())
    {
        UE_LOG(LogTemp, Log, TEXT("FormationTestActor: 正在进行阵型变换，跳过此次请求"));
        return;
    }

    // 获取当前阵型
    FVector CurrentCenter;
    FFormationData CurrentFormation = UFormationLibrary::GetCurrentFormationFromActors(TestUnits, CurrentCenter);

    // 获取目标阵型
    FFormationData TargetFormation = PredefinedFormations[FormationIndex];
    
    // 调整目标阵型的单位数量以匹配当前单位数量
    if (TargetFormation.Positions.Num() != TestUnits.Num())
    {
        TargetFormation = UFormationLibrary::ResizeFormation(TargetFormation, TestUnits.Num());
    }

    // 配置变换参数
    FFormationTransitionConfig Config;
    Config.TransitionMode = TransitionMode;
    Config.Duration = TransitionDuration;
    Config.bUseEasing = true;
    Config.EasingStrength = 2.0f;
    Config.bShowDebug = bShowDebug;
    Config.DebugDuration = TransitionDuration + 2.0f;

    // 开始变换
    bool bSuccess = FormationManager->StartFormationTransition(TestUnits, CurrentFormation, TargetFormation, Config);
    
    if (bSuccess)
    {
        CurrentFormationIndex = FormationIndex;
        LastTransitionTime = GetWorld()->GetTimeSeconds();
        
        UE_LOG(LogTemp, Log, TEXT("FormationTestActor: 开始变换到阵型 '%s' (索引: %d)"), 
               *GetFormationName(FormationIndex), FormationIndex);

        // 绘制调试信息
        if (bShowDebug)
        {
            UFormationLibrary::DrawFormationDebug(this, CurrentFormation, Config.DebugDuration, FLinearColor::Green, 2.0f);
            UFormationLibrary::DrawFormationDebug(this, TargetFormation, Config.DebugDuration, FLinearColor::Red, 2.0f);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FormationTestActor: 阵型变换启动失败"));
    }
}

void AFormationTestActor::CreateTestUnits(int32 UnitCount, TSubclassOf<AActor> UnitClass)
{
    // 清理现有单位
    ClearTestUnits();

    if (UnitCount <= 0)
    {
        return;
    }

    TestUnits.Reserve(UnitCount);
    FVector CenterLocation = GetActorLocation();

    // 在一个小范围内随机生成单位
    for (int32 i = 0; i < UnitCount; i++)
    {
        FVector SpawnLocation = CenterLocation + FVector(
            FMath::RandRange(-200.0f, 200.0f),
            FMath::RandRange(-200.0f, 200.0f),
            0.0f
        );

        AActor* NewUnit = nullptr;
        
        if (UnitClass)
        {
            // 使用指定的类创建单位
            NewUnit = GetWorld()->SpawnActor<AActor>(UnitClass, SpawnLocation, FRotator::ZeroRotator);
        }
        else
        {
            // 创建默认单位
            NewUnit = CreateDefaultUnit(SpawnLocation);
        }

        if (NewUnit)
        {
            NewUnit->SetActorLabel(FString::Printf(TEXT("TestUnit_%d"), i));
            TestUnits.Add(NewUnit);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("FormationTestActor: 创建了 %d 个测试单位"), TestUnits.Num());
}

void AFormationTestActor::ClearTestUnits()
{
    for (AActor* Unit : TestUnits)
    {
        if (IsValid(Unit))
        {
            Unit->Destroy();
        }
    }
    TestUnits.Empty();
}

void AFormationTestActor::StartDemo()
{
    if (!bInitialized)
    {
        InitializePredefinedFormations();
        bInitialized = true;
    }

    if (TestUnits.Num() == 0)
    {
        CreateTestUnits(16);
    }

    bAutoLoop = true;
    LastTransitionTime = GetWorld()->GetTimeSeconds();
    
    UE_LOG(LogTemp, Log, TEXT("FormationTestActor: 开始演示模式"));
}

void AFormationTestActor::StopDemo()
{
    bAutoLoop = false;
    
    if (FormationManager->IsTransitioning())
    {
        FormationManager->StopFormationTransition(false);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FormationTestActor: 停止演示模式"));
}

FString AFormationTestActor::GetFormationName(int32 FormationIndex) const
{
    if (!PredefinedFormations.IsValidIndex(FormationIndex))
    {
        return TEXT("无效阵型");
    }

    switch (FormationIndex)
    {
        case 0: return TEXT("方形阵型");
        case 1: return TEXT("圆形阵型");
        case 2: return TEXT("水平线形阵型");
        case 3: return TEXT("垂直线形阵型");
        case 4: return TEXT("三角形阵型");
        case 5: return TEXT("倒三角形阵型");
        default: return FString::Printf(TEXT("自定义阵型_%d"), FormationIndex);
    }
}

FString AFormationTestActor::GetCurrentFormationName() const
{
    return GetFormationName(CurrentFormationIndex);
}

bool AFormationTestActor::IsTransitioning() const
{
    return FormationManager ? FormationManager->IsTransitioning() : false;
}

float AFormationTestActor::GetTransitionProgress() const
{
    return FormationManager ? FormationManager->GetTransitionProgress() : 0.0f;
}

void AFormationTestActor::UpdateAutoLoop(float DeltaTime)
{
    if (FormationManager->IsTransitioning())
    {
        return; // 正在变换中，等待完成
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastTransitionTime >= LoopInterval)
    {
        SwitchToNextFormation();
    }
}

AActor* AFormationTestActor::CreateDefaultUnit(FVector Location)
{
    // 创建一个简单的Actor作为测试单位
    AActor* NewActor = GetWorld()->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
    
    if (NewActor)
    {
        // 添加一个静态网格组件作为可视化
        UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(NewActor);
        NewActor->SetRootComponent(MeshComponent);
        MeshComponent->RegisterComponent();

        // 尝试设置一个默认的立方体网格
        static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube"));
        if (CubeMeshAsset.Succeeded())
        {
            MeshComponent->SetStaticMesh(CubeMeshAsset.Object);
            MeshComponent->SetWorldScale3D(FVector(0.5f, 0.5f, 0.5f));
        }
    }

    return NewActor;
}
