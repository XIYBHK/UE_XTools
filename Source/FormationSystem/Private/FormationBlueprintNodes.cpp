#include "FormationBlueprintNodes.h"
#include "FormationSystemCore.h"
#include "FormationLibrary.h"
#include "FormationMovementComponent.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "GameFramework/PlayerController.h"

bool UFormationBlueprintNodes::QuickFormationTransition(
    const UObject* WorldContext,
    const TArray<AActor*>& Units,
    EFormationType TargetFormationType,
    FTransform FormationTransform,
    UFormationManagerComponent*& OutFormationManager,
    float FormationSize,
    float TransitionDuration,
    EFormationTransitionMode TransitionMode,
    bool bShowDebug)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        OutFormationManager = nullptr;
        return false;
    }

    if (Units.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("QuickFormationTransition: 单位数组为空"));
        OutFormationManager = nullptr;
        return false;
    }

    // 创建临时Actor来承载阵型管理器
    AActor* FormationActor = World->SpawnActor<AActor>();
    if (!FormationActor)
    {
        OutFormationManager = nullptr;
        return false;
    }

    FormationActor->SetActorLocation(FormationTransform.GetLocation());
    FormationActor->SetActorLabel(TEXT("QuickFormationManager"));

    // 创建阵型管理器组件
    OutFormationManager = NewObject<UFormationManagerComponent>(FormationActor);
    FormationActor->AddInstanceComponent(OutFormationManager);
    OutFormationManager->RegisterComponent();

    // 获取当前阵型
    FVector CurrentCenter;
    FFormationData CurrentFormation = UFormationLibrary::GetCurrentFormationFromActors(Units, CurrentCenter);

    // 创建目标阵型（先在原点创建，然后应用完整变换）
    FFormationData TargetFormation = UFormationBlueprintNodes::CreateFormationByType(
        TargetFormationType, FVector::ZeroVector, FormationSize, Units.Num());

    // 应用阵型变换（位置、旋转和缩放）
    TargetFormation = ApplyFormationTransform(TargetFormation, FormationTransform);

    // 配置变换参数
    FFormationTransitionConfig Config;
    Config.TransitionMode = TransitionMode;
    Config.Duration = TransitionDuration;
    Config.bUseEasing = true;
    Config.EasingStrength = 2.0f;
    Config.bShowDebug = bShowDebug;
    Config.DebugDuration = TransitionDuration + 2.0f;

    // 开始变换
    bool bSuccess = OutFormationManager->StartFormationTransition(Units, CurrentFormation, TargetFormation, Config);

    if (bSuccess)
    {
        if (bShowDebug)
        {
            UFormationLibrary::DrawFormationDebug(WorldContext, CurrentFormation, Config.DebugDuration, FLinearColor::Green, 2.0f);
            UFormationLibrary::DrawFormationDebug(WorldContext, TargetFormation, Config.DebugDuration, FLinearColor::Red, 2.0f);
        }
    }
    else
    {
        FormationActor->Destroy();
        OutFormationManager = nullptr;
    }

    return bSuccess;
}

UFormationManagerComponent* UFormationBlueprintNodes::CreateFormationManager(AActor* TargetActor)
{
    if (!IsValid(TargetActor))
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateFormationManager: 目标Actor无效"));
        return nullptr;
    }

    // 检查是否已经有阵型管理器组件
    UFormationManagerComponent* ExistingManager = TargetActor->FindComponentByClass<UFormationManagerComponent>();
    if (ExistingManager)
    {
        return ExistingManager;
    }

    // 创建新的阵型管理器组件
    UFormationManagerComponent* NewManager = NewObject<UFormationManagerComponent>(TargetActor);
    TargetActor->AddInstanceComponent(NewManager);
    NewManager->RegisterComponent();

    return NewManager;
}



