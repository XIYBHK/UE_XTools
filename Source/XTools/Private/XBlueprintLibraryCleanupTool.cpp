// Copyright Epic Games, Inc. All Rights Reserved.

#include "XBlueprintLibraryCleanupTool.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#endif

#define LOCTEXT_NAMESPACE "XBlueprintLibraryCleanupTool"

#if WITH_EDITOR

UBlueprint* UXBlueprintLibraryCleanupTool::GetBlueprintFromAssetData(const FAssetData& AssetData)
{
    // 🔍 调试：优先使用内存中的版本，避免重新加载覆盖已修改的蓝图
    
    // 1. 首先尝试通过对象路径在内存中查找
    UObject* ExistingAsset = FindObject<UBlueprint>(nullptr, *AssetData.GetObjectPathString());
    if (ExistingAsset)
    {
        UE_LOG(LogTemp, Log, TEXT("   从内存中找到蓝图: %s"), *AssetData.AssetName.ToString());
        return Cast<UBlueprint>(ExistingAsset);
    }
    
    // 2. 尝试FastGetAsset（不强制加载）
    if (UObject* FastAsset = AssetData.FastGetAsset(false))
    {
        UE_LOG(LogTemp, Log, TEXT("   通过FastGetAsset获取: %s"), *AssetData.AssetName.ToString());
        return Cast<UBlueprint>(FastAsset);
    }
    
    // 3. 最后才从磁盘加载（可能覆盖内存中的修改）
    UE_LOG(LogTemp, Warning, TEXT("   从磁盘加载蓝图: %s (可能覆盖内存修改)"), *AssetData.AssetName.ToString());
    return Cast<UBlueprint>(AssetData.GetAsset());
}

bool UXBlueprintLibraryCleanupTool::IsBlueprintFunctionLibrary(UBlueprint* Blueprint)
{
    if (!Blueprint || !Blueprint->ParentClass)
    {
        return false;
    }
    
    // 检查是否是蓝图函数库
    bool bIsBlueprintLibrary = Blueprint->ParentClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass());
    
    if (!bIsBlueprintLibrary)
    {
        return false;
    }
    
    // 确保这是用户创建的蓝图函数库，而不是引擎内置的
    FString BlueprintPath = Blueprint->GetPathName();
    
    // 排除引擎内置路径
    if (BlueprintPath.StartsWith(TEXT("/Engine/")) ||
        BlueprintPath.StartsWith(TEXT("/Script/Engine")) ||
        BlueprintPath.StartsWith(TEXT("/Script/CoreUObject")) ||
        BlueprintPath.StartsWith(TEXT("/Script/UMG")) ||
        BlueprintPath.StartsWith(TEXT("/Script/")) ||
        BlueprintPath.Contains(TEXT("Engine/Content")) ||
        BlueprintPath.Contains(TEXT("EngineContent")))
    {
        return false;
    }
    
    // 只处理项目内容和插件内容
    FString ProjectPath = FString(TEXT("/")) + FApp::GetProjectName();
    return BlueprintPath.StartsWith(TEXT("/Game/")) || 
           BlueprintPath.Contains(TEXT("/Plugins/")) ||
           BlueprintPath.StartsWith(ProjectPath);
}

