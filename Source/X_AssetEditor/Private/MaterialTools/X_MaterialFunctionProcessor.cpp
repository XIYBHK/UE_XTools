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

void FX_MaterialFunctionProcessor::ProcessAssetMaterialFunction(
    const TArray<FAssetData>& SelectedAssets,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& TargetNode)
{
    if (!MaterialFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质函数为空"));
        return;
    }

    // 收集所有材质，使用并行处理提高性能
    TArray<UMaterial*> Materials = FX_MaterialFunctionCollector::CollectMaterialsFromAssetParallel(SelectedAssets);
    
    // 处理所有材质
    for (UMaterial* Material : Materials)
    {
        if (Material)
        {
            // 委托给Operation类处理
            FX_MaterialFunctionOperation::AddMaterialFunctionToMaterial(Material, MaterialFunction, TargetNode);
        }
    }
}

void FX_MaterialFunctionProcessor::ProcessActorMaterialFunction(
    const TArray<AActor*>& SelectedActors,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& TargetNode)
{
    if (!MaterialFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质函数为空"));
        return;
    }

    // 收集所有材质，使用并行处理提高性能
    TArray<UMaterial*> Materials = FX_MaterialFunctionCollector::CollectMaterialsFromActorParallel(SelectedActors);
    
    // 处理所有材质
    for (UMaterial* Material : Materials)
    {
        if (Material)
        {
            // 委托给Operation类处理
            FX_MaterialFunctionOperation::AddMaterialFunctionToMaterial(Material, MaterialFunction, TargetNode);
        }
    }
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
    
    if (!MaterialFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质函数为空"));
        return Result;
    }

    // 收集所有材质
    TArray<UMaterialInterface*> MaterialsToProcess = FX_MaterialFunctionCollector::CollectMaterialsFromAssets(SourceObjects);
    Result.TotalMaterials = MaterialsToProcess.Num();
    
    if (MaterialsToProcess.Num() == 0)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("未找到任何材质"));
        return Result;
    }

    // 实际处理每个材质
    UE_LOG(LogX_AssetEditor, Log, TEXT("处理 %d 个材质"), MaterialsToProcess.Num());
    
    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    int32 AlreadyHasFunctionCount = 0;
    
    // 遍历处理每个材质
    for (UMaterialInterface* MaterialInterface : MaterialsToProcess)
    {
        if (!MaterialInterface)
        {
            FailedCount++;
            continue;
        }
        
        // 获取基础材质
        UMaterial* BaseMaterial = FX_MaterialFunctionCore::GetBaseMaterial(MaterialInterface);
        if (!BaseMaterial)
        {
            FailedCount++;
            continue;
        }
        
        // 检查材质是否已包含此函数
        if (FX_MaterialFunctionOperation::DoesMaterialContainFunction(BaseMaterial, MaterialFunction))
        {
            AlreadyHasFunctionCount++;
            continue;
        }
        
        // 添加函数到材质
        UMaterialExpressionMaterialFunctionCall* FunctionCall = 
            FX_MaterialFunctionOperation::AddFunctionToMaterial(BaseMaterial, MaterialFunction, NodeName, 
                PosX, PosY, bSetupConnections, true, EConnectionMode::Add, Params);
        
        if (FunctionCall)
        {
            SuccessCount++;
        }
        else
        {
            FailedCount++;
        }
    }
    
    // 更新结果
    Result.SuccessCount = SuccessCount;
    Result.FailedCount = FailedCount;
    Result.AlreadyHasFunctionCount = AlreadyHasFunctionCount;
    
    UE_LOG(LogX_AssetEditor, Log, TEXT("%s"), *Result.GetSummaryString());
    return Result;
}

FMaterialProcessResult FX_MaterialFunctionProcessor::AddFresnelToAssets(
    const TArray<UObject*>& SourceObjects)
{
    // 获取菲涅尔函数
    UMaterialFunctionInterface* FresnelFunction = FX_MaterialFunctionCore::GetFresnelFunction();
    if (!FresnelFunction)
    {
        UE_LOG(LogX_AssetEditor, Error, TEXT("无法获取菲涅尔函数"));
        return FMaterialProcessResult();
    }
    
    // 创建菲涅尔函数的默认参数
    FX_MaterialFunctionParams FresnelParams;
    FresnelParams.NodeName = "Fresnel";
    FresnelParams.PosX = 0; // 设为0，使用自动位置计算
    FresnelParams.PosY = 0; // 设为0，使用自动位置计算
    FresnelParams.bSetupConnections = true;
    FresnelParams.bConnectToEmissive = true; // 菲涅尔通常连接到自发光
    FresnelParams.ConnectionMode = EConnectionMode::None; // 对于菲涅尔函数，使用直接连接方式
    
    // 使用自动计算的位置
    UE_LOG(LogX_AssetEditor, Log, TEXT("添加菲涅尔函数，使用自动位置计算"));
    return AddFunctionToMultipleMaterials(SourceObjects, FresnelFunction, FName(FresnelParams.NodeName), 
        0, 0, FresnelParams.bSetupConnections, MakeShared<FX_MaterialFunctionParams>(FresnelParams));
} 