UFormationManagerComponent* UFormationBlueprintNodes::FormationTransitionSequence(
    const UObject* WorldContext,
    const TArray<AActor*>& Units,
    const TArray<EFormationType>& FormationSequence,
    FVector CenterLocation,
    float FormationSize,
    float TransitionDuration,
    float SequenceInterval,
    bool bLoop,
    bool bShowDebug)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World || Units.Num() == 0 || FormationSequence.Num() == 0)
    {
        return nullptr;
    }

    // 创建序列管理器Actor
    AActor* SequenceActor = World->SpawnActor<AActor>();
    if (!SequenceActor)
    {
        return nullptr;
    }

    SequenceActor->SetActorLocation(CenterLocation);
    SequenceActor->SetActorLabel(TEXT("FormationSequenceManager"));

    UFormationManagerComponent* FormationManager = NewObject<UFormationManagerComponent>(SequenceActor);
    SequenceActor->AddInstanceComponent(FormationManager);
    FormationManager->RegisterComponent();

    // TODO: 实现序列逻辑（需要额外的序列管理组件）
    // 这里先实现第一个阵型的变换
    if (FormationSequence.Num() > 0)
    {
        FVector CurrentCenter;
        FFormationData CurrentFormation = UFormationLibrary::GetCurrentFormationFromActors(Units, CurrentCenter);
        FFormationData FirstTargetFormation = UFormationBlueprintNodes::CreateFormationByType(FormationSequence[0], CenterLocation, FormationSize, Units.Num());

        FFormationTransitionConfig Config;
        Config.Duration = TransitionDuration;
        Config.bShowDebug = bShowDebug;

        FormationManager->StartFormationTransition(Units, CurrentFormation, FirstTargetFormation, Config);
    }

    return FormationManager;
}

float UFormationBlueprintNodes::GetRecommendedFormationSize(
    int32 UnitCount,
    EFormationType FormationType,
    float UnitSpacing)
{
    if (UnitCount <= 0)
    {
        return 100.0f;
    }

    switch (FormationType)
    {
        case EFormationType::Square:
        {
            int32 SideLength = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(UnitCount)));
            return SideLength * UnitSpacing;
        }
        case EFormationType::Circle:
        {
            // 根据单位数量计算合适的圆形半径
            float Circumference = UnitCount * UnitSpacing;
            return Circumference / (2.0f * UE_PI);
        }
        case EFormationType::Line:
        {
            return UnitCount * UnitSpacing;
        }
        case EFormationType::Triangle:
        {
            int32 Rows = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(UnitCount * 2)));
            return Rows * UnitSpacing;
        }
        default:
            return UnitCount * UnitSpacing * 0.5f;
    }
}



bool UFormationBlueprintNodes::CheckFormationCompatibility(
    const TArray<AActor*>& Units,
    EFormationType FromFormationType,
    EFormationType ToFormationType,
    FString& WarningMessage)
{
    WarningMessage.Empty();

    if (Units.Num() == 0)
    {
        WarningMessage = TEXT("单位数组为空");
        return false;
    }

    // 检查单位是否有效
    int32 ValidUnits = 0;
    for (AActor* Unit : Units)
    {
        if (IsValid(Unit))
        {
            ValidUnits++;
        }
    }

    if (ValidUnits == 0)
    {
        WarningMessage = TEXT("没有有效的单位");
        return false;
    }

    if (ValidUnits != Units.Num())
    {
        WarningMessage = FString::Printf(TEXT("有 %d 个无效单位"), Units.Num() - ValidUnits);
    }

    // 检查特定阵型的限制
    if (FromFormationType == EFormationType::Triangle || ToFormationType == EFormationType::Triangle)
    {
        if (ValidUnits < 3)
        {
            WarningMessage += TEXT("三角形阵型至少需要3个单位");
            return false;
        }
    }

    return true;
}





FFormationData UFormationBlueprintNodes::CreateFormationByType(
    EFormationType FormationType,
    FVector CenterLocation,
    float FormationSize,
    int32 UnitCount)
{
    switch (FormationType)
    {
        case EFormationType::Square:
            return UFormationLibrary::CreateSquareFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize / FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(UnitCount)))));

        case EFormationType::Circle:
            return UFormationLibrary::CreateCircleFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize);

        case EFormationType::Line:
            return UFormationLibrary::CreateLineFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize / FMath::Max(1, UnitCount - 1));

        case EFormationType::Triangle:
            return UFormationLibrary::CreateTriangleFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize / FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(UnitCount)))));

        case EFormationType::Arrow:
            return UFormationLibrary::CreateArrowFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize / FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(UnitCount)))));

        case EFormationType::Spiral:
            return UFormationLibrary::CreateSpiralFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize, 2.0f);

        case EFormationType::SolidCircle:
            return UFormationLibrary::CreateSolidCircleFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize);

        case EFormationType::Zigzag:
            return UFormationLibrary::CreateZigzagFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, FormationSize / FMath::Max(1, UnitCount - 1), FormationSize * 0.3f);

        case EFormationType::Custom:
        default:
            return UFormationLibrary::CreateSquareFormation(CenterLocation, FRotator::ZeroRotator, UnitCount, 100.0f);
    }
}