TArray<UBlueprint*> UXBlueprintLibraryCleanupTool::GetAllBlueprintFunctionLibraries()
{
    TArray<UBlueprint*> BlueprintLibraries;
    
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    // 🚀 高性能搜索算法
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    
    // 只搜索用户项目和插件目录，排除引擎目录
    Filter.PackagePaths.Add(TEXT("/Game"));
    Filter.PackagePaths.Add(TEXT("/Plugins"));
    Filter.bRecursivePaths = true;
    
    TArray<FAssetData> AssetDataArray;
    AssetRegistry.GetAssets(Filter, AssetDataArray);
    
    UE_LOG(LogTemp, Warning, TEXT("找到 %d 个蓝图资产"), AssetDataArray.Num());
    
    if (AssetDataArray.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("没有找到任何蓝图资产！可能的原因："));
        UE_LOG(LogTemp, Error, TEXT("   1. 路径过滤太严格 - 蓝图可能不在 /Game 或 /Plugins 路径"));
        UE_LOG(LogTemp, Error, TEXT("   2. 资产注册表未更新 - 尝试重新扫描项目"));
        UE_LOG(LogTemp, Error, TEXT("   3. 使用了错误的搜索参数"));
        UE_LOG(LogTemp, Warning, TEXT("建议：检查蓝图函数库是否确实位于 Content 文件夹中"));
    }
    
    // 使用元数据检查，避免不必要的蓝图加载
    int32 TotalAssets = AssetDataArray.Num();
    int32 MetadataChecked = 0;
    int32 MetadataFound = 0;
    int32 AssetsLoaded = 0;
    
    // 收集所有符合条件的AssetData，批量加载
    TArray<FAssetData> FunctionLibraryAssets;
    
    for (const FAssetData& AssetData : AssetDataArray)
    {
        MetadataChecked++;
        
        // 方法1：尝试通过ParentClassPath元数据检查（UE5标准方式）
        FString ParentClassPath;
        bool bIsFunctionLibrary = false;
        
        // 尝试多种可能的标签名称
        if (AssetData.GetTagValue(TEXT("ParentClassPath"), ParentClassPath) ||
            AssetData.GetTagValue(TEXT("ParentClass"), ParentClassPath))
        {
            MetadataFound++;
            // 检查父类路径是否包含BlueprintFunctionLibrary
            bIsFunctionLibrary = ParentClassPath.Contains(TEXT("BlueprintFunctionLibrary"));
        }
        
        if (bIsFunctionLibrary)
        {
            // 直接从AssetData获取路径，避免立即加载蓝图
            FString BlueprintPath = AssetData.GetObjectPathString();
            
            // 确保是用户自定义的（不是引擎内置的）
            if (BlueprintPath.StartsWith(TEXT("/Game/")) || 
                BlueprintPath.Contains(TEXT("/Plugins/")))
            {
                FunctionLibraryAssets.Add(AssetData);
            }
        }
    }
    
    // 智能加载策略
    UE_LOG(LogTemp, Warning, TEXT("开始加载 %d 个蓝图函数库..."), FunctionLibraryAssets.Num());
    double LoadStartTime = FPlatformTime::Seconds();
    
    // 尝试从内存中获取已加载的蓝图
    int32 FromMemory = 0;
    int32 FromDisk = 0;
    
    for (const FAssetData& AssetData : FunctionLibraryAssets)
    {
        AssetsLoaded++;
        
        // 首先检查蓝图是否已经在内存中
        if (UBlueprint* ExistingBlueprint = Cast<UBlueprint>(AssetData.FastGetAsset(false)))
        {
            BlueprintLibraries.Add(ExistingBlueprint);
            FromMemory++;
        }
        else
        {
            // 只有在内存中不存在时才从磁盘加载
            if (UBlueprint* Blueprint = GetBlueprintFromAssetData(AssetData))
            {
                BlueprintLibraries.Add(Blueprint);
                FromDisk++;
            }
        }
    }
    
    double LoadEndTime = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Warning, TEXT("加载完成，耗时: %.3f 秒"), LoadEndTime - LoadStartTime);
    UE_LOG(LogTemp, Warning, TEXT("   从内存获取: %d 个"), FromMemory);
    UE_LOG(LogTemp, Warning, TEXT("   从磁盘加载: %d 个"), FromDisk);
    
    return BlueprintLibraries;
}

bool UXBlueprintLibraryCleanupTool::IsWorldContextPin(const UEdGraphPin* Pin)
{
    if (!Pin)
    {
        return false;
    }
    
    // 检查引脚方向 - World Context参数可能是输入或输出引脚
    // 在函数入口节点中，通常是输出引脚
    if (Pin->Direction != EGPD_Input && Pin->Direction != EGPD_Output)
    {
        return false;
    }
    
    // 🛡️ 安全检查：跳过所有隐藏的引脚
    // 隐藏的引脚通常是系统自动生成的，不会造成用户可见的问题
    if (Pin->bHidden)
    {
        return false; // 跳过所有隐藏引脚，它们不需要清理
    }
    
    FString PinName = Pin->PinName.ToString().ToLower();
    bool bIsWorldContextName = PinName.Contains(TEXT("worldcontext")) || 
                               PinName.Contains(TEXT("world context")) ||
                               PinName == TEXT("worldcontextobject") ||
                               PinName == TEXT("__worldcontext");  // 添加带下划线前缀的检测
    
    // 只有名称匹配且未连接的引脚才被认为是需要清理的World Context参数
    if (bIsWorldContextName)
    {
        // 检查引脚是否有连接
        bool bHasConnections = Pin->LinkedTo.Num() > 0;
        return !bHasConnections; // 只返回未连接的World Context引脚
    }
    
    return false;
}

