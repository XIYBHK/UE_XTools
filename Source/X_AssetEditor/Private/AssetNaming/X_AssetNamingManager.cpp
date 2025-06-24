// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetNaming/X_AssetNamingManager.h"
#include "X_AssetEditor.h"
#include "EditorUtilityLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFilemanager.h"

DEFINE_LOG_CATEGORY(LogX_AssetNaming);

TUniquePtr<FX_AssetNamingManager> FX_AssetNamingManager::Instance = nullptr;

FX_AssetNamingManager& FX_AssetNamingManager::Get()
{
    if (!Instance.IsValid())
    {
        Instance = TUniquePtr<FX_AssetNamingManager>(new FX_AssetNamingManager());
    }
    return *Instance;
}

void FX_AssetNamingManager::Initialize()
{
    CreateAssetPrefixMap();
    UE_LOG(LogX_AssetNaming, Log, TEXT("资产命名管理器已初始化，共 %d 个前缀规则"), AssetPrefixes.Num());
}

void FX_AssetNamingManager::CreateAssetPrefixMap()
{
    AssetPrefixes.Empty();
    
    // --- 核心与通用 ---
    AssetPrefixes.Add(TEXT("Blueprint"), TEXT("BP_"));
    AssetPrefixes.Add(TEXT("World"), TEXT("Map_"));
    
    // --- 网格体与几何体 ---
    AssetPrefixes.Add(TEXT("StaticMesh"), TEXT("SM_"));
    AssetPrefixes.Add(TEXT("SkeletalMesh"), TEXT("SK_"));
    AssetPrefixes.Add(TEXT("GeometryCollection"), TEXT("GC_"));
    AssetPrefixes.Add(TEXT("PhysicsAsset"), TEXT("PHYS_"));
    AssetPrefixes.Add(TEXT("PhysicalMaterial"), TEXT("PM_"));
    AssetPrefixes.Add(TEXT("Skeleton"), TEXT("SKEL_"));
    
    // --- 材质与纹理 ---
    AssetPrefixes.Add(TEXT("Material"), TEXT("M_"));
    AssetPrefixes.Add(TEXT("MaterialInstanceConstant"), TEXT("MI_"));
    AssetPrefixes.Add(TEXT("MaterialFunction"), TEXT("MF_"));
    AssetPrefixes.Add(TEXT("MaterialParameterCollection"), TEXT("MPC_"));
    AssetPrefixes.Add(TEXT("Texture2D"), TEXT("T_"));
    AssetPrefixes.Add(TEXT("TextureCube"), TEXT("TC_"));
    AssetPrefixes.Add(TEXT("TextureRenderTarget2D"), TEXT("RT_"));
    
    // --- UI ---
    AssetPrefixes.Add(TEXT("WidgetBlueprint"), TEXT("WBP_"));
    AssetPrefixes.Add(TEXT("Font"), TEXT("Font_"));
    
    // --- 数据与配置 ---
    AssetPrefixes.Add(TEXT("DataTable"), TEXT("DT_"));
    AssetPrefixes.Add(TEXT("CurveFloat"), TEXT("Curve_"));
    AssetPrefixes.Add(TEXT("UserDefinedStruct"), TEXT("S_"));
    AssetPrefixes.Add(TEXT("UserDefinedEnum"), TEXT("E_"));
    AssetPrefixes.Add(TEXT("DataAsset"), TEXT("DA_"));
    
    // --- 音频 ---
    AssetPrefixes.Add(TEXT("SoundCue"), TEXT("SC_"));
    AssetPrefixes.Add(TEXT("SoundWave"), TEXT("S_"));
    
    // --- 效果 ---
    AssetPrefixes.Add(TEXT("ParticleSystem"), TEXT("PS_"));
    AssetPrefixes.Add(TEXT("NiagaraSystem"), TEXT("NS_"));
    
    // --- AI ---
    AssetPrefixes.Add(TEXT("BehaviorTree"), TEXT("BT_"));
    AssetPrefixes.Add(TEXT("BlackboardData"), TEXT("BB_"));
    
    // --- 动画 ---
    AssetPrefixes.Add(TEXT("AnimBlueprint"), TEXT("ABP_"));
    AssetPrefixes.Add(TEXT("AnimSequence"), TEXT("A_"));
    AssetPrefixes.Add(TEXT("AnimMontage"), TEXT("AM_"));
    AssetPrefixes.Add(TEXT("BlendSpace"), TEXT("BS_"));
    
    // --- 蓝图特殊类型 ---
    AssetPrefixes.Add(TEXT("BlueprintFunctionLibrary"), TEXT("BPFL_"));
    AssetPrefixes.Add(TEXT("BlueprintInterface"), TEXT("BPI_"));
    AssetPrefixes.Add(TEXT("EditorUtilityBlueprint"), TEXT("EUBP_"));
}