bool UFormationBlueprintNodes::CharacterFormationMovement(
    const UObject* WorldContext,
    const TArray<ACharacter*>& Characters,
    EFormationType TargetFormationType,
    FTransform FormationTransform,
    TArray<FVector>& OutTargetPositions,
    float FormationSize,
    EFormationTransitionMode TransitionMode,
    bool bUseAIMoveTo,
    float AcceptanceRadius,
    bool bShowDebug)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        OutTargetPositions.Empty();
        return false;
    }

    if (Characters.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CharacterFormationMovement: Character数组为空"));
        OutTargetPositions.Empty();
        return false;
    }

    // 过滤有效的Character
    TArray<ACharacter*> ValidCharacters;
    for (ACharacter* Character : Characters)
    {
        if (IsValid(Character))
        {
            ValidCharacters.Add(Character);
        }
    }

    if (ValidCharacters.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CharacterFormationMovement: 没有有效的Character"));
        OutTargetPositions.Empty();
        return false;
    }

    // 获取当前Character位置
    TArray<FVector> CurrentPositions;
    CurrentPositions.Reserve(ValidCharacters.Num());
    for (ACharacter* Character : ValidCharacters)
    {
        CurrentPositions.Add(Character->GetActorLocation());
    }

    // 创建当前阵型数据
    FVector CurrentCenter;
    FFormationData CurrentFormation = UFormationLibrary::GetCurrentFormationFromActors(
        TArray<AActor*>(reinterpret_cast<AActor* const*>(ValidCharacters.GetData()), ValidCharacters.Num()),
        CurrentCenter
    );

    // 创建目标阵型数据（先在原点创建，然后应用完整变换）
    FFormationData TargetFormation = UFormationBlueprintNodes::CreateFormationByType(
        TargetFormationType, FVector::ZeroVector, FormationSize, ValidCharacters.Num()
    );

    // 应用阵型变换（位置、旋转和缩放）
    TargetFormation = ApplyFormationTransform(TargetFormation, FormationTransform);

    // 获取世界坐标位置
    TArray<FVector> FromPositions = CurrentFormation.GetWorldPositions();
    TArray<FVector> ToPositions = TargetFormation.GetWorldPositions();

    // 添加调试日志：输出阵型AABB信息
    if (FromPositions.Num() > 0 && ToPositions.Num() > 0)
    {
        // 计算起始阵型的包围盒
        FBox FromAABB(FromPositions[0], FromPositions[0]);
        for (const FVector& Pos : FromPositions)
        {
            FromAABB += Pos;
        }

        // 计算目标阵型的包围盒
        FBox ToAABB(ToPositions[0], ToPositions[0]);
        for (const FVector& Pos : ToPositions)
        {
            ToAABB += Pos;
        }

        FVector FromSize = FromAABB.GetSize();
        FVector ToSize = ToAABB.GetSize();

        // 输出调试信息
        UE_LOG(LogFormationSystem, Log, TEXT("CharacterFormationMovement: 阵型AABB分析"));
        UE_LOG(LogFormationSystem, Log, TEXT("  当前阵型: Size=(%s)"), *FromSize.ToString());
        UE_LOG(LogFormationSystem, Log, TEXT("  目标阵型: Size=(%s)"), *ToSize.ToString());
        
        // 相同阵型检测
        const float SizeTolerance = 5.0f;
        bool bIsSameFormation = 
            FMath::Abs(FromSize.X - ToSize.X) < SizeTolerance &&
            FMath::Abs(FromSize.Y - ToSize.Y) < SizeTolerance &&
            FMath::Abs(FromSize.Z - ToSize.Z) < SizeTolerance;
            
        UE_LOG(LogFormationSystem, Log, TEXT("  阵型分析: %s"), 
            bIsSameFormation ? TEXT("相同阵型平移") : TEXT("不同阵型变换"));
    }

    // 使用阵型管理器的分配算法计算最优分配
    UFormationManagerComponent* TempManager = NewObject<UFormationManagerComponent>();
    TArray<int32> Assignment = TempManager->CalculateOptimalAssignment(FromPositions, ToPositions, TransitionMode);

    // 根据分配结果生成目标位置数组
    OutTargetPositions.Empty();
    OutTargetPositions.SetNum(ValidCharacters.Num());

    for (int32 i = 0; i < ValidCharacters.Num(); i++)
    {
        int32 TargetIndex = Assignment.IsValidIndex(i) ? Assignment[i] : i;
        OutTargetPositions[i] = ToPositions.IsValidIndex(TargetIndex) ? ToPositions[TargetIndex] : CurrentPositions[i];
    }

    // 根据bUseAIMoveTo参数选择移动方式
    if (bUseAIMoveTo)
    {
        // 使用AI移动系统
        for (int32 i = 0; i < ValidCharacters.Num(); i++)
        {
            ACharacter* Character = ValidCharacters[i];
            if (IsValid(Character))
            {
                // 尝试获取AI控制器
                if (APawn* Pawn = Cast<APawn>(Character))
                {
                    if (AAIController* AIController = Cast<AAIController>(Pawn->GetController()))
                    {
                        // 使用AI移动到目标位置
                        AIController->MoveToLocation(OutTargetPositions[i], AcceptanceRadius);
                    }
                    else if (APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
                    {
                        // 如果是玩家控制，可以在这里添加其他逻辑
                        UE_LOG(LogTemp, Log, TEXT("CharacterFormationMovement: 玩家控制的角色，跳过AI移动"));
                    }
                }
            }
        }
    }
    else
    {
        // 使用FormationMovementComponent进行移动
        int32 MovingCharacters = 0;
        int32 AddedComponents = 0;

        for (int32 i = 0; i < ValidCharacters.Num(); i++)
        {
            ACharacter* Character = ValidCharacters[i];
            if (IsValid(Character) && OutTargetPositions.IsValidIndex(i))
            {
                // 查找或创建FormationMovementComponent
                UFormationMovementComponent* MovementComp = Character->FindComponentByClass<UFormationMovementComponent>();
                if (!MovementComp)
                {
                    // 如果Character没有FormationMovementComponent，动态添加一个
                    MovementComp = NewObject<UFormationMovementComponent>(Character);
                    Character->AddInstanceComponent(MovementComp);
                    MovementComp->RegisterComponent();
                    AddedComponents++;
                }

                if (MovementComp)
                {
                    // 开始移动到目标位置
                    MovementComp->StartMoveToLocation(OutTargetPositions[i], AcceptanceRadius, 1.0f);
                    MovingCharacters++;
                }
            }
        }

        // 简化日志输出
        FString ComponentInfo = AddedComponents > 0 ? FString::Printf(TEXT("，添加了%d个移动组件"), AddedComponents) : TEXT("");

        UE_LOG(LogTemp, Log, TEXT("CharacterFormationMovement: %d个角色开始移动%s"),
               MovingCharacters, *ComponentInfo);
    }

    // 调试绘制
    if (bShowDebug)
    {
        float DebugDuration = 5.0f;

        // 使用UE5的调试绘制功能
        for (int32 i = 0; i < CurrentPositions.Num(); i++)
        {
            UKismetSystemLibrary::DrawDebugSphere(World, CurrentPositions[i], 30.0f, 8, FLinearColor::Green, DebugDuration, 2.0f);
            UKismetSystemLibrary::DrawDebugString(World, CurrentPositions[i] + FVector(0, 0, 100),
                           FString::Printf(TEXT("C%d"), i), nullptr, FLinearColor::Green, DebugDuration);
        }

        // 绘制目标位置（红色）
        for (int32 i = 0; i < OutTargetPositions.Num(); i++)
        {
            UKismetSystemLibrary::DrawDebugSphere(World, OutTargetPositions[i], 30.0f, 8, FLinearColor::Red, DebugDuration, 2.0f);
            UKismetSystemLibrary::DrawDebugString(World, OutTargetPositions[i] + FVector(0, 0, 100),
                           FString::Printf(TEXT("T%d"), i), nullptr, FLinearColor::Red, DebugDuration);
        }

        // 绘制移动路径（黄色）
        for (int32 i = 0; i < FMath::Min(CurrentPositions.Num(), OutTargetPositions.Num()); i++)
        {
            UKismetSystemLibrary::DrawDebugLine(World, CurrentPositions[i], OutTargetPositions[i],
                         FLinearColor::Yellow, DebugDuration, 3.0f);
        }

        // 绘制阵型中心
        FVector FormationCenter = FormationTransform.GetLocation();
        UKismetSystemLibrary::DrawDebugSphere(World, FormationCenter, 50.0f, 12, FLinearColor::Blue, DebugDuration, 3.0f);
        UKismetSystemLibrary::DrawDebugString(World, FormationCenter + FVector(0, 0, 150),
                       TEXT("Formation Center"), nullptr, FLinearColor::Blue, DebugDuration);
    }

    return true;
}