TArray<UXBlueprintLibraryCleanupTool::FWorldContextScanResult> UXBlueprintLibraryCleanupTool::ScanWorldContextParams(const TArray<UBlueprint*>& Blueprints)
{
    TArray<FWorldContextScanResult> Results;
    
    // 性能监控
    double StartTime = FPlatformTime::Seconds();
    int32 TotalGraphs = 0;
    int32 TotalNodes = 0;
    int32 TotalPins = 0;
    int32 FunctionEntryNodes = 0;
    
    for (UBlueprint* Blueprint : Blueprints)
    {
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);
        
        for (UEdGraph* Graph : AllGraphs)
        {
            TotalGraphs++;
            
            // 🚀 优化：预先过滤节点类型，避免不必要的Cast
            TArray<UK2Node_FunctionEntry*> EntryNodes;
            EntryNodes.Reserve(Graph->Nodes.Num() / 10); // 预估容量
            
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                TotalNodes++;
                if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
                {
                    EntryNodes.Add(EntryNode);
                    FunctionEntryNodes++;
                }
            }
            
            // 🚀 优化：只处理函数入口节点
            for (UK2Node_FunctionEntry* EntryNode : EntryNodes)
            {
                // 确保这是用户定义的函数，而不是系统生成的
                FString FunctionName = Graph->GetFName().ToString();
                
                // 跳过系统函数和事件
                if (FunctionName.StartsWith(TEXT("ExecuteUbergraph")) ||
                    FunctionName.StartsWith(TEXT("ReceiveBegin")) ||
                    FunctionName.StartsWith(TEXT("ReceiveEnd")) ||
                    FunctionName.StartsWith(TEXT("ReceiveTick")) ||
                    FunctionName.Contains(TEXT("Event_")) ||
                    FunctionName.Contains(TEXT("__")) ||
                    FunctionName == TEXT("UserConstructionScript"))
                {
                    continue;
                }
                
                // 🚀 优化：预先过滤引脚，只检查输出引脚且名称可能包含WorldContext
                for (UEdGraphPin* Pin : EntryNode->Pins)
                {
                    TotalPins++;
                    
                    // 快速预过滤：只检查输出引脚且名称包含关键词
                    if (Pin->Direction == EGPD_Output)
                    {
                        if (IsWorldContextPin(Pin))
                        {
                            FWorldContextScanResult Result;
                            Result.Blueprint = Blueprint;
                            Result.FunctionName = FunctionName;
                            Result.PinName = Pin->PinName.ToString();
                            Result.Node = EntryNode;
                            Result.bIsCallNode = false;
                            Results.Add(Result);
                        }
                    }
                }
            }
        }
    }
    
    // 性能统计
    double EndTime = FPlatformTime::Seconds();
    double ElapsedTime = EndTime - StartTime;
    
    UE_LOG(LogTemp, Warning, TEXT("扫描性能统计:"));
    UE_LOG(LogTemp, Warning, TEXT("   扫描时间: %.3f 秒"), ElapsedTime);
    UE_LOG(LogTemp, Warning, TEXT("   处理蓝图: %d"), Blueprints.Num());
    UE_LOG(LogTemp, Warning, TEXT("   处理图形: %d"), TotalGraphs);
    UE_LOG(LogTemp, Warning, TEXT("   检查节点: %d"), TotalNodes);
    UE_LOG(LogTemp, Warning, TEXT("   函数入口: %d"), FunctionEntryNodes);
    UE_LOG(LogTemp, Warning, TEXT("   检查引脚: %d"), TotalPins);
    UE_LOG(LogTemp, Warning, TEXT("   找到结果: %d"), Results.Num());
    
    return Results;
}

