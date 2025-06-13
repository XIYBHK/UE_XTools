#include "MaterialTools/X_MaterialFunctionProcessor.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialFunction.h"
#include "Logging/LogMacros.h"
#include "X_AssetEditor.h"
#include "MaterialTools/X_MaterialFunctionCore.h"
#include "MaterialTools/X_MaterialFunctionCollector.h"
#include "GameFramework/Actor.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "Misc/ScopedSlowTask.h"

void FX_MaterialFunctionProcessor::ProcessAssetMaterialFunction(
    const TArray<FAssetData>& SelectedAssets,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& TargetNode,
    const TSharedPtr<FX_MaterialFunctionParams>& Params)
{
    // 参数验证
    if (SelectedAssets.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("ProcessAssetMaterialFunction: 未选择任何资产"));
        return;
    }
    
    if (!MaterialFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("ProcessAssetMaterialFunction: 材质函数为空"));
        return;
    }

    UE_LOG(LogX_AssetEditor, Log, TEXT("开始处理%d个资产的材质函数应用: %s"), 
        SelectedAssets.Num(), *MaterialFunction->GetName());

    // 收集所有材质，使用并行处理提高性能
    TArray<UMaterial*> Materials = FX_MaterialFunctionCollector::CollectMaterialsFromAssetParallel(SelectedAssets);
    
    if (Materials.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("ProcessAssetMaterialFunction: 从选中资产中未找到任何有效材质"));
        return;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("从%d个资产中收集到%d个材质"), SelectedAssets.Num(), Materials.Num());
    
    // 处理所有材质
    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    
    for (UMaterial* Material : Materials)
    {
        if (Material)
        {
            // 委托给Operation类处理
            UMaterialExpressionMaterialFunctionCall* Result = nullptr;
            
            if (Params.IsValid())
            {
                // 使用用户提供的参数
                Result = FX_MaterialFunctionOperation::AddFunctionToMaterial(
                    Material, 
                    MaterialFunction, 
                    TargetNode, 
                    Params->PosX, 
                    Params->PosY, 
                    Params->bSetupConnections,
                    Params->bEnableSmartConnect,
                    Params->ConnectionMode,
                    Params);
            }
            else
            {
                // 使用默认参数
                Result = FX_MaterialFunctionOperation::AddMaterialFunctionToMaterial(
                    Material, MaterialFunction, TargetNode);
            }
            
            if (Result)
            {
                SuccessCount++;
            }
            else
            {
                FailedCount++;
            }
        }
        else
        {
            FailedCount++;
        }
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("材质函数处理完成: 成功=%d, 失败=%d"), SuccessCount, FailedCount);
}

void FX_MaterialFunctionProcessor::ProcessActorMaterialFunction(
    const TArray<AActor*>& SelectedActors,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& TargetNode,
    const TSharedPtr<FX_MaterialFunctionParams>& Params)
{
    // 参数验证
    if (SelectedActors.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("ProcessActorMaterialFunction: 未选择任何Actor"));
        return;
    }
    
    if (!MaterialFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("ProcessActorMaterialFunction: 材质函数为空"));
        return;
    }

    UE_LOG(LogX_AssetEditor, Log, TEXT("开始处理%d个Actor的材质函数应用: %s"), 
        SelectedActors.Num(), *MaterialFunction->GetName());

    // 收集所有材质，使用并行处理提高性能
    TArray<UMaterial*> Materials = FX_MaterialFunctionCollector::CollectMaterialsFromActorParallel(SelectedActors);
    
    if (Materials.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("ProcessActorMaterialFunction: 从选中Actor中未找到任何有效材质"));
        return;
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("从%d个Actor中收集到%d个材质"), SelectedActors.Num(), Materials.Num());
    
    // 处理所有材质
    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    
    for (UMaterial* Material : Materials)
    {
        if (Material)
        {
            // 委托给Operation类处理
            UMaterialExpressionMaterialFunctionCall* Result = nullptr;
            
            if (Params.IsValid())
            {
                // 使用用户提供的参数
                Result = FX_MaterialFunctionOperation::AddFunctionToMaterial(
                    Material, 
                    MaterialFunction, 
                    TargetNode, 
                    Params->PosX, 
                    Params->PosY, 
                    Params->bSetupConnections,
                    Params->bEnableSmartConnect,
                    Params->ConnectionMode,
                    Params);
            }
            else
            {
                // 使用默认参数
                Result = FX_MaterialFunctionOperation::AddMaterialFunctionToMaterial(
                    Material, MaterialFunction, TargetNode);
            }
            
            if (Result)
            {
                SuccessCount++;
            }
            else
            {
                FailedCount++;
            }
        }
        else
        {
            FailedCount++;
        }
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("Actor材质函数处理完成: 成功=%d, 失败=%d"), SuccessCount, FailedCount);
}