// ========== RTS群集移动和路径感知算法实现 ==========

bool UFormationBlueprintNodes::RTSFlockFormationTransition(
    const UObject* WorldContext,
    const TArray<AActor*>& Units,
    EFormationType TargetFormationType,
    FVector CenterLocation,
    UFormationManagerComponent*& OutFormationManager,
    float FormationSize,
    float TransitionDuration,
    const FBoidsMovementParams& BoidsParams,
    bool bShowDebug)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        OutFormationManager = nullptr;
        return false;
    }

    if (Units.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("RTSFlockFormationTransition: 单位数组为空"));
        OutFormationManager = nullptr;
        return false;
    }

    // 创建阵型管理器
    AActor* ManagerActor = World->SpawnActor<AActor>();
    if (!ManagerActor)
    {
        OutFormationManager = nullptr;
        return false;
    }

    // 创建阵型管理器组件
    OutFormationManager = NewObject<UFormationManagerComponent>(ManagerActor);
    if (!OutFormationManager)
    {
        ManagerActor->Destroy();
        return false;
    }

    ManagerActor->AddInstanceComponent(OutFormationManager);
    OutFormationManager->RegisterComponent();

    // 设置Boids参数
    OutFormationManager->SetBoidsMovementParams(BoidsParams);

    // 获取当前阵型
    FVector CurrentCenter;
    FFormationData CurrentFormation = UFormationLibrary::GetCurrentFormationFromActors(Units, CurrentCenter);

    // 创建目标阵型
    FFormationData TargetFormation = CreateFormationByType(TargetFormationType, CenterLocation, FormationSize, Units.Num());

    // 配置变换参数（使用RTS群集移动模式）
    FFormationTransitionConfig Config;
    Config.TransitionMode = EFormationTransitionMode::RTSFlockMovement;
    Config.Duration = TransitionDuration;
    Config.bUseEasing = true;
    Config.EasingStrength = 1.5f; // 较温和的缓动
    Config.bShowDebug = bShowDebug;
    Config.DebugDuration = TransitionDuration + 2.0f;

    // 开始变换
    bool bSuccess = OutFormationManager->StartFormationTransition(Units, CurrentFormation, TargetFormation, Config);

    if (bSuccess)
    {
        if (bShowDebug)
        {
            UFormationLibrary::DrawFormationDebug(WorldContext, CurrentFormation, Config.DebugDuration, FLinearColor::Green, 2.0f);
            UFormationLibrary::DrawFormationDebug(WorldContext, TargetFormation, Config.DebugDuration, FLinearColor::Blue, 2.0f);
        }
    }
    else
    {
        ManagerActor->Destroy();
        OutFormationManager = nullptr;
    }

    return bSuccess;
}