int32 UXBlueprintLibraryCleanupTool::PreviewCleanupWorldContextParams(bool bLogToConsole)
{
    if (bLogToConsole)
    {
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
        UE_LOG(LogTemp, Warning, TEXT("[XTools] 开始扫描蓝图函数库中的World Context参数..."));
        UE_LOG(LogTemp, Warning, TEXT("安全限制：只处理用户自定义蓝图函数库"));
        UE_LOG(LogTemp, Warning, TEXT("注意：只会处理【未连接】的World Context参数"));
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
    }
    
    // 获取所有蓝图函数库
    TArray<UBlueprint*> BlueprintLibraries = GetAllBlueprintFunctionLibraries();
    
    // 注意：不再需要强制刷新，因为我们已经排除了隐藏的系统引脚
    
    if (bLogToConsole)
    {
        UE_LOG(LogTemp, Warning, TEXT("找到 %d 个用户自定义蓝图函数库"), BlueprintLibraries.Num());
        
        // 显示找到的蓝图函数库列表
        for (UBlueprint* BP : BlueprintLibraries)
        {
            if (BP)
            {
                UE_LOG(LogTemp, Warning, TEXT("  %s"), *BP->GetName());
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("注意：已自动排除UE引擎内置的蓝图函数库"));
    }
    
    // 扫描World Context参数
    TArray<FWorldContextScanResult> ScanResults = ScanWorldContextParams(BlueprintLibraries);
    
    if (bLogToConsole)
    {
        if (ScanResults.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("未发现需要清理的【未连接】World Context参数"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("发现 %d 个需要清理的【未连接】World Context参数:"), ScanResults.Num());
            UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
            
            for (const FWorldContextScanResult& Result : ScanResults)
            {
                UE_LOG(LogTemp, Warning, TEXT("蓝图: %s"), *Result.Blueprint->GetName());
                UE_LOG(LogTemp, Warning, TEXT("   函数: %s"), *Result.FunctionName);
                UE_LOG(LogTemp, Warning, TEXT("   参数: %s (未连接)"), *Result.PinName);
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
        UE_LOG(LogTemp, Warning, TEXT("[XTools] 扫描完成！如需执行清理，请调用ExecuteCleanupWorldContextParams"));
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
    }
    
    return ScanResults.Num();
}

int32 UXBlueprintLibraryCleanupTool::ExecuteCleanupWorldContextParams(bool bLogToConsole)
{
    if (bLogToConsole)
    {
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
        UE_LOG(LogTemp, Warning, TEXT("[XTools] 开始执行World Context参数清理..."));
        UE_LOG(LogTemp, Warning, TEXT("注意：只会清理【未连接】的World Context参数"));
        UE_LOG(LogTemp, Warning, TEXT("警告：这将修改蓝图资产，请确保已备份！"));
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
    }
    
    // 获取所有蓝图函数库
    TArray<UBlueprint*> BlueprintLibraries = GetAllBlueprintFunctionLibraries();
    
    // 扫描World Context参数
    TArray<FWorldContextScanResult> ScanResults = ScanWorldContextParams(BlueprintLibraries);
    
    if (ScanResults.Num() == 0)
    {
        if (bLogToConsole)
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ 未发现需要清理的【未连接】World Context参数"));
        }
        return 0;
    }
    
    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    
    // 按蓝图分组处理
    TMap<UBlueprint*, TArray<FWorldContextScanResult>> BlueprintGroups;
    for (const FWorldContextScanResult& Result : ScanResults)
    {
        BlueprintGroups.FindOrAdd(Result.Blueprint).Add(Result);
    }
    
    for (auto& Pair : BlueprintGroups)
    {
        UBlueprint* Blueprint = Pair.Key;
        TArray<FWorldContextScanResult>& Results = Pair.Value;
        
        if (bLogToConsole)
        {
            UE_LOG(LogTemp, Warning, TEXT("处理蓝图: %s"), *Blueprint->GetName());
        }
        
        bool bBlueprintModified = false;
        
        for (const FWorldContextScanResult& Result : Results)
        {
            // 尝试移除参数
            if (Result.Node && !Result.bIsCallNode) // 只处理函数入口节点
            {
                UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Result.Node);
                if (EntryNode)
                {
                    // 查找对应的引脚
                    UEdGraphPin* PinToRemove = nullptr;
                    for (UEdGraphPin* Pin : EntryNode->Pins)
                    {
                        if (Pin->PinName.ToString() == Result.PinName && IsWorldContextPin(Pin))
                        {
                            PinToRemove = Pin;
                            break;
                        }
                    }
                    
                    if (PinToRemove)
                    {
                        // 使用更安全的方式移除引脚
                        // 1. 先断开所有连接（虽然我们已经检查了未连接，但为了安全）
                        PinToRemove->BreakAllPinLinks();
                        
                        // 2. 尝试通过用户定义引脚信息移除引脚
                        bool bRemoveSuccess = false;
                        
                        // 查找对应的用户定义引脚信息
                        TSharedPtr<FUserPinInfo> UserPinToRemove = nullptr;
                        for (TSharedPtr<FUserPinInfo> UserPin : EntryNode->UserDefinedPins)
                        {
                            if (UserPin.IsValid() && UserPin->PinName == PinToRemove->PinName)
                            {
                                UserPinToRemove = UserPin;
                                break;
                            }
                        }
                        
                        if (UserPinToRemove.IsValid())
                        {
                            try 
                            {
                                // 移除用户定义引脚
                                EntryNode->RemoveUserDefinedPin(UserPinToRemove);
                                bRemoveSuccess = true;
                                
                                // 移除成功，记录日志在下面统一处理
                            }
                            catch (...)
                            {
                                if (bLogToConsole)
                                {
                                    UE_LOG(LogTemp, Error, TEXT("   ❌ 移除用户定义引脚时发生异常: %s::%s"), 
                                           *Result.FunctionName, *Result.PinName);
                                }
                            }
                        }
                        else
                        {
                            // 如果不是用户定义引脚，尝试普通移除
                            try
                            {
                                EntryNode->RemovePin(PinToRemove);
                                bRemoveSuccess = true;
                                
                                if (bLogToConsole)
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("   🔧 通过普通方式移除: %s::%s"), 
                                           *Result.FunctionName, *Result.PinName);
                                }
                            }
                            catch (...)
                            {
                                if (bLogToConsole)
                                {
                                    UE_LOG(LogTemp, Error, TEXT("   ❌ 移除引脚时发生异常: %s::%s"), 
                                           *Result.FunctionName, *Result.PinName);
                                }
                            }
                        }
                        
                        // 3. 重构节点以更新界面
                        if (bRemoveSuccess)
                        {
                            EntryNode->ReconstructNode();
                        }
                        
                        if (bRemoveSuccess)
                        {
                            bBlueprintModified = true;
                            SuccessCount++;
                            
                            if (bLogToConsole)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("   已移除参数: %s::%s"), 
                                       *Result.FunctionName, *Result.PinName);
                            }
                        }
                        else
                        {
                            FailureCount++;
                        }
                    }
                    else
                    {
                        FailureCount++;
                        if (bLogToConsole)
                        {
                            UE_LOG(LogTemp, Error, TEXT("   ❌ 未找到参数: %s::%s"), 
                                   *Result.FunctionName, *Result.PinName);
                        }
                    }
                }
                else
                {
                    FailureCount++;
                }
            }
        }
        
        // 如果蓝图被修改，重新编译并保存
        if (bBlueprintModified)
        {
            // 更安全的编译方式
            try
            {
                // 1. 先刷新节点
                FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
                
                // 2. 重新编译蓝图
                FKismetEditorUtilities::CompileBlueprint(Blueprint);
                
                // 3. 检查编译结果
                if (Blueprint->Status == BS_Error)
                {
                    if (bLogToConsole)
                    {
                        UE_LOG(LogTemp, Error, TEXT("   ❌ 蓝图编译失败: %s"), *Blueprint->GetName());
                    }
                }
                else
                {
                    // 4. 标记为已修改
                    Blueprint->MarkPackageDirty();
                    
                    if (bLogToConsole)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("   已重新编译蓝图"));
                    }
                }
            }
            catch (...)
            {
                if (bLogToConsole)
                {
                    UE_LOG(LogTemp, Error, TEXT("   ⚠️  蓝图编译过程中发生异常: %s"), *Blueprint->GetName());
                }
            }
        }
    }
    
    if (bLogToConsole)
    {
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
        UE_LOG(LogTemp, Warning, TEXT("[XTools] 清理完成！"));
        UE_LOG(LogTemp, Warning, TEXT("成功清理: %d 个参数"), SuccessCount);
        if (FailureCount > 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("清理失败: %d 个参数"), FailureCount);
        }
        UE_LOG(LogTemp, Warning, TEXT("建议：全量编译项目以确保所有调用点正确更新"));
        UE_LOG(LogTemp, Warning, TEXT("========================================"));
    }
    
    return SuccessCount;
}

#else // !WITH_EDITOR

int32 UXBlueprintLibraryCleanupTool::PreviewCleanupWorldContextParams(bool bLogToConsole)
{
    UE_LOG(LogTemp, Warning, TEXT("[XTools] 蓝图清理工具仅在编辑器模式下可用"));
    return 0;
}

int32 UXBlueprintLibraryCleanupTool::ExecuteCleanupWorldContextParams(bool bLogToConsole)
{
    UE_LOG(LogTemp, Warning, TEXT("[XTools] 蓝图清理工具仅在编辑器模式下可用"));
    return 0;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
