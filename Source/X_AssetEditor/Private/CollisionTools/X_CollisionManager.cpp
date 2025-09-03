// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/X_CollisionManager.h"
#include "X_AssetEditor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConvexElem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/MessageDialog.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "StaticMeshEditorSubsystem.h"
#include "StaticMeshEditorSubsystemHelpers.h"

DEFINE_LOG_CATEGORY(LogX_CollisionManager);

FX_CollisionOperationResult FX_CollisionManager::RemoveCollisionFromAssets(const TArray<FAssetData>& SelectedAssets)
{
    FX_CollisionOperationResult Result;
    
    LogOperation(FString::Printf(TEXT("开始移除 %d 个资产的碰撞"), SelectedAssets.Num()));

    for (const FAssetData& AssetData : SelectedAssets)
    {
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

        if (RemoveCollisionFromMesh(StaticMesh))
        {
            Result.SuccessCount++;
            LogOperation(FString::Printf(TEXT("成功移除碰撞: %s"), *AssetData.AssetName.ToString()));
        }
        else
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("移除碰撞失败: %s"), *AssetData.AssetName.ToString());
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
        }
    }

    ShowOperationResult(Result, TEXT("移除碰撞"));
    return Result;
}

FX_CollisionOperationResult FX_CollisionManager::AddConvexCollisionToAssets(const TArray<FAssetData>& SelectedAssets)
{
    FX_CollisionOperationResult Result;
    
    LogOperation(FString::Printf(TEXT("开始为 %d 个资产添加凸包碰撞"), SelectedAssets.Num()));

    for (const FAssetData& AssetData : SelectedAssets)
    {
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

        if (AddConvexCollisionToMesh(StaticMesh))
        {
            Result.SuccessCount++;
            LogOperation(FString::Printf(TEXT("成功添加凸包碰撞: %s"), *AssetData.AssetName.ToString()));
        }
        else
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("添加凸包碰撞失败: %s"), *AssetData.AssetName.ToString());
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
        }
    }

    ShowOperationResult(Result, TEXT("添加凸包碰撞"));
    return Result;
}

FX_CollisionOperationResult FX_CollisionManager::SetCollisionComplexity(const TArray<FAssetData>& SelectedAssets, EX_CollisionComplexity ComplexityType)
{
    FX_CollisionOperationResult Result;
    ECollisionTraceFlag TraceFlag = ConvertToCollisionTraceFlag(ComplexityType);
    
    FString ComplexityName;
    switch (ComplexityType)
    {
        case EX_CollisionComplexity::UseDefault:
            ComplexityName = TEXT("项目默认");
            break;
        case EX_CollisionComplexity::UseSimpleAndComplex:
            ComplexityName = TEXT("简单与复杂");
            break;
        case EX_CollisionComplexity::UseSimpleAsComplex:
            ComplexityName = TEXT("将简单碰撞用作复杂碰撞");
            break;
        case EX_CollisionComplexity::UseComplexAsSimple:
            ComplexityName = TEXT("将复杂碰撞用作简单碰撞");
            break;
    }
    
    LogOperation(FString::Printf(TEXT("开始为 %d 个资产设置碰撞复杂度为: %s"), SelectedAssets.Num(), *ComplexityName));

    for (const FAssetData& AssetData : SelectedAssets)
    {
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

        if (SetMeshCollisionComplexity(StaticMesh, TraceFlag))
        {
            Result.SuccessCount++;
            LogOperation(FString::Printf(TEXT("成功设置碰撞复杂度: %s -> %s"), *AssetData.AssetName.ToString(), *ComplexityName));
        }
        else
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("设置碰撞复杂度失败: %s"), *AssetData.AssetName.ToString());
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
        }
    }

    ShowOperationResult(Result, FString::Printf(TEXT("设置碰撞复杂度为%s"), *ComplexityName));
    return Result;
}