bool UFormationBlueprintNodes::PathAwareFormationTransition(
    const UObject* WorldContext,
    const TArray<AActor*>& Units,
    EFormationType TargetFormationType,
    FVector CenterLocation,
    UFormationManagerComponent*& OutFormationManager,
    FPathConflictInfo& OutConflictInfo,
    float FormationSize,
    float TransitionDuration,
    bool bShowDebug)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        OutFormationManager = nullptr;
        return false;
    }

    if (Units.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PathAwareFormationTransition: 单位数组为空"));
        OutFormationManager = nullptr;
        return false;
    }

    // 创建阵型管理器
    AActor* ManagerActor = World->SpawnActor<AActor>();
    if (!ManagerActor)
    {
        OutFormationManager = nullptr;
        return false;
    }

    // 创建阵型管理器组件
    OutFormationManager = NewObject<UFormationManagerComponent>(ManagerActor);
    if (!OutFormationManager)
    {
        ManagerActor->Destroy();
        return false;
    }

    ManagerActor->AddInstanceComponent(OutFormationManager);
    OutFormationManager->RegisterComponent();

    // 获取当前阵型
    FVector CurrentCenter;
    FFormationData CurrentFormation = UFormationLibrary::GetCurrentFormationFromActors(Units, CurrentCenter);

    // 创建目标阵型
    FFormationData TargetFormation = CreateFormationByType(TargetFormationType, CenterLocation, FormationSize, Units.Num());

    // 预先检测路径冲突
    OutConflictInfo = OutFormationManager->CheckFormationPathConflicts(
        CurrentFormation.GetWorldPositions(),
        TargetFormation.GetWorldPositions()
    );



    // 配置变换参数（使用路径感知模式）
    FFormationTransitionConfig Config;
    Config.TransitionMode = EFormationTransitionMode::PathAwareAssignment;
    Config.Duration = TransitionDuration;
    Config.bUseEasing = true;
    Config.EasingStrength = 2.0f;
    Config.bShowDebug = bShowDebug;
    Config.DebugDuration = TransitionDuration + 2.0f;

    // 开始变换
    bool bSuccess = OutFormationManager->StartFormationTransition(Units, CurrentFormation, TargetFormation, Config);

    if (bSuccess)
    {
        if (bShowDebug)
        {
            UFormationLibrary::DrawFormationDebug(WorldContext, CurrentFormation, Config.DebugDuration, FLinearColor::Green, 2.0f);
            UFormationLibrary::DrawFormationDebug(WorldContext, TargetFormation, Config.DebugDuration, FLinearColor::Red, 2.0f);

            // 绘制冲突路径
            if (OutConflictInfo.bHasConflict)
            {
                TArray<FVector> FromPositions = CurrentFormation.GetWorldPositions();
                TArray<FVector> ToPositions = TargetFormation.GetWorldPositions();

                for (const FIntPoint& ConflictPair : OutConflictInfo.ConflictPairs)
                {
                    if (FromPositions.IsValidIndex(ConflictPair.X) && ToPositions.IsValidIndex(ConflictPair.X) &&
                        FromPositions.IsValidIndex(ConflictPair.Y) && ToPositions.IsValidIndex(ConflictPair.Y))
                    {
                        // 绘制冲突路径为红色
                        UKismetSystemLibrary::DrawDebugLine(WorldContext,
                            FromPositions[ConflictPair.X], ToPositions[ConflictPair.X],
                            FLinearColor::Red, Config.DebugDuration, 3.0f);
                        UKismetSystemLibrary::DrawDebugLine(WorldContext,
                            FromPositions[ConflictPair.Y], ToPositions[ConflictPair.Y],
                            FLinearColor::Red, Config.DebugDuration, 3.0f);
                    }
                }
            }
        }
    }
    else
    {
        ManagerActor->Destroy();
        OutFormationManager = nullptr;
    }

    return bSuccess;
}

