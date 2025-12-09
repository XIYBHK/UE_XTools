#include "MaterialTools/X_MaterialFunctionOperation.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Logging/LogMacros.h"
#include "X_AssetEditor.h"
#include "MaterialTools/X_MaterialFunctionCore.h"

// 拆分后模块
#include "MaterialTools/X_MaterialFunctionCollector.h"
#include "MaterialTools/X_MaterialFunctionConnector.h"
#include "MaterialTools/X_MaterialFunctionProcessor.h"

// 并行处理相关
#include "Async/ParallelFor.h"
#include "Engine/SkinnedAssetCommon.h"
#include "HAL/PlatformProcess.h"

// 参数相关
#include "MaterialTools/X_MaterialFunctionParams.h"

#define LOCTEXT_NAMESPACE "X_MaterialFunctionOperation"

// 屏幕消息显示
#include "Engine/Engine.h"

// 撤销支持
#include "ScopedTransaction.h"

// UE官方材质编辑API
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"


/**
 * 检查材质是否为引擎自带材质
 * @param Material 要检查的材质
 * @return 如果是引擎材质返回true
 */
static bool IsEngineMaterial(UMaterial* Material)
{
    if (!Material)
    {
        return false;
    }

    // 获取材质的包路径
    FString PackagePath = Material->GetPackage()->GetName();

    // 检查是否在引擎目录下
    return PackagePath.StartsWith(TEXT("/Engine/")) ||
           PackagePath.StartsWith(TEXT("/Game/Engine/")) ||
           PackagePath.Contains(TEXT("Engine/Content/"));
}

/**
 * 检查表达式是否为简单的常量节点
 * @param Expression 要检查的表达式
 * @return 如果是简单常量节点返回true
 */
static bool IsSimpleConstantExpression(UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return false;
    }

    // 使用类名检查，兼容所有 UE 版本
    FString ClassName = Expression->GetClass()->GetName();

    // 检查是否为常量或参数类型
    if (ClassName.Contains(TEXT("Constant")) ||
        ClassName.Contains(TEXT("Parameter")))
    {
        return true;
    }

    return false;
}

/**
 * 显示屏幕消息
 * @param Message 要显示的消息
 * @param bIsError 是否为错误消息
 */
static void ShowScreenMessage(const FString& Message, bool bIsError = true)
{
    if (GEngine)
    {
        FColor MessageColor = bIsError ? FColor::Red : FColor::Green;
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, MessageColor, Message);
    }

    // 同时输出到日志
    if (bIsError)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("%s"), *Message);
    }
    else
    {
        UE_LOG(LogX_AssetEditor, Log, TEXT("%s"), *Message);
    }
}

UMaterial* FX_MaterialFunctionOperation::GetBaseMaterial(UMaterialInterface* MaterialInterface)
{
    return FX_MaterialFunctionCore::GetBaseMaterial(MaterialInterface);
}

void FX_MaterialFunctionOperation::ProcessAssetMaterialFunction(
    const TArray<FAssetData>& SelectedAssets,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& TargetNode,
    const TSharedPtr<FX_MaterialFunctionParams>& Params)
{
    // 委托给处理器类处理
    FX_MaterialFunctionProcessor::ProcessAssetMaterialFunction(SelectedAssets, MaterialFunction, TargetNode, Params);
}

void FX_MaterialFunctionOperation::ProcessActorMaterialFunction(
    const TArray<AActor*>& SelectedActors,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& TargetNode,
    const TSharedPtr<FX_MaterialFunctionParams>& Params)
{
    // 委托给处理器类处理
    FX_MaterialFunctionProcessor::ProcessActorMaterialFunction(SelectedActors, MaterialFunction, TargetNode, Params);
}

TArray<UMaterial*> FX_MaterialFunctionOperation::CollectMaterialsFromAsset(const FAssetData& Asset)
{
    // 委托给收集器类处理
    return FX_MaterialFunctionCollector::CollectMaterialsFromAsset(Asset);
}