FX_CollisionOperationResult FX_CollisionManager::AddSimpleCollisionToAssets(const TArray<FAssetData>& SelectedAssets, uint8 ShapeType)
{
    FX_CollisionOperationResult Result;

    LogOperation(FString::Printf(TEXT("开始为 %d 个资产添加简单碰撞，类型=%d"), SelectedAssets.Num(), (int32)ShapeType));

    UStaticMeshEditorSubsystem* SMEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>() : nullptr;
    if (!SMEditorSubsystem)
    {
        Result.FailureCount = SelectedAssets.Num();
        Result.ErrorMessages.Add(TEXT("无法获取 UStaticMeshEditorSubsystem"));
        ShowOperationResult(Result, TEXT("添加简单碰撞"));
        return Result;
    }

    for (const FAssetData& AssetData : SelectedAssets)
    {
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

        const int32 PrimIndex = SMEditorSubsystem->AddSimpleCollisionsWithNotification(StaticMesh, static_cast<EScriptCollisionShapeType>(ShapeType), true);
        if (PrimIndex >= 0)
        {
            Result.SuccessCount++;
            LogOperation(FString::Printf(TEXT("成功添加简单碰撞: %s (PrimIndex=%d)"), *AssetData.AssetName.ToString(), PrimIndex));
        }
        else
        {
            Result.FailureCount++;
            FString ErrorMsg = FString::Printf(TEXT("添加简单碰撞失败: %s"), *AssetData.AssetName.ToString());
            Result.ErrorMessages.Add(ErrorMsg);
            LogOperation(ErrorMsg, true);
        }
    }

    ShowOperationResult(Result, TEXT("添加简单碰撞"));
    return Result;
}

bool FX_CollisionManager::IsStaticMeshAsset(const FAssetData& AssetData)
{
    return AssetData.AssetClassPath.GetAssetName().ToString() == TEXT("StaticMesh");
}

UStaticMesh* FX_CollisionManager::GetStaticMeshFromAsset(const FAssetData& AssetData)
{
    if (!IsStaticMeshAsset(AssetData))
    {
        return nullptr;
    }

    return Cast<UStaticMesh>(AssetData.GetAsset());
}

void FX_CollisionManager::ShowOperationResult(const FX_CollisionOperationResult& Result, const FString& OperationName)
{
    FString NotificationText;
    FString DetailText;

    if (Result.IsSuccess() && Result.SuccessCount > 0)
    {
        NotificationText = FString::Printf(TEXT("%s操作完成: 成功处理 %d 个资产"), *OperationName, Result.SuccessCount);
        if (Result.SkippedCount > 0)
        {
            NotificationText += FString::Printf(TEXT("，跳过 %d 个非静态网格体"), Result.SkippedCount);
        }
    }
    else if (Result.FailureCount > 0)
    {
        NotificationText = FString::Printf(TEXT("%s操作部分失败: 成功 %d，失败 %d"), *OperationName, Result.SuccessCount, Result.FailureCount);
        if (Result.SkippedCount > 0)
        {
            NotificationText += FString::Printf(TEXT("，跳过 %d 个"), Result.SkippedCount);
        }

        // 构建详细错误信息
        if (Result.ErrorMessages.Num() > 0)
        {
            DetailText = TEXT("错误详情:\n");
            for (int32 i = 0; i < FMath::Min(Result.ErrorMessages.Num(), 5); ++i) // 最多显示5个错误
            {
                DetailText += FString::Printf(TEXT("• %s\n"), *Result.ErrorMessages[i]);
            }
            if (Result.ErrorMessages.Num() > 5)
            {
                DetailText += FString::Printf(TEXT("... 还有 %d 个错误"), Result.ErrorMessages.Num() - 5);
            }
        }
    }
    else
    {
        NotificationText = FString::Printf(TEXT("%s操作完成: 没有找到可处理的静态网格体资产"), *OperationName);
    }

    // 显示通知
    FNotificationInfo Info(FText::FromString(NotificationText));
    Info.bFireAndForget = true;
    Info.FadeOutDuration = 3.0f;
    Info.ExpireDuration = 5.0f;

    if (Result.IsSuccess())
    {
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("NotificationList.SuccessImage"));
    }
    else
    {
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("NotificationList.FailImage"));
    }

    FSlateNotificationManager::Get().AddNotification(Info);

    // 如果有错误且错误数量较多，显示详细对话框
    if (!DetailText.IsEmpty() && Result.FailureCount > 3)
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(DetailText), FText::FromString(OperationName + TEXT(" 错误详情")));
    }
}

bool FX_CollisionManager::RemoveCollisionFromMesh(UStaticMesh* StaticMesh)
{
    if (!StaticMesh || !StaticMesh->GetBodySetup())
    {
        return false;
    }

    try
    {
        UBodySetup* BodySetup = StaticMesh->GetBodySetup();

        // 清除所有碰撞数据
        BodySetup->RemoveSimpleCollision();

        // 标记为已修改
        BodySetup->MarkPackageDirty();
        StaticMesh->MarkPackageDirty();

        // 重新构建碰撞
        BodySetup->CreatePhysicsMeshes();

        // 保存修改
        SaveStaticMeshChanges(StaticMesh);

        return true;
    }
    catch (const std::exception& e)
    {
        LogOperation(FString::Printf(TEXT("移除碰撞时发生异常: %s"), ANSI_TO_TCHAR(e.what())), true);
        return false;
    }
}

bool FX_CollisionManager::AddConvexCollisionToMesh(UStaticMesh* StaticMesh)
{
    if (!StaticMesh || !StaticMesh->GetBodySetup())
    {
        return false;
    }

    try
    {
        UBodySetup* BodySetup = StaticMesh->GetBodySetup();

        // 先清除现有的简单碰撞
        BodySetup->RemoveSimpleCollision();

        // 添加凸包碰撞
        // 使用静态网格体的渲染数据生成凸包
        if (StaticMesh->GetNumLODs() > 0)
        {
            // 为每个LOD生成凸包（通常只使用LOD0）
            const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];

            // 创建凸包元素
            FKConvexElem ConvexElem;

            // 从顶点数据生成凸包
            TArray<FVector> Vertices;
            const int32 NumVertices = LODResource.GetNumVertices();

            for (int32 VertIndex = 0; VertIndex < NumVertices; ++VertIndex)
            {
                FVector Vertex = (FVector)LODResource.VertexBuffers.PositionVertexBuffer.VertexPosition(VertIndex);
                Vertices.Add(Vertex);
            }

            // 设置凸包顶点
            ConvexElem.VertexData = Vertices;
            ConvexElem.UpdateElemBox();

            // 添加到BodySetup
            BodySetup->AggGeom.ConvexElems.Add(ConvexElem);
        }

        // 标记为已修改
        BodySetup->MarkPackageDirty();
        StaticMesh->MarkPackageDirty();

        // 重新构建碰撞
        BodySetup->CreatePhysicsMeshes();

        // 保存修改
        SaveStaticMeshChanges(StaticMesh);

        return true;
    }
    catch (const std::exception& e)
    {
        LogOperation(FString::Printf(TEXT("添加凸包碰撞时发生异常: %s"), ANSI_TO_TCHAR(e.what())), true);
        return false;
    }
}

bool FX_CollisionManager::SetMeshCollisionComplexity(UStaticMesh* StaticMesh, ECollisionTraceFlag TraceFlag)
{
    if (!StaticMesh || !StaticMesh->GetBodySetup())
    {
        return false;
    }

    // ✅ 使用UE错误处理模式，不使用异常
    UBodySetup* BodySetup = StaticMesh->GetBodySetup();
    if (!BodySetup)
    {
        LogOperation(TEXT("无法获取BodySetup，设置碰撞复杂度失败"), true);
        return false;
    }

    // 设置碰撞复杂度
    BodySetup->CollisionTraceFlag = TraceFlag;

    // 标记为已修改
    BodySetup->MarkPackageDirty();
    StaticMesh->MarkPackageDirty();

    // 重新构建碰撞
    BodySetup->CreatePhysicsMeshes();

    // 保存修改
    SaveStaticMeshChanges(StaticMesh);
    LogOperation(TEXT("碰撞复杂度设置成功"), false);

    return true;
}

void FX_CollisionManager::SaveStaticMeshChanges(UStaticMesh* StaticMesh)
{
    if (!StaticMesh)
    {
        return;
    }

    // 标记包为脏状态
    StaticMesh->MarkPackageDirty();

    // 如果资产编辑器打开，刷新显示
    if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
    {
        if (AssetEditorSubsystem->FindEditorForAsset(StaticMesh, false))
        {
            // 刷新资产编辑器
            AssetEditorSubsystem->CloseAllEditorsForAsset(StaticMesh);
        }
    }
}

void FX_CollisionManager::LogOperation(const FString& Message, bool bIsError)
{
    if (bIsError)
    {
        UE_LOG(LogX_CollisionManager, Error, TEXT("%s"), *Message);
    }
    else
    {
        UE_LOG(LogX_CollisionManager, Log, TEXT("%s"), *Message);
    }
}



ECollisionTraceFlag FX_CollisionManager::ConvertToCollisionTraceFlag(EX_CollisionComplexity ComplexityType)
{
    switch (ComplexityType)
    {
        case EX_CollisionComplexity::UseDefault:
            return CTF_UseDefault;
        case EX_CollisionComplexity::UseSimpleAndComplex:
            return CTF_UseSimpleAndComplex;
        case EX_CollisionComplexity::UseSimpleAsComplex:
            return CTF_UseSimpleAsComplex;
        case EX_CollisionComplexity::UseComplexAsSimple:
            return CTF_UseComplexAsSimple;
        default:
            return CTF_UseDefault;
    }
}