FFormationData UFormationBlueprintNodes::ApplyFormationTransform(
    const FFormationData& FormationData,
    const FTransform& Transform)
{
    FFormationData TransformedFormation = FormationData;

    // 获取变换信息
    FVector FormationCenter = Transform.GetLocation();
    FRotator AdditionalRotation = Transform.GetRotation().Rotator();
    FVector Scale = Transform.GetScale3D();

    // 对每个位置应用变换
    for (int32 i = 0; i < TransformedFormation.Positions.Num(); i++)
    {
        FVector& Position = TransformedFormation.Positions[i];

        // 1. 应用缩放（相对于原点）
        FVector ScaledPosition = Position * Scale;

        // 2. 应用额外的旋转（在原有旋转基础上）
        FVector RotatedPosition = AdditionalRotation.RotateVector(ScaledPosition);

        // 3. 存储变换后的相对位置（仍然相对于中心）
        Position = RotatedPosition;
    }

    // 更新阵型的中心位置
    TransformedFormation.CenterLocation = FormationCenter;

    // 组合旋转：原有旋转 + 额外旋转
    FQuat OriginalQuat = FormationData.Rotation.Quaternion();
    FQuat AdditionalQuat = AdditionalRotation.Quaternion();
    FQuat CombinedQuat = AdditionalQuat * OriginalQuat;
    TransformedFormation.Rotation = CombinedQuat.Rotator();

    return TransformedFormation;
}