FString FX_AssetNamingManager::GetSimpleClassName(const FAssetData& AssetData) const
{
    FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();

    // 移除 _C 后缀（如果有的话）
    if (ClassName.EndsWith(TEXT("_C")))
    {
        ClassName.LeftChopInline(2, false);
    }

    // 如果类名为空，使用资产名称作为备选
    if (ClassName.IsEmpty())
    {
        ClassName = AssetData.AssetName.ToString();
    }

    return ClassName;
}

FString FX_AssetNamingManager::GetAssetClassDisplayName(const FAssetData& AssetData) const
{
    return GetSimpleClassName(AssetData);
}

FString FX_AssetNamingManager::GetCorrectPrefix(const FAssetData& AssetData, const FString& SimpleClassName) const
{
    // 尝试直接从类名获取前缀
    const FString* PrefixPtr = AssetPrefixes.Find(SimpleClassName);
    if (PrefixPtr)
    {
        return *PrefixPtr;
    }

    // 尝试从完整类路径中获取类名
    FString FullClassPath = AssetData.AssetClassPath.ToString();
    FString Left, Right;
    if (FullClassPath.Split(TEXT("."), &Left, &Right))
    {
        PrefixPtr = AssetPrefixes.Find(Right);
        if (PrefixPtr)
        {
            return *PrefixPtr;
        }
    }

    UE_LOG(LogX_AssetNaming, Warning, TEXT("无法确定资产 '%s' 的前缀 (类型: %s)"),
        *AssetData.AssetName.ToString(), *SimpleClassName);

    return TEXT("");
}

const TMap<FString, FString>& FX_AssetNamingManager::GetAssetPrefixes() const
{
    return AssetPrefixes;
}

void FX_AssetNamingManager::RenameSelectedAssets()
{
    // 获取选中的资产数据
    TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
    if (SelectedAssets.Num() == 0)
    {
        UE_LOG(LogX_AssetNaming, Warning, TEXT("未选中任何资产，无法执行重命名操作"));
        return;
    }

    // 获取AssetTools模块
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    IAssetTools& AssetTools = AssetToolsModule.Get();

    // 开始资产重命名操作
    FScopedTransaction Transaction(NSLOCTEXT("X_AssetNaming", "RenameAssets", "重命名资产"));

    int32 SuccessCount = 0;
    int32 SkippedCount = 0;
    int32 FailedCount = 0;
    FString OperationDetails;

    UE_LOG(LogX_AssetNaming, Log, TEXT("开始处理%d个资产的命名规范化"), SelectedAssets.Num());

    // 添加进度条
    FScopedSlowTask SlowTask(
        SelectedAssets.Num(),
        FText::Format(NSLOCTEXT("X_AssetNaming", "NormalizingAssetNames", "正在规范化 {0} 个资产的命名..."),
        FText::AsNumber(SelectedAssets.Num()))
    );
    SlowTask.MakeDialog(true);

    for (const FAssetData& AssetData : SelectedAssets)
    {
        SlowTask.EnterProgressFrame(1.0f);

        if (SlowTask.ShouldCancel())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("用户取消了命名规范化操作"));
            break;
        }

        if (!AssetData.IsValid())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("发现无效的资产数据，已跳过"));
            FailedCount++;
            continue;
        }

        FString CurrentName = AssetData.AssetName.ToString();
        FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());

        if (PackagePath.IsEmpty())
        {
            UE_LOG(LogX_AssetNaming, Warning, TEXT("资产'%s'的包路径无效"), *CurrentName);
            FailedCount++;
            continue;
        }

        FString SimpleClassName = GetSimpleClassName(AssetData);
        FString CorrectPrefix = GetCorrectPrefix(AssetData, SimpleClassName);

        if (CorrectPrefix.IsEmpty())
        {
            FailedCount++;
            continue;
        }

        // 检查当前名称是否已经符合规范
        if (CurrentName.StartsWith(CorrectPrefix))
        {
            SkippedCount++;
            continue;
        }

        // 构建新名称
        FString BaseName = CurrentName;

        // 移除已有的不正确前缀
        for (const auto& Pair : AssetPrefixes)
        {
            const FString& ExistingPrefix = Pair.Value;
            if (!ExistingPrefix.IsEmpty() && CurrentName.StartsWith(ExistingPrefix))
            {
                BaseName = CurrentName.RightChop(ExistingPrefix.Len());
                break;
            }
        }

        FString NewName = CorrectPrefix + BaseName;
        FString FinalNewName = NewName;
        int32 SuffixCounter = 1;

        // 检查是否存在同名资产
        while (FPackageName::DoesPackageExist(FString::Printf(TEXT("%s/%s"), *PackagePath, *FinalNewName)))
        {
            FinalNewName = FString::Printf(TEXT("%s_%d"), *NewName, SuffixCounter++);
        }

        if (FinalNewName != CurrentName)
        {
            // 执行重命名操作
            UObject* AssetObject = AssetData.GetAsset();
            if (AssetObject)
            {
                TArray<FAssetRenameData> AssetsToRename;
                AssetsToRename.Add(FAssetRenameData(AssetObject, PackagePath, FinalNewName));

                if (AssetTools.RenameAssets(AssetsToRename))
                {
                    SuccessCount++;
                    UE_LOG(LogX_AssetNaming, Log, TEXT("重命名成功: %s -> %s"), *CurrentName, *FinalNewName);
                }
                else
                {
                    FailedCount++;
                    UE_LOG(LogX_AssetNaming, Error, TEXT("重命名失败: %s"), *CurrentName);
                }
            }
            else
            {
                FailedCount++;
                UE_LOG(LogX_AssetNaming, Error, TEXT("无法加载资产: %s"), *CurrentName);
            }
        }
        else
        {
            SkippedCount++;
        }
    }

    // 显示操作结果
    ShowRenameResult(SuccessCount, SkippedCount, FailedCount, OperationDetails);
}

void FX_AssetNamingManager::ShowRenameResult(int32 SuccessCount, int32 SkippedCount, int32 FailedCount, const FString& OperationDetails) const
{
    // 构建详细的操作结果信息
    static FString LastOperationDetails;
    LastOperationDetails.Empty();
    LastOperationDetails.Append(FString::Printf(TEXT("资产命名规范化操作详情 (%s)\n\n"), *FDateTime::Now().ToString()));
    LastOperationDetails.Append(TEXT("==================== 命名规范化完成 ====================\n"));
    LastOperationDetails.Append(TEXT("处理结果统计:\n"));
    LastOperationDetails.Append(FString::Printf(TEXT("- 总计资产: %d\n"), SuccessCount + SkippedCount + FailedCount));
    LastOperationDetails.Append(FString::Printf(TEXT("- 成功重命名: %d\n"), SuccessCount));
    LastOperationDetails.Append(FString::Printf(TEXT("- 已符合规范: %d\n"), SkippedCount));
    LastOperationDetails.Append(FString::Printf(TEXT("- 处理失败: %d\n"), FailedCount));
    LastOperationDetails.Append(TEXT("====================================================\n"));

    // 显示可点击的通知
    FNotificationInfo Info(FText::Format(
        NSLOCTEXT("X_AssetNaming", "AssetRenameNotification", "资产命名规范化完成\n总计: {0} | 成功: {1} | 已符合: {2} | 失败: {3}\n点击查看详情"),
        FText::AsNumber(SuccessCount + SkippedCount + FailedCount),
        FText::AsNumber(SuccessCount),
        FText::AsNumber(SkippedCount),
        FText::AsNumber(FailedCount)
    ));

    Info.bUseLargeFont = false;
    Info.bUseSuccessFailIcons = false;
    Info.bUseThrobber = false;
    Info.FadeOutDuration = 1.0f;
    Info.ExpireDuration = FailedCount > 0 ? 8.0f : 5.0f;
    Info.bFireAndForget = true;
    Info.bAllowThrottleWhenFrameRateIsLow = true;
    Info.Image = nullptr;

    // 添加点击查看详情功能
    Info.Hyperlink = FSimpleDelegate::CreateLambda([=]()
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            FText::FromString(LastOperationDetails),
            NSLOCTEXT("X_AssetNaming", "ViewDetailsHyperlink", "查看详情"));
    });
    Info.HyperlinkText = NSLOCTEXT("X_AssetNaming", "ViewDetailsHyperlink", "查看详情");

    // 显示通知
    TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

    if (NotificationItem.IsValid())
    {
        if (FailedCount == 0)
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
        }
        else
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
        }
    }

    // 如果失败数量较多，自动显示详细信息
    int32 TotalAssets = SuccessCount + SkippedCount + FailedCount;
    if (FailedCount > 0 && FailedCount > TotalAssets / 3)
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            FText::FromString(LastOperationDetails),
            NSLOCTEXT("X_AssetNaming", "AssetRenameDetails", "资产命名规范化详情"));
    }

    UE_LOG(LogX_AssetNaming, Log, TEXT("资产重命名操作完成: 成功 %d，跳过 %d，失败 %d"),
        SuccessCount, SkippedCount, FailedCount);
}