TArray<UMaterial*> FX_MaterialFunctionOperation::CollectMaterialsFromActor(AActor* Actor)
{
    // 委托给收集器类处理
    return FX_MaterialFunctionCollector::CollectMaterialsFromActor(Actor);
}

TArray<UMaterial*> FX_MaterialFunctionOperation::CollectMaterialsFromAssetParallel(const TArray<FAssetData>& Assets)
{
    // 委托给收集器类处理
    return FX_MaterialFunctionCollector::CollectMaterialsFromAssetParallel(Assets);
}

TArray<UMaterial*> FX_MaterialFunctionOperation::CollectMaterialsFromActorParallel(const TArray<AActor*>& Actors)
{
    // 委托给收集器类处理
    return FX_MaterialFunctionCollector::CollectMaterialsFromActorParallel(Actors);
}

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionOperation::AddMaterialFunctionToMaterial(
    UMaterial* Material,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& TargetNode,
    const TSharedPtr<FX_MaterialFunctionParams>& UserParams)
{
    if (!Material || !MaterialFunction)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或材质函数为空"));
        return nullptr;
    }

    // 收集所有材质 - 委托给收集器类
    TArray<UMaterial*> Materials = FX_MaterialFunctionCollector::CollectMaterialsFromAsset(FAssetData(Material));
    
    // 处理所有材质
    UMaterialExpressionMaterialFunctionCall* Result = nullptr;
    for (UMaterial* CurrentMaterial : Materials)
    {
        if (CurrentMaterial)
        {
            Result = AddFunctionToMaterial(CurrentMaterial, MaterialFunction, TargetNode, UserParams);
            if (Result)
            {
                // 如果成功添加了函数，返回结果
                return Result;
            }
        }
    }
    
    return Result;
}

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionOperation::FindNodeInMaterial(
    UMaterial* Material, 
    const FName& NodeName)
{
    if (!Material)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质为空"));
        return nullptr;
    }

    // 遍历所有材质表达式
    TArrayView<const TObjectPtr<UMaterialExpression>> ExpressionsView = Material->GetExpressions();
    
    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : ExpressionsView)
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            if (FunctionCall->MaterialFunction)
            {
                FString FunctionCallNameStr = FunctionCall->GetName();
                FString NodeNameStr = NodeName.ToString();
                if (FunctionCallNameStr.Equals(NodeNameStr, ESearchCase::CaseSensitive))
                {
                    return const_cast<UMaterialExpressionMaterialFunctionCall*>(FunctionCall);
                }
            }
        }
    }

    return nullptr;
}