FMaterialProcessResult FX_MaterialFunctionProcessor::AddFunctionToMultipleMaterials(
    const TArray<UObject*>& SourceObjects,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& NodeName,
    int32 PosX,
    int32 PosY,
    bool bSetupConnections,
    const TSharedPtr<FX_MaterialFunctionParams>& Params)
{
    FMaterialProcessResult Result;
    Result.TotalSourceObjects = SourceObjects.Num();
    
    // 参数验证
    if (SourceObjects.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("AddFunctionToMultipleMaterials: 源对象列表为空"));
        return Result;
    }
    
    if (!MaterialFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("AddFunctionToMultipleMaterials: 材质函数为空"));
        return Result;
    }

    UE_LOG(LogX_AssetEditor, Log, TEXT("开始批量添加材质函数 %s 到 %d 个源对象"), 
        *MaterialFunction->GetName(), SourceObjects.Num());

    // 收集所有材质
    TArray<UMaterialInterface*> MaterialsToProcess = FX_MaterialFunctionCollector::CollectMaterialsFromAssets(SourceObjects);
    Result.TotalMaterials = MaterialsToProcess.Num();
    
    if (MaterialsToProcess.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("AddFunctionToMultipleMaterials: 未找到任何材质"));
        return Result;
    }

    // 实际处理每个材质
    UE_LOG(LogX_AssetEditor, Log, TEXT("从 %d 个源对象中收集到 %d 个材质"), SourceObjects.Num(), MaterialsToProcess.Num());
    
    // 创建进度条
    FScopedSlowTask SlowTask(
        MaterialsToProcess.Num(),
        FText::Format(
            FText::FromString(TEXT("正在添加材质函数 {0} 到 {1} 个材质...")),
            FText::FromString(MaterialFunction->GetName()),
            FText::AsNumber(MaterialsToProcess.Num())
        )
    );
    SlowTask.MakeDialog(true); // true表示允许取消
    
    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    int32 AlreadyHasFunctionCount = 0;
    
    // 遍历处理每个材质
    for (UMaterialInterface* MaterialInterface : MaterialsToProcess)
    {
        // 更新进度条
        SlowTask.EnterProgressFrame(1.0f);
        
        // 检查用户是否取消了操作
        if (SlowTask.ShouldCancel())
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("用户取消了材质函数添加操作"));
            break;
        }
        
        if (!MaterialInterface)
        {
            FailedCount++;
            UE_LOG(LogX_AssetEditor, Verbose, TEXT("材质接口为空，跳过"));
            continue;
        }
        
        // 获取基础材质
        UMaterial* BaseMaterial = FX_MaterialFunctionCore::GetBaseMaterial(MaterialInterface);
        if (!BaseMaterial)
        {
            FailedCount++;
            UE_LOG(LogX_AssetEditor, Verbose, TEXT("无法获取基础材质: %s"), *MaterialInterface->GetName());
            continue;
        }
        
        // 检查材质是否已包含此函数
        if (FX_MaterialFunctionOperation::DoesMaterialContainFunction(BaseMaterial, MaterialFunction))
        {
            AlreadyHasFunctionCount++;
            UE_LOG(LogX_AssetEditor, Verbose, TEXT("材质 %s 已包含函数 %s，跳过"), 
                *BaseMaterial->GetName(), *MaterialFunction->GetName());
            continue;
        }
        
        // 添加函数到材质
        UMaterialExpressionMaterialFunctionCall* FunctionCall = 
            FX_MaterialFunctionOperation::AddFunctionToMaterial(BaseMaterial, MaterialFunction, NodeName, 
                PosX, PosY, bSetupConnections, true, EConnectionMode::Add, Params);
        
        if (FunctionCall)
        {
            SuccessCount++;
            UE_LOG(LogX_AssetEditor, Verbose, TEXT("成功添加函数到材质: %s"), *BaseMaterial->GetName());
        }
        else
        {
            FailedCount++;
            UE_LOG(LogX_AssetEditor, Warning, TEXT("添加函数到材质失败: %s"), *BaseMaterial->GetName());
        }
    }
    
    // 更新结果
    Result.SuccessCount = SuccessCount;
    Result.FailedCount = FailedCount;
    Result.AlreadyHasFunctionCount = AlreadyHasFunctionCount;
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("批量添加材质函数完成: %s"), *Result.GetSummaryString());
    return Result;
}

FMaterialProcessResult FX_MaterialFunctionProcessor::AddFresnelToAssets(
    const TArray<UObject*>& SourceObjects)
{
    // 参数验证
    if (SourceObjects.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("AddFresnelToAssets: 源对象列表为空"));
        return FMaterialProcessResult();
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("开始为%d个源对象添加菲涅尔效果"), SourceObjects.Num());
    
    // 获取菲涅尔函数
    UMaterialFunctionInterface* FresnelFunction = FX_MaterialFunctionCore::GetFresnelFunction();
    if (!FresnelFunction)
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("AddFresnelToAssets: 无法获取菲涅尔函数，请确认引擎材质函数库是否完整"));
        return FMaterialProcessResult();
    }
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("成功获取菲涅尔函数: %s"), *FresnelFunction->GetName());
    
    // 创建菲涅尔函数的默认参数
    FX_MaterialFunctionParams FresnelParams;
    FresnelParams.NodeName = "Fresnel";
    FresnelParams.PosX = 0; // 设为0，使用自动位置计算
    FresnelParams.PosY = 0; // 设为0，使用自动位置计算
    FresnelParams.bSetupConnections = true;
    FresnelParams.bConnectToEmissive = true; // 菲涅尔通常连接到自发光
    FresnelParams.ConnectionMode = EConnectionMode::None; // 对于菲涅尔函数，使用直接连接方式
    
    // 使用自动计算的位置
    UE_LOG(LogX_AssetEditor, Log, TEXT("添加菲涅尔函数，使用自动位置计算，连接到自发光通道"));
    return AddFunctionToMultipleMaterials(SourceObjects, FresnelFunction, FName(FresnelParams.NodeName), 
        0, 0, FresnelParams.bSetupConnections, MakeShared<FX_MaterialFunctionParams>(FresnelParams));
} 