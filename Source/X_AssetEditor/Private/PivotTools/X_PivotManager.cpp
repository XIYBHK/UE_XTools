/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "PivotTools/X_PivotManager.h"
#include "PivotTools/X_PivotOperation.h"
#include "X_AssetEditor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Editor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "X_PivotManager"

DEFINE_LOG_CATEGORY(LogX_PivotTools);

FX_PivotOperationResult FX_PivotManager::SetPivotForAssets(
    const TArray<FAssetData>& SelectedAssets,
    EPivotBoundsPoint BoundsPoint)
{
    FX_PivotOperationResult Result;
    
    LogOperation(FString::Printf(TEXT("开始为 %d 个资产设置 Pivot"), SelectedAssets.Num()));

    // 显示进度对话框
    FScopedSlowTask Progress((float)SelectedAssets.Num(),
        LOCTEXT("SettingPivot", "正在修改网格 Pivot..."));
    Progress.MakeDialog();

    for (const FAssetData& AssetData : SelectedAssets)
    {
        Progress.EnterProgressFrame(1.0f,
            FText::Format(LOCTEXT("ProcessingMesh", "处理: {0}"),
            FText::FromName(AssetData.AssetName)));

        if (!IsStaticMeshAsset(AssetData))
        {
            Result.SkippedCount++;
            LogOperation(FString::Printf(TEXT("跳过非静态网格体资产: %s"), *AssetData.AssetName.ToString()));
            continue;
        }

        UStaticMesh* StaticMesh = GetStaticMeshFromAsset(AssetData);
        if (!StaticMesh)
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("无法加载静态网格体: %s"), *AssetData.AssetName.ToString());
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
            continue;
        }

        // 查找场景中所有使用该资产的 Actor，记录它们的位置和旋转
        TArray<TPair<AStaticMeshActor*, FTransform>> ActorsToCompensate;
        if (GEditor && GEditor->GetEditorWorldContext().World())
        {
            UWorld* World = GEditor->GetEditorWorldContext().World();
            for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
            {
                AStaticMeshActor* SMActor = *It;
                if (SMActor && SMActor->GetStaticMeshComponent())
                {
                    if (SMActor->GetStaticMeshComponent()->GetStaticMesh() == StaticMesh)
                    {
                        ActorsToCompensate.Add(TPair<AStaticMeshActor*, FTransform>(
                            SMActor, 
                            SMActor->GetActorTransform()
                        ));
                    }
                }
            }
        }

        // 计算修改前的边界盒中心（用于补偿计算）
        FVector OldCenter = StaticMesh->GetBoundingBox().GetCenter();

        FString ErrorMessage;
        if (SetPivotForStaticMesh(StaticMesh, BoundsPoint, ErrorMessage))
        {
            // 计算修改后的边界盒中心
            FVector NewCenter = StaticMesh->GetBoundingBox().GetCenter();
            FVector Offset = NewCenter - OldCenter;

            // 补偿场景中所有使用该资产的 Actor 位置
            for (const auto& Pair : ActorsToCompensate)
            {
                AStaticMeshActor* SMActor = Pair.Key;
                FTransform OriginalTransform = Pair.Value;
                
                // 顶点向 +Offset 方向移动，Actor 需要向 -Offset 方向移动来保持世界位置
                FVector PivotOffsetWorld = OriginalTransform.GetRotation().RotateVector(-Offset);
                FVector NewActorLocation = OriginalTransform.GetLocation() + PivotOffsetWorld;
                
                SMActor->Modify();
                SMActor->SetActorLocation(NewActorLocation);
                SMActor->MarkPackageDirty();
                
                if (UStaticMeshComponent* MeshComp = SMActor->GetStaticMeshComponent())
                {
                    MeshComp->UpdateComponentToWorld();
                }
            }
            
            // 刷新视口
            if (ActorsToCompensate.Num() > 0)
            {
                GEditor->RedrawLevelEditingViewports(true);
                GEditor->NoteSelectionChange();
            }

            Result.SuccessCount++;
            FString SuccessMsg = FString::Printf(TEXT("成功设置 Pivot: %s (补偿了 %d 个场景Actor)"), 
                *AssetData.AssetName.ToString(), ActorsToCompensate.Num());
            Result.SuccessMessages.Add(SuccessMsg);
            LogOperation(SuccessMsg);
        }
        else
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("设置 Pivot 失败: %s - %s"), 
                *AssetData.AssetName.ToString(), *ErrorMessage);
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
        }
    }

    ShowOperationResult(Result, TEXT("设置 Pivot"));
    return Result;
}

FX_PivotOperationResult FX_PivotManager::SetPivotForActors(
    const TArray<AActor*>& SelectedActors,
    EPivotBoundsPoint BoundsPoint)
{
    FX_PivotOperationResult Result;
    
    LogOperation(FString::Printf(TEXT("开始为 %d 个 Actor 设置 Pivot"), SelectedActors.Num()));

    // 显示进度对话框
    FScopedSlowTask Progress((float)SelectedActors.Num(),
        LOCTEXT("SettingPivotForActors", "正在修改 Actor Pivot..."));
    Progress.MakeDialog();

    for (AActor* Actor : SelectedActors)
    {
        if (!Actor)
        {
            Result.SkippedCount++;
            continue;
        }

        Progress.EnterProgressFrame(1.0f,
            FText::Format(LOCTEXT("ProcessingActor", "处理: {0}"),
            FText::FromString(Actor->GetActorLabel())));

        FString ErrorMessage;
        if (SetPivotForStaticMeshActor(Actor, BoundsPoint, ErrorMessage))
        {
            Result.SuccessCount++;
            FString SuccessMsg = FString::Printf(TEXT("成功设置 Actor Pivot: %s"), *Actor->GetActorLabel());
            Result.SuccessMessages.Add(SuccessMsg);
            LogOperation(SuccessMsg);
        }
        else
        {
            if (ErrorMessage.IsEmpty())
            {
                Result.SkippedCount++;
            }
            else
            {
                Result.FailureCount++;
                FString ErrorMsg = FString::Printf(TEXT("设置 Actor Pivot 失败: %s - %s"), 
                    *Actor->GetActorLabel(), *ErrorMessage);
                Result.ErrorMessages.Add(ErrorMsg);
                LogOperation(ErrorMsg, true);
            }
        }
    }

    ShowOperationResult(Result, TEXT("设置 Actor Pivot"));
    return Result;
}

FX_PivotOperationResult FX_PivotManager::SetPivotToCenterForAssets(const TArray<FAssetData>& SelectedAssets)
{
    return SetPivotForAssets(SelectedAssets, EPivotBoundsPoint::Center);
}

FX_PivotOperationResult FX_PivotManager::SetPivotToCenterForActors(const TArray<AActor*>& SelectedActors)
{
    return SetPivotForActors(SelectedActors, EPivotBoundsPoint::Center);
}

bool FX_PivotManager::IsStaticMeshAsset(const FAssetData& AssetData)
{
    return AssetData.AssetClassPath.GetAssetName().ToString() == TEXT("StaticMesh");
}

UStaticMesh* FX_PivotManager::GetStaticMeshFromAsset(const FAssetData& AssetData)
{
    if (!IsStaticMeshAsset(AssetData))
    {
        return nullptr;
    }

    return Cast<UStaticMesh>(AssetData.GetAsset());
}

void FX_PivotManager::ShowOperationResult(const FX_PivotOperationResult& Result, const FString& OperationName)
{
    if (Result.GetTotalCount() == 0)
    {
        return;
    }

    if (Result.FailureCount == 0 && Result.SkippedCount == 0)
    {
        // 全部成功
        FNotificationInfo Info(FText::Format(
            LOCTEXT("PivotSuccess", "{0} 完成：成功处理 {1} 个网格"),
            FText::FromString(OperationName),
            FText::AsNumber(Result.SuccessCount)));
        Info.Image = FAppStyle::GetBrush(TEXT("LevelEditor.RecompileGameCode.Success"));
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    else
    {
        // 部分失败或有跳过
        FText Message = FText::Format(
            LOCTEXT("PivotPartialSuccess", "{0} 完成：成功 {1}，失败 {2}，跳过 {3}"),
            FText::FromString(OperationName),
            FText::AsNumber(Result.SuccessCount),
            FText::AsNumber(Result.FailureCount),
            FText::AsNumber(Result.SkippedCount));

        if (!Result.ErrorMessages.IsEmpty())
        {
            FString ErrorDetails = FString::Join(Result.ErrorMessages, TEXT("\n"));
            Message = FText::Format(
                LOCTEXT("PivotPartialSuccessWithDetails", "{0}\n\n错误详情：\n{1}"),
                Message,
                FText::FromString(ErrorDetails));
        }

        FMessageDialog::Open(EAppMsgType::Ok, Message, 
            FText::Format(LOCTEXT("PivotOperationResult", "{0} 结果"), FText::FromString(OperationName)));
    }
}

bool FX_PivotManager::SetPivotForStaticMesh(
    UStaticMesh* StaticMesh,
    EPivotBoundsPoint BoundsPoint,
    FString& OutErrorMessage)
{
    if (!StaticMesh)
    {
        OutErrorMessage = TEXT("静态网格体为空");
        return false;
    }

    FX_PivotOperation Operation(StaticMesh);
    return Operation.Execute(BoundsPoint, OutErrorMessage);
}

bool FX_PivotManager::SetPivotForStaticMeshActor(
    AActor* SMActor,
    EPivotBoundsPoint BoundsPoint,
    FString& OutErrorMessage)
{
    if (!SMActor)
    {
        OutErrorMessage = TEXT("Actor 为空");
        return false;
    }

    // 获取 StaticMeshComponent
    AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(SMActor);
    if (!StaticMeshActor)
    {
        // 不是 StaticMeshActor，跳过（不算错误）
        return false;
    }

    UStaticMeshComponent* MeshComponent = StaticMeshActor->GetStaticMeshComponent();
    if (!MeshComponent || !MeshComponent->GetStaticMesh())
    {
        OutErrorMessage = TEXT("Actor 没有有效的静态网格体组件");
        return false;
    }

    UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
    
    // 记录原始世界位置和旋转
    FVector OriginalWorldLocation = StaticMeshActor->GetActorLocation();
    FRotator OriginalWorldRotation = StaticMeshActor->GetActorRotation();
    
    // 计算网格边界盒（本地空间）
    FBox MeshBounds = StaticMesh->GetBoundingBox();
    
    // 特殊处理：世界原点模式
    if (BoundsPoint == EPivotBoundsPoint::WorldOrigin)
    {
        // 目标：让 Pivot 在世界原点 (0,0,0)，网格在世界空间的位置不变
        // 方法：
        // 1. 将顶点向 Actor 世界位置方向移动（本地空间）
        // 2. 将 Actor 位置设置为世界原点
        // 结果：网格在世界空间的位置不变，但 Pivot 现在在世界原点
        
        // 将 Actor 世界位置转换到本地空间（考虑旋转）
        FVector LocalOffset = OriginalWorldRotation.UnrotateVector(OriginalWorldLocation);
        
        // 应用偏移到网格顶点
        FX_PivotOperation Operation(StaticMesh);
        FString ErrorMessage;
        if (!Operation.Execute(LocalOffset, ErrorMessage))
        {
            OutErrorMessage = ErrorMessage;
            return false;
        }
        
        // 将 Actor 位置设置为世界原点
        StaticMeshActor->Modify();
        StaticMeshActor->SetActorLocation(FVector::ZeroVector);
        
        UE_LOG(LogX_PivotTools, Log, TEXT("世界原点模式: 原Actor位置=(%s), 本地偏移=(%s), 新Actor位置=(0,0,0)"),
            *OriginalWorldLocation.ToString(),
            *LocalOffset.ToString());
    }
    else
    {
        // 普通模式：设置 Pivot 到指定位置，保持 Actor 世界位置不变
        
        // 计算目标 Pivot 点（本地空间）
        FVector TargetPivotLocal = CalculateTargetPoint(MeshBounds, BoundsPoint);
        
        // 修改网格 Pivot
        FString ErrorMessage;
        if (!SetPivotForStaticMesh(StaticMesh, BoundsPoint, ErrorMessage))
        {
            OutErrorMessage = ErrorMessage;
            return false;
        }

        // 补偿 Actor 位置：
        // 顶点向 -TargetPivotLocal 方向移动，Actor 需要向 +TargetPivotLocal 方向移动来补偿
        FVector PivotOffsetWorld = OriginalWorldRotation.RotateVector(TargetPivotLocal);
        FVector NewActorLocation = OriginalWorldLocation + PivotOffsetWorld;
        
        UE_LOG(LogX_PivotTools, Log, TEXT("普通模式: 原位置=(%s), 本地偏移=(%s), 世界偏移=(%s), 新位置=(%s)"),
            *OriginalWorldLocation.ToString(),
            *TargetPivotLocal.ToString(),
            *PivotOffsetWorld.ToString(),
            *NewActorLocation.ToString());
        
        // 使用事务系统来支持 Undo 并触发编辑器更新
        StaticMeshActor->Modify();
        StaticMeshActor->SetActorLocation(NewActorLocation);
    }
    
    // 标记 Actor 已修改，触发编辑器刷新
    StaticMeshActor->MarkPackageDirty();
    
    // 触发完整的属性变更通知
    FPropertyChangedEvent PropertyChangedEvent(nullptr);
    StaticMeshActor->PostEditChangeProperty(PropertyChangedEvent);
    
    // 更新组件变换
    if (MeshComponent)
    {
        MeshComponent->UpdateComponentToWorld();
    }
    
    // 强制刷新视口显示和选择
    GEditor->RedrawLevelEditingViewports(true);
    GEditor->NoteSelectionChange();

    return true;
}

FVector FX_PivotManager::CalculateTargetPoint(
    const FBox& BoundingBox,
    EPivotBoundsPoint BoundsPoint)
{
    FVector Center = BoundingBox.GetCenter();
    FVector Min = BoundingBox.Min;
    FVector Max = BoundingBox.Max;

    switch (BoundsPoint)
    {
        case EPivotBoundsPoint::Center:
            return Center;

        case EPivotBoundsPoint::Bottom:
            return FVector(Center.X, Center.Y, Min.Z);

        case EPivotBoundsPoint::Top:
            return FVector(Center.X, Center.Y, Max.Z);

        case EPivotBoundsPoint::Left:
            return FVector(Min.X, Center.Y, Center.Z);

        case EPivotBoundsPoint::Right:
            return FVector(Max.X, Center.Y, Center.Z);

        case EPivotBoundsPoint::Front:
            return FVector(Center.X, Max.Y, Center.Z);

        case EPivotBoundsPoint::Back:
            return FVector(Center.X, Min.Y, Center.Z);

        case EPivotBoundsPoint::WorldOrigin:
            return FVector::ZeroVector;

        default:
            return Center;
    }
}

void FX_PivotManager::LogOperation(const FString& Message, bool bIsError)
{
    if (bIsError)
    {
        UE_LOG(LogX_PivotTools, Error, TEXT("%s"), *Message);
    }
    else
    {
        UE_LOG(LogX_PivotTools, Log, TEXT("%s"), *Message);
    }
}

// 初始化静态成员
TMap<FSoftObjectPath, FX_PivotSnapshot> FX_PivotManager::PivotSnapshots;

FX_PivotOperationResult FX_PivotManager::RecordPivotSnapshots(const TArray<FAssetData>& SelectedAssets)
{
    FX_PivotOperationResult Result;
    
    LogOperation(FString::Printf(TEXT("开始记录 %d 个资产的 Pivot 快照"), SelectedAssets.Num()));

    for (const FAssetData& AssetData : SelectedAssets)
    {
        if (!IsStaticMeshAsset(AssetData))
        {
            Result.SkippedCount++;
            continue;
        }

        UStaticMesh* StaticMesh = GetStaticMeshFromAsset(AssetData);
        if (!StaticMesh)
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("无法加载静态网格体: %s"), *AssetData.AssetName.ToString());
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
            continue;
        }

        // 创建快照
        FX_PivotSnapshot Snapshot;
        Snapshot.MeshPath = FSoftObjectPath(StaticMesh);
        Snapshot.BoundsCenter = StaticMesh->GetBoundingBox().GetCenter();
        Snapshot.Timestamp = FDateTime::Now();

        // 存储快照
        PivotSnapshots.Add(Snapshot.MeshPath, Snapshot);

        Result.SuccessCount++;
        FString SuccessMsg = FString::Printf(TEXT("已记录 Pivot: %s (中心=%s)"), 
            *AssetData.AssetName.ToString(), 
            *Snapshot.BoundsCenter.ToString());
        Result.SuccessMessages.Add(SuccessMsg);
        LogOperation(SuccessMsg);
    }

    // 显示结果
    FText Message = FText::Format(
        LOCTEXT("RecordPivotSuccess", "Pivot 记录完成：成功 {0}，失败 {1}，跳过 {2}\n当前共有 {3} 个快照"),
        FText::AsNumber(Result.SuccessCount),
        FText::AsNumber(Result.FailureCount),
        FText::AsNumber(Result.SkippedCount),
        FText::AsNumber(PivotSnapshots.Num()));

    FNotificationInfo Info(Message);
    Info.Image = FAppStyle::GetBrush(TEXT("LevelEditor.RecompileGameCode.Success"));
    Info.ExpireDuration = 3.0f;
    FSlateNotificationManager::Get().AddNotification(Info);

    // 自动保存到磁盘
    SaveSnapshotsToDisk();

    return Result;
}

FX_PivotOperationResult FX_PivotManager::RestorePivotSnapshots(const TArray<FAssetData>& SelectedAssets)
{
    FX_PivotOperationResult Result;
    
    if (PivotSnapshots.Num() == 0)
    {
        FMessageDialog::Open(EAppMsgType::Ok, 
            LOCTEXT("NoSnapshots", "没有可用的 Pivot 快照\n请先使用\"记录 Pivot\"功能"),
            LOCTEXT("RestorePivotTitle", "还原 Pivot"));
        return Result;
    }

    LogOperation(FString::Printf(TEXT("开始还原 %d 个资产的 Pivot"), SelectedAssets.Num()));

    FScopedSlowTask Progress((float)SelectedAssets.Num(),
        LOCTEXT("RestoringPivot", "正在还原 Pivot..."));
    Progress.MakeDialog();

    for (const FAssetData& AssetData : SelectedAssets)
    {
        Progress.EnterProgressFrame(1.0f,
            FText::Format(LOCTEXT("RestoringMesh", "还原: {0}"),
            FText::FromName(AssetData.AssetName)));

        if (!IsStaticMeshAsset(AssetData))
        {
            Result.SkippedCount++;
            continue;
        }

        UStaticMesh* StaticMesh = GetStaticMeshFromAsset(AssetData);
        if (!StaticMesh)
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("无法加载静态网格体: %s"), *AssetData.AssetName.ToString());
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
            continue;
        }

        // 查找快照
        FSoftObjectPath MeshPath(StaticMesh);
        FX_PivotSnapshot* Snapshot = PivotSnapshots.Find(MeshPath);
        
        if (!Snapshot)
        {
            Result.SkippedCount++;
            LogOperation(FString::Printf(TEXT("未找到快照: %s"), *AssetData.AssetName.ToString()));
            continue;
        }

        // 计算当前边界盒中心
        FVector CurrentCenter = StaticMesh->GetBoundingBox().GetCenter();
        
        // 计算需要的偏移量：从当前中心移动到记录的中心
        FVector Offset = Snapshot->BoundsCenter - CurrentCenter;

        // 如果偏移量很小，跳过
        if (Offset.IsNearlyZero(0.001f))
        {
            Result.SkippedCount++;
            LogOperation(FString::Printf(TEXT("Pivot 已经在目标位置: %s"), *AssetData.AssetName.ToString()));
            continue;
        }

        // 查找场景中所有使用该资产的 Actor，记录它们的位置和旋转
        TArray<TPair<AStaticMeshActor*, FTransform>> ActorsToCompensate;
        if (GEditor && GEditor->GetEditorWorldContext().World())
        {
            UWorld* World = GEditor->GetEditorWorldContext().World();
            for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
            {
                AStaticMeshActor* SMActor = *It;
                if (SMActor && SMActor->GetStaticMeshComponent())
                {
                    if (SMActor->GetStaticMeshComponent()->GetStaticMesh() == StaticMesh)
                    {
                        ActorsToCompensate.Add(TPair<AStaticMeshActor*, FTransform>(
                            SMActor, 
                            SMActor->GetActorTransform()
                        ));
                    }
                }
            }
        }

        // 应用偏移
        FX_PivotOperation Operation(StaticMesh);
        FString ErrorMessage;
        if (Operation.Execute(Offset, ErrorMessage))
        {
            // 补偿场景中所有使用该资产的 Actor 位置
            for (const auto& Pair : ActorsToCompensate)
            {
                AStaticMeshActor* SMActor = Pair.Key;
                FTransform OriginalTransform = Pair.Value;
                
                // 顶点向 +Offset 方向移动，Actor 需要向 -Offset 方向移动来保持世界位置
                FVector PivotOffsetWorld = OriginalTransform.GetRotation().RotateVector(-Offset);
                FVector NewActorLocation = OriginalTransform.GetLocation() + PivotOffsetWorld;
                
                SMActor->Modify();
                SMActor->SetActorLocation(NewActorLocation);
                SMActor->MarkPackageDirty();
                
                if (UStaticMeshComponent* MeshComp = SMActor->GetStaticMeshComponent())
                {
                    MeshComp->UpdateComponentToWorld();
                }
            }
            
            // 刷新视口
            if (ActorsToCompensate.Num() > 0)
            {
                GEditor->RedrawLevelEditingViewports(true);
                GEditor->NoteSelectionChange();
            }

            Result.SuccessCount++;
            FString SuccessMsg = FString::Printf(TEXT("成功还原 Pivot: %s (补偿了 %d 个场景Actor)"), 
                *AssetData.AssetName.ToString(), ActorsToCompensate.Num());
            Result.SuccessMessages.Add(SuccessMsg);
            LogOperation(SuccessMsg);
        }
        else
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("还原 Pivot 失败: %s - %s"), 
                *AssetData.AssetName.ToString(), *ErrorMessage);
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
        }
    }

    ShowOperationResult(Result, TEXT("还原 Pivot"));
    return Result;
}

FX_PivotOperationResult FX_PivotManager::RestorePivotSnapshotsForActors(const TArray<AActor*>& SelectedActors)
{
    FX_PivotOperationResult Result;
    
    if (PivotSnapshots.Num() == 0)
    {
        FMessageDialog::Open(EAppMsgType::Ok, 
            LOCTEXT("NoSnapshots", "没有可用的 Pivot 快照\n请先使用\"记录 Pivot\"功能"),
            LOCTEXT("RestorePivotTitle", "还原 Pivot"));
        return Result;
    }

    LogOperation(FString::Printf(TEXT("开始为 %d 个 Actor 还原 Pivot"), SelectedActors.Num()));

    for (AActor* Actor : SelectedActors)
    {
        AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
        if (!StaticMeshActor)
        {
            Result.SkippedCount++;
            continue;
        }

        UStaticMeshComponent* MeshComponent = StaticMeshActor->GetStaticMeshComponent();
        if (!MeshComponent || !MeshComponent->GetStaticMesh())
        {
            Result.SkippedCount++;
            continue;
        }

        UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
        FSoftObjectPath MeshPath(StaticMesh);
        
        // 查找快照
        FX_PivotSnapshot* Snapshot = PivotSnapshots.Find(MeshPath);
        if (!Snapshot)
        {
            Result.SkippedCount++;
            LogOperation(FString::Printf(TEXT("未找到快照: %s"), *StaticMesh->GetName()));
            continue;
        }

        // 计算当前边界盒中心
        FVector CurrentCenter = StaticMesh->GetBoundingBox().GetCenter();
        
        // 计算需要的偏移量
        FVector Offset = Snapshot->BoundsCenter - CurrentCenter;

        // 如果偏移量很小，跳过
        if (Offset.IsNearlyZero(0.001f))
        {
            Result.SkippedCount++;
            LogOperation(FString::Printf(TEXT("Pivot 已经在目标位置: %s"), *StaticMesh->GetName()));
            continue;
        }

        // 记录原始世界位置和旋转
        FVector OriginalWorldLocation = StaticMeshActor->GetActorLocation();
        FRotator OriginalWorldRotation = StaticMeshActor->GetActorRotation();

        // 应用偏移到网格
        FX_PivotOperation Operation(StaticMesh);
        FString ErrorMessage;
        if (!Operation.Execute(Offset, ErrorMessage))
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("还原 Pivot 失败: %s - %s"), 
                *StaticMesh->GetName(), *ErrorMessage);
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
            continue;
        }

        // 补偿 Actor 位置：顶点向 +Offset 方向移动，Actor 需要向 -Offset 方向移动来保持世界位置
        FVector PivotOffsetWorld = OriginalWorldRotation.RotateVector(-Offset);
        FVector NewActorLocation = OriginalWorldLocation + PivotOffsetWorld;

        // 使用事务系统来支持 Undo
        StaticMeshActor->Modify();
        StaticMeshActor->SetActorLocation(NewActorLocation);

        // 标记 Actor 已修改，触发编辑器刷新
        StaticMeshActor->MarkPackageDirty();
        FPropertyChangedEvent PropertyChangedEvent(nullptr);
        StaticMeshActor->PostEditChangeProperty(PropertyChangedEvent);
        
        if (MeshComponent)
        {
            MeshComponent->UpdateComponentToWorld();
        }

        Result.SuccessCount++;
        FString SuccessMsg = FString::Printf(TEXT("成功还原 Actor Pivot: %s"), *StaticMesh->GetName());
        Result.SuccessMessages.Add(SuccessMsg);
        LogOperation(SuccessMsg);
    }

    // 刷新视口
    GEditor->RedrawLevelEditingViewports(true);
    GEditor->NoteSelectionChange();

    ShowOperationResult(Result, TEXT("还原 Actor Pivot"));
    return Result;
}

void FX_PivotManager::ClearPivotSnapshots()
{
    int32 Count = PivotSnapshots.Num();
    PivotSnapshots.Empty();
    
    // 删除磁盘文件
    FString FilePath = GetSnapshotFilePath();
    if (FPaths::FileExists(FilePath))
    {
        IFileManager::Get().Delete(*FilePath);
        LogOperation(FString::Printf(TEXT("已删除快照文件: %s"), *FilePath));
    }
    
    FText Message = FText::Format(
        LOCTEXT("ClearSnapshotsSuccess", "已清除 {0} 个 Pivot 快照"),
        FText::AsNumber(Count));

    FNotificationInfo Info(Message);
    Info.Image = FAppStyle::GetBrush(TEXT("Icons.Delete"));
    Info.ExpireDuration = 2.0f;
    FSlateNotificationManager::Get().AddNotification(Info);

    LogOperation(FString::Printf(TEXT("已清除 %d 个 Pivot 快照"), Count));
}

int32 FX_PivotManager::GetSnapshotCount()
{
    return PivotSnapshots.Num();
}

FString FX_PivotManager::GetSnapshotFilePath()
{
    // 保存到项目的 Saved 目录
    return FPaths::ProjectSavedDir() / TEXT("XTools") / TEXT("PivotSnapshots.json");
}

bool FX_PivotManager::SaveSnapshotsToDisk()
{
    if (PivotSnapshots.Num() == 0)
    {
        LogOperation(TEXT("没有快照需要保存"));
        return true;
    }

    FString FilePath = GetSnapshotFilePath();
    
    // 创建 JSON 对象
    TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
    
    // 添加版本信息
    RootObject->SetStringField(TEXT("Version"), TEXT("1.0"));
    RootObject->SetStringField(TEXT("SaveTime"), FDateTime::Now().ToString());
    
    // 创建快照数组
    TArray<TSharedPtr<FJsonValue>> SnapshotsArray;
    
    for (const auto& Pair : PivotSnapshots)
    {
        const FX_PivotSnapshot& Snapshot = Pair.Value;
        
        TSharedPtr<FJsonObject> SnapshotObject = MakeShared<FJsonObject>();
        SnapshotObject->SetStringField(TEXT("MeshPath"), Snapshot.MeshPath.ToString());
        SnapshotObject->SetNumberField(TEXT("CenterX"), Snapshot.BoundsCenter.X);
        SnapshotObject->SetNumberField(TEXT("CenterY"), Snapshot.BoundsCenter.Y);
        SnapshotObject->SetNumberField(TEXT("CenterZ"), Snapshot.BoundsCenter.Z);
        SnapshotObject->SetStringField(TEXT("Timestamp"), Snapshot.Timestamp.ToString());
        
        SnapshotsArray.Add(MakeShared<FJsonValueObject>(SnapshotObject));
    }
    
    RootObject->SetArrayField(TEXT("Snapshots"), SnapshotsArray);
    
    // 序列化为字符串
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    
    if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
    {
        LogOperation(TEXT("序列化 JSON 失败"), true);
        return false;
    }
    
    // 确保目录存在
    FString Directory = FPaths::GetPath(FilePath);
    if (!IFileManager::Get().DirectoryExists(*Directory))
    {
        IFileManager::Get().MakeDirectory(*Directory, true);
    }
    
    // 写入文件
    if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
    {
        LogOperation(FString::Printf(TEXT("保存快照文件失败: %s"), *FilePath), true);
        return false;
    }
    
    LogOperation(FString::Printf(TEXT("成功保存 %d 个快照到: %s"), PivotSnapshots.Num(), *FilePath));
    return true;
}

bool FX_PivotManager::LoadSnapshotsFromDisk()
{
    FString FilePath = GetSnapshotFilePath();
    
    // 检查文件是否存在
    if (!FPaths::FileExists(FilePath))
    {
        LogOperation(TEXT("快照文件不存在，跳过加载"));
        return true; // 不算错误
    }
    
    // 读取文件
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        LogOperation(FString::Printf(TEXT("读取快照文件失败: %s"), *FilePath), true);
        return false;
    }
    
    // 解析 JSON
    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        LogOperation(TEXT("解析 JSON 失败"), true);
        return false;
    }
    
    // 读取快照数组
    const TArray<TSharedPtr<FJsonValue>>* SnapshotsArray;
    if (!RootObject->TryGetArrayField(TEXT("Snapshots"), SnapshotsArray))
    {
        LogOperation(TEXT("JSON 格式错误：缺少 Snapshots 字段"), true);
        return false;
    }
    
    // 清空现有快照
    PivotSnapshots.Empty();
    
    // 加载快照
    for (const TSharedPtr<FJsonValue>& Value : *SnapshotsArray)
    {
        const TSharedPtr<FJsonObject>* SnapshotObject;
        if (!Value->TryGetObject(SnapshotObject))
        {
            continue;
        }
        
        FX_PivotSnapshot Snapshot;
        
        FString MeshPathStr;
        if ((*SnapshotObject)->TryGetStringField(TEXT("MeshPath"), MeshPathStr))
        {
            Snapshot.MeshPath = FSoftObjectPath(MeshPathStr);
        }
        
        double X = 0.0, Y = 0.0, Z = 0.0;
        if ((*SnapshotObject)->TryGetNumberField(TEXT("CenterX"), X) &&
            (*SnapshotObject)->TryGetNumberField(TEXT("CenterY"), Y) &&
            (*SnapshotObject)->TryGetNumberField(TEXT("CenterZ"), Z))
        {
            Snapshot.BoundsCenter = FVector(X, Y, Z);
        }
        
        FString TimestampStr;
        if ((*SnapshotObject)->TryGetStringField(TEXT("Timestamp"), TimestampStr))
        {
            FDateTime::Parse(TimestampStr, Snapshot.Timestamp);
        }
        
        if (Snapshot.IsValid())
        {
            PivotSnapshots.Add(Snapshot.MeshPath, Snapshot);
        }
    }
    
    LogOperation(FString::Printf(TEXT("成功加载 %d 个快照从: %s"), PivotSnapshots.Num(), *FilePath));
    return true;
}

#undef LOCTEXT_NAMESPACE