bool FX_MaterialFunctionOperation::DoesMaterialContainFunction(
    UMaterial* Material,
    UMaterialFunctionInterface* Function)
{
    if (!Material || !Function)
    {
        return false;
    }

    TArrayView<const TObjectPtr<UMaterialExpression>> ExpressionsView = Material->GetExpressions();
    
    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : ExpressionsView)
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            if (FunctionCall->MaterialFunction == Function)
            {
                return true;
            }
        }
    }

    return false;
}

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionOperation::AddFunctionToMaterial(
    UMaterial* Material,
    UMaterialFunctionInterface* Function,
    const FName& NodeName,
    int32 PosX,
    int32 PosY,
    bool bSetupConnections,
    bool bEnableSmartConnect,
    EConnectionMode ConnectionMode,
    const TSharedPtr<FX_MaterialFunctionParams>& UserParams)
{
    if (!Material || !Function)
    {
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质或函数为空"));
        return nullptr;
    }

    // 如果材质编辑器已打开，先关闭以避免冲突
    bool bWasEditorOpen = false;
    if (GEditor)
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            TArray<IAssetEditorInstance*> OpenEditors = AssetEditorSubsystem->FindEditorsForAsset(Material);
            if (OpenEditors.Num() > 0)
            {
                bWasEditorOpen = true;
                AssetEditorSubsystem->CloseAllEditorsForAsset(Material);
                UE_LOG(LogX_AssetEditor, Log, TEXT("材质编辑器已打开，先关闭以避免冲突"));
            }
        }
    }
    
    // 不使用FScopedTransaction，避免在材质编辑器中撤销时崩溃
    // 用户可以通过删除节点来移除添加的材质函数

    //  检查是否为引擎自带材质
    if (IsEngineMaterial(Material))
    {
        FText ErrorText = FText::Format(
            LOCTEXT("CannotModifyEngineMaterial", "无法修改引擎自带材质: {0}\n修改引擎材质易导致崩溃，请复制材质到项目文件夹后再操作"),
            FText::FromString(Material->GetName()));

        ShowScreenMessage(ErrorText.ToString(), true);
        return nullptr;
    }

    // 检查材质是否已包含该函数
    if (DoesMaterialContainFunction(Material, Function))
    {
        FText WarningText = FText::Format(
            LOCTEXT("MaterialAlreadyContainsFunction", "材质 {0} 已包含函数 {1}，跳过重复添加"),
            FText::FromString(Material->GetName()),
            FText::FromString(Function->GetName()));

        ShowScreenMessage(WarningText.ToString(), true);
        UE_LOG(LogX_AssetEditor, Warning, TEXT("材质 %s 已包含函数 %s"),
            *Material->GetName(), *Function->GetName());
        return nullptr;
    }
    
    // 创建材质函数调用表达式
    UMaterialExpressionMaterialFunctionCall* FunctionCall = CreateMaterialFunctionCallExpression(
        Material, Function, PosX, PosY);
    
    if (FunctionCall && bSetupConnections)
    {
        // 设置自动连接 - 委托给连接器类
        FX_MaterialFunctionConnector::SetupAutoConnections(Material, FunctionCall, ConnectionMode, UserParams);
    }
    
    // 直接设置材质函数表达式的描述
    if (FunctionCall)
    {
        // 设置函数调用表达式的描述信息
        FString DescriptionText = FString::Printf(TEXT("%s\n添加时间: %s"), 
            *Function->GetName(), 
            *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
        
        // 设置描述
        FunctionCall->Desc = DescriptionText;
        
        // 开启评论气泡功能
        FunctionCall->bCommentBubbleVisible = true;
        
        // 设置表达式名称，使其在材质编辑器中更容易识别
        FunctionCall->MaterialExpressionEditorX -= 15; // 稍微调整位置以便更好地显示描述
    }
    
    // 标记材质为已修改
    Material->MarkPackageDirty();
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    
    // 刷新材质编辑器
    FX_MaterialFunctionCore::RefreshOpenMaterialEditor(Material);
    
    return FunctionCall;
}

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionOperation::AddFunctionToMaterial(
    UMaterial* Material,
    UMaterialFunctionInterface* Function,
    const FName& NodeName,
    const TSharedPtr<FX_MaterialFunctionParams>& UserParams)
{
    // 如果提供了参数，使用参数中的位置和连接设置
    int32 PosX = UserParams.IsValid() ? UserParams->PosX : 0;
    int32 PosY = UserParams.IsValid() ? UserParams->PosY : 0;
    bool bSetupConnections = UserParams.IsValid() ? UserParams->bSetupConnections : true;
    bool bEnableSmartConnect = UserParams.IsValid() ? UserParams->bEnableSmartConnect : true;
    EConnectionMode ConnectionMode = UserParams.IsValid() ? UserParams->ConnectionMode : EConnectionMode::Add;
    
    // 调用完整版本的函数
    return AddFunctionToMaterial(Material, Function, NodeName, PosX, PosY, bSetupConnections, bEnableSmartConnect, ConnectionMode, UserParams);
}

bool FX_MaterialFunctionOperation::ConnectExpressionToMaterialProperty(
    UMaterial* Material,
    UMaterialExpression* Expression,
    EMaterialProperty MaterialProperty,
    int32 OutputIndex)
{
    // 委托给连接器类
    return FX_MaterialFunctionConnector::ConnectExpressionToMaterialProperty(Material, Expression, MaterialProperty, OutputIndex);
}

bool FX_MaterialFunctionOperation::ConnectExpressionToMaterialPropertyByName(
    UMaterial* Material,
    UMaterialExpression* Expression,
    const FString& PropertyName,
    int32 OutputIndex)
{
    // 委托给连接器类
    return FX_MaterialFunctionConnector::ConnectExpressionToMaterialPropertyByName(Material, Expression, PropertyName, OutputIndex);
}

FMaterialProcessResult FX_MaterialFunctionOperation::AddFunctionToMultipleMaterials(
    const TArray<UObject*>& SourceObjects,
    UMaterialFunctionInterface* MaterialFunction,
    const FName& NodeName,
    int32 PosX,
    int32 PosY,
    bool bSetupConnections,
    const TSharedPtr<FX_MaterialFunctionParams>& Params)
{
    // 委托给处理器类
    return FX_MaterialFunctionProcessor::AddFunctionToMultipleMaterials(SourceObjects, MaterialFunction, NodeName, 
        PosX, PosY, bSetupConnections, Params);
}

FMaterialProcessResult FX_MaterialFunctionOperation::AddFresnelToAssets(
    const TArray<UObject*>& SourceObjects)
{
    // 委托给处理器类
    return FX_MaterialFunctionProcessor::AddFresnelToAssets(SourceObjects);
}

TArray<UMaterialInterface*> FX_MaterialFunctionOperation::CollectMaterialsFromAssets(
    TArray<UObject*> SourceObjects)
{
    // 委托给收集器类
    return FX_MaterialFunctionCollector::CollectMaterialsFromAssets(SourceObjects);
}

UMaterialExpressionMaterialFunctionCall* FX_MaterialFunctionOperation::CreateMaterialFunctionCallExpression(
    UMaterial* Material,
    UMaterialFunctionInterface* Function,
    int32 PosX,
    int32 PosY)
{
    // 使用UE官方API创建材质表达式（自动处理RF_Transactional、GUID、Material属性等）
    UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(
        Material, UMaterialExpressionMaterialFunctionCall::StaticClass(), PosX, PosY);
    
    UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(NewExpression);
    if (FunctionCall)
    {
        // 设置材质函数引用
        FunctionCall->SetMaterialFunction(Function);
        
        // 如果PosX和PosY都是0，进行智能位置计算
        if (PosX == 0 && PosY == 0)
        {
            // 获取材质编辑器数据
            UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
            if (EditorOnlyData)
            {
                // 搜集材质中的相关表达式信息
                UMaterialExpression* BaseColorExpr = EditorOnlyData->BaseColor.Expression;
                UMaterialExpression* EmissiveExpr = EditorOnlyData->EmissiveColor.Expression;
                UMaterialExpression* MetallicExpr = EditorOnlyData->Metallic.Expression;
                UMaterialExpression* RoughnessExpr = EditorOnlyData->Roughness.Expression;
                
                // 检查是否启用了MaterialAttributes模式，并查找MakeMaterialAttributes节点
                UMaterialExpression* MakeMaterialAttributesNode = nullptr;
                if (EditorOnlyData->MaterialAttributes.IsConnected())
                {
                    // 回溯查找MakeMaterialAttributes节点
                    UMaterialExpression* CurrentExpr = EditorOnlyData->MaterialAttributes.Expression;
                    while (CurrentExpr)
                    {
                        if (UMaterialExpressionMakeMaterialAttributes* MakeMANode = Cast<UMaterialExpressionMakeMaterialAttributes>(CurrentExpr))
                        {
                            MakeMaterialAttributesNode = MakeMANode;
                            UE_LOG(LogX_AssetEditor, Log, TEXT("找到MakeMaterialAttributes节点用于位置计算"));
                            break;
                        }
                        // 如果是MaterialFunctionCall，继续向前查找其输入
                        if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(CurrentExpr))
                        {
                            // 查找第一个连接的输入
                            bool bFoundInput = false;
                            for (const FFunctionExpressionInput& Input : FuncCall->FunctionInputs)
                            {
                                if (Input.Input.IsConnected() && Input.Input.Expression)
                                {
                                    CurrentExpr = Input.Input.Expression;
                                    bFoundInput = true;
                                    break;
                                }
                            }
                            if (!bFoundInput)
                                break;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                
                // 获取材质中所有表达式中心点
                int32 CenterX = 0;
                int32 CenterY = 0;
                int32 ExprCount = 0;
                
                TArrayView<const TObjectPtr<UMaterialExpression>> AllExpressions = Material->GetExpressions();
                for (const TObjectPtr<UMaterialExpression>& ExprPtr : AllExpressions)
                {
                    UMaterialExpression* Expr = ExprPtr.Get();
                    if (!Expr) continue;
                    
                    CenterX += Expr->MaterialExpressionEditorX;
                    CenterY += Expr->MaterialExpressionEditorY;
                    ExprCount++;
                }
                
                // 如果材质中有表达式，计算中心点
                if (ExprCount > 0)
                {
                    CenterX /= ExprCount;
                    CenterY /= ExprCount;
                }
                
                // 根据函数类型和材质内容进行智能位置计算
                FString FunctionName = Function->GetName();

                // 【统一逻辑】优先检查是否在MaterialAttributes模式下，如果是则统一放在目标节点左侧
                if (MakeMaterialAttributesNode)
                {
                    // MaterialAttributes模式：无论什么函数类型，都放在MakeMaterialAttributes节点左侧齐高
                    PosX = MakeMaterialAttributesNode->MaterialExpressionEditorX - 250;
                    PosY = MakeMaterialAttributesNode->MaterialExpressionEditorY;
                    UE_LOG(LogX_AssetEditor, Log, TEXT("[MaterialAttributes模式] 将函数 %s 放置在MakeMaterialAttributes节点左侧: (%d, %d)"),
                        *FunctionName, PosX, PosY);
                }
                // 普通模式：根据函数类型放在对应属性节点附近
                else if (FunctionName.Contains(TEXT("Fresnel")))
                {
                    // 检查EmissiveExpr是否为简单常量，如果是则忽略
                    bool bHasComplexEmissiveExpr = EmissiveExpr && !IsSimpleConstantExpression(EmissiveExpr);
                    bool bHasComplexBaseColorExpr = BaseColorExpr && !IsSimpleConstantExpression(BaseColorExpr);

                    // 对于菲涅尔函数，优先考虑放在与EmissiveColor相关的表达式左侧
                    if (bHasComplexEmissiveExpr)
                    {
                        PosX = EmissiveExpr->MaterialExpressionEditorX - 250;
                        PosY = EmissiveExpr->MaterialExpressionEditorY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("将菲涅尔函数放置在EmissiveColor表达式左侧: (%d, %d)"), PosX, PosY);
                    }
                    // 如果有BaseColor表达式，也可以考虑放在附近
                    else if (bHasComplexBaseColorExpr)
                    {
                        PosX = BaseColorExpr->MaterialExpressionEditorX - 250;
                        PosY = BaseColorExpr->MaterialExpressionEditorY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("将菲涅尔函数放置在BaseColor表达式左侧: (%d, %d)"), PosX, PosY);
                    }
                    // 否则使用材质表达式的平均位置（排除简单常量）
                    else if (ExprCount > 1)  // 如果只有1个节点且是简单常量，ExprCount=1，应该走下面的空白材质逻辑
                    {
                        PosX = CenterX + 200;
                        PosY = CenterY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("将菲涅尔函数放置在材质表达式中心点右侧: (%d, %d)"), PosX, PosY);
                    }
                    // 如果材质是空白的或只有简单常量，放在材质主节点左侧
                    else
                    {
                        // 获取材质主节点的实际位置（UMaterial的EditorX/EditorY）
                        int32 MaterialNodeX = Material->EditorX;
                        int32 MaterialNodeY = Material->EditorY;
                        PosX = MaterialNodeX - 250;
                        PosY = MaterialNodeY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("空白材质或仅有简单常量：材质主节点位置(%d, %d)，将菲涅尔函数放置在其左侧: (%d, %d)"),
                            MaterialNodeX, MaterialNodeY, PosX, PosY);
                    }
                }
                else if (FunctionName.Contains(TEXT("BaseColor")))
                {
                    // 检查BaseColorExpr是否为简单常量，如果是则忽略
                    bool bHasComplexBaseColorExpr = BaseColorExpr && !IsSimpleConstantExpression(BaseColorExpr);

                    // 对于BaseColor相关函数，放在与BaseColor相关的表达式左侧
                    if (bHasComplexBaseColorExpr)
                    {
                        PosX = BaseColorExpr->MaterialExpressionEditorX - 250;
                        PosY = BaseColorExpr->MaterialExpressionEditorY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("将BaseColor函数放置在BaseColor表达式左侧: (%d, %d)"), PosX, PosY);
                    }
                    else if (ExprCount > 1)
                    {
                        PosX = CenterX + 200;
                        PosY = CenterY;
                    }
                    else
                    {
                        // 空白材质或仅有简单常量：获取材质主节点实际位置，BaseColor引脚在主节点偏上约50px
                        int32 MaterialNodeX = Material->EditorX;
                        int32 MaterialNodeY = Material->EditorY;
                        PosX = MaterialNodeX - 250;
                        PosY = MaterialNodeY - 50;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("空白材质或仅有简单常量：材质主节点位置(%d, %d)，将BaseColor函数放置在其左侧: (%d, %d)"),
                            MaterialNodeX, MaterialNodeY, PosX, PosY);
                    }
                }
                else if (FunctionName.Contains(TEXT("Metallic")))
                {
                    // 检查MetallicExpr是否为简单常量，如果是则忽略
                    bool bHasComplexMetallicExpr = MetallicExpr && !IsSimpleConstantExpression(MetallicExpr);

                    // 对于Metallic相关函数，放在与Metallic相关的表达式左侧
                    if (bHasComplexMetallicExpr)
                    {
                        PosX = MetallicExpr->MaterialExpressionEditorX - 250;
                        PosY = MetallicExpr->MaterialExpressionEditorY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("将Metallic函数放置在Metallic表达式左侧: (%d, %d)"), PosX, PosY);
                    }
                    else if (ExprCount > 1)
                    {
                        PosX = CenterX + 200;
                        PosY = CenterY + 100;
                    }
                    else
                    {
                        // 空白材质或仅有简单常量：获取材质主节点实际位置，Metallic引脚在主节点偏下约50px
                        int32 MaterialNodeX = Material->EditorX;
                        int32 MaterialNodeY = Material->EditorY;
                        PosX = MaterialNodeX - 250;
                        PosY = MaterialNodeY + 50;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("空白材质或仅有简单常量：材质主节点位置(%d, %d)，将Metallic函数放置在其左侧: (%d, %d)"),
                            MaterialNodeX, MaterialNodeY, PosX, PosY);
                    }
                }
                else if (FunctionName.Contains(TEXT("Roughness")))
                {
                    // 检查RoughnessExpr是否为简单常量，如果是则忽略
                    bool bHasComplexRoughnessExpr = RoughnessExpr && !IsSimpleConstantExpression(RoughnessExpr);

                    // 对于Roughness相关函数，放在与Roughness相关的表达式左侧
                    if (bHasComplexRoughnessExpr)
                    {
                        PosX = RoughnessExpr->MaterialExpressionEditorX - 250;
                        PosY = RoughnessExpr->MaterialExpressionEditorY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("将Roughness函数放置在Roughness表达式左侧: (%d, %d)"), PosX, PosY);
                    }
                    else if (ExprCount > 1)
                    {
                        PosX = CenterX + 200;
                        PosY = CenterY + 150;
                    }
                    else
                    {
                        // 空白材质或仅有简单常量：获取材质主节点实际位置，Roughness引脚在主节点偏下约100px
                        int32 MaterialNodeX = Material->EditorX;
                        int32 MaterialNodeY = Material->EditorY;
                        PosX = MaterialNodeX - 250;
                        PosY = MaterialNodeY + 100;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("空白材质或仅有简单常量：材质主节点位置(%d, %d)，将Roughness函数放置在其左侧: (%d, %d)"),
                            MaterialNodeX, MaterialNodeY, PosX, PosY);
                    }
                }
                else
                {
                    // 对于其他类型函数，使用材质表达式中心点或默认位置
                    if (ExprCount > 0)
                    {
                        PosX = CenterX + 200;
                        PosY = CenterY;
                    }
                    else
                    {
                        // 空白材质：获取材质主节点实际位置并放在其左侧
                        int32 MaterialNodeX = Material->EditorX;
                        int32 MaterialNodeY = Material->EditorY;
                        PosX = MaterialNodeX - 250;
                        PosY = MaterialNodeY;
                        UE_LOG(LogX_AssetEditor, Log, TEXT("空白材质：材质主节点位置(%d, %d)，将函数放置在其左侧: (%d, %d)"),
                            MaterialNodeX, MaterialNodeY, PosX, PosY);
                    }
                }
                
                UE_LOG(LogX_AssetEditor, Log, TEXT("智能计算位置: (%d, %d)"), PosX, PosY);
            }
        }
        
        // 重叠检测和避让
        const int32 NodeWidth = 200;
        const int32 NodeHeight = 120;
        const int32 OffsetStep = 50;
        const int32 MaxAttempts = 20;
        
        bool bFoundValidPosition = false;
        int32 AttemptCount = 0;
        int32 CurrentPosX = PosX;
        int32 CurrentPosY = PosY;
        
        while (!bFoundValidPosition && AttemptCount < MaxAttempts)
        {
            bool bOverlaps = false;
            
            TArrayView<const TObjectPtr<UMaterialExpression>> CheckExpressions = Material->GetExpressions();
            for (const TObjectPtr<UMaterialExpression>& ExprPtr : CheckExpressions)
            {
                UMaterialExpression* Expr = ExprPtr.Get();
                if (!Expr) continue;
                
                int32 ExprX = Expr->MaterialExpressionEditorX;
                int32 ExprY = Expr->MaterialExpressionEditorY;
                
                bool bXOverlap = (CurrentPosX < ExprX + NodeWidth) && (CurrentPosX + NodeWidth > ExprX);
                bool bYOverlap = (CurrentPosY < ExprY + NodeHeight) && (CurrentPosY + NodeHeight > ExprY);
                
                if (bXOverlap && bYOverlap)
                {
                    bOverlaps = true;
                    break;
                }
            }
            
            if (!bOverlaps)
            {
                bFoundValidPosition = true;
                PosX = CurrentPosX;
                PosY = CurrentPosY;
                UE_LOG(LogX_AssetEditor, Log, TEXT("找到无重叠位置: (%d, %d)"), PosX, PosY);
            }
            else
            {
                // 优先向上偏移
                CurrentPosY -= OffsetStep;
                AttemptCount++;
                
                // 如果向上偏移5次还有重叠，尝试向下偏移
                if (AttemptCount == 5)
                {
                    CurrentPosY = PosY + OffsetStep * 5;
                }
                // 如果向下也偏移5次还有重叠，向左偏移并重置Y
                else if (AttemptCount % 10 == 0)
                {
                    CurrentPosX -= NodeWidth + 50;
                    CurrentPosY = PosY;
                }
            }
        }
        
        if (!bFoundValidPosition)
        {
            UE_LOG(LogX_AssetEditor, Warning, TEXT("未找到完全无重叠的位置，使用最后尝试的位置: (%d, %d)"), CurrentPosX, CurrentPosY);
            PosX = CurrentPosX;
            PosY = CurrentPosY;
        }
        // 更新位置（官方API已设置初始位置，这里更新为计算后的位置）
        FunctionCall->MaterialExpressionEditorX = PosX;
        FunctionCall->MaterialExpressionEditorY = PosY;
        
        // 更新函数引用（官方API的SetMaterialFunction已处理，这里确保更新）
        FunctionCall->UpdateFromFunctionResource();
        
        UE_LOG(LogX_AssetEditor, Log, TEXT("函数资源已更新"));
    }
    
    return FunctionCall;
}

bool FX_MaterialFunctionOperation::SetupAutoConnections(
    UMaterial* Material, 
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    EConnectionMode ConnectionMode,
    const TSharedPtr<FX_MaterialFunctionParams>& Params)
{
    // 委托给连接器类
    return FX_MaterialFunctionConnector::SetupAutoConnections(Material, FunctionCall, ConnectionMode, Params);
}

UMaterialExpressionAdd* FX_MaterialFunctionOperation::CreateAddConnectionToProperty(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex,
    EMaterialProperty MaterialProperty)
{
    // 委托给连接器类
    return FX_MaterialFunctionConnector::CreateAddConnectionToProperty(Material, FunctionCall, OutputIndex, MaterialProperty);
}

UMaterialExpressionMultiply* FX_MaterialFunctionOperation::CreateMultiplyConnectionToProperty(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    int32 OutputIndex,
    EMaterialProperty MaterialProperty)
{
    // 委托给连接器类
    return FX_MaterialFunctionConnector::CreateMultiplyConnectionToProperty(Material, FunctionCall, OutputIndex, MaterialProperty);
}

bool FX_MaterialFunctionOperation::CheckFunctionHasInputsAndOutputs(UMaterialFunctionInterface* Function)
{
    if (!Function)
    {
        return false;
    }

    // 获取函数输入输出引脚数量
    int32 InputCount = 0;
    int32 OutputCount = 0;
    GetFunctionInputOutputCount(Function, InputCount, OutputCount);
    
    // 返回是否同时具有输入和输出引脚
    return (InputCount > 0 && OutputCount > 0);
}

void FX_MaterialFunctionOperation::GetFunctionInputOutputCount(
    UMaterialFunctionInterface* Function,
    int32& OutInputCount, 
    int32& OutOutputCount)
{
    OutInputCount = 0;
    OutOutputCount = 0;
    
    if (!Function)
    {
        return;
    }
    
    // 创建一个临时的材质函数调用来检查输入输出引脚
    // 这是一种获取函数输入输出信息的有效方法
    UMaterial* TempMaterial = NewObject<UMaterial>();
    if (!TempMaterial)
    {
        return;
    }
    
    UMaterialExpressionMaterialFunctionCall* FunctionCall = NewObject<UMaterialExpressionMaterialFunctionCall>(TempMaterial);
    if (!FunctionCall)
    {
        return;
    }
    
    FunctionCall->MaterialFunction = Function;
    FunctionCall->UpdateFromFunctionResource();
    
    // 获取输入输出引脚数量
    OutInputCount = FunctionCall->FunctionInputs.Num();
    OutOutputCount = FunctionCall->FunctionOutputs.Num();
    
    // 清理临时对象
    FunctionCall->MarkAsGarbage();
    TempMaterial->MarkAsGarbage();
}

#undef LOCTEXT_NAMESPACE