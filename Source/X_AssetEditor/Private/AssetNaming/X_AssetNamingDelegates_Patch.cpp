/*
* 补丁：改进自动规范命名的导入检测机制
*
* 问题：OnAssetPostImport 在部分导入场景下不触发（如拖拽导入）
* 解决：降低对 Factory 时间窗的依赖，添加文件时间戳备用检测
*
* 应用方式：将此文件中的代码复制到 X_AssetNamingDelegates.cpp 对应位置
*/

// ========== 修改 1：降低 Factory 时间窗权重 ==========

// 文件: X_AssetNamingDelegates.cpp
// 函数: OnAssetAdded
// 位置: 第 262-320 行

// 修改前代码（硬性过滤）：
if (TimeSinceLastFactory > FactoryTimeWindow)
{
    UE_LOG(LogX_AssetNamingDelegates, Verbose,
        TEXT("跳过自动重命名：非 Factory 创建流程 (TimeSinceLastFactory: %.3f s, Window: %.1f s) - %s"),
        TimeSinceLastFactory, FactoryTimeWindow, *AssetData.AssetName.ToString());
    return false;
}

// 修改后代码（软性提示 + 备用检测）：
bool bIsLikelyFactoryCreation = (TimeSinceLastFactory <= FactoryTimeWindow);

if (!bIsLikelyFactoryCreation)
{
    UE_LOG(LogX_AssetNamingDelegates, Verbose,
        TEXT("Factory 时间窗未命中，尝试备用检测 (TimeSinceLastFactory: %.3f s, Window: %.1f s) - %s"),
        TimeSinceLastFactory, FactoryTimeWindow, *AssetData.AssetName.ToString());

    // 不再直接跳过，而是继续检查文件时间戳等备用条件
}
else
{
    UE_LOG(LogX_AssetNamingDelegates, Log,
        TEXT("Factory 时间窗命中 (Time: %.3fs)，优先执行重命名: %s"),
        TimeSinceLastFactory, *AssetData.AssetName.ToString());
}

// ========== 修改 2：添加文件时间戳备用检测 ==========

// 文件: X_AssetNamingDelegates.cpp
// 函数: OnAssetAdded
// 位置: 在 Lambda 中的类型检查之后，重命名回调之前

// 在以下代码之后插入：
//     if (AssetClass && !AssetClass->IsChildOf(FactoryClass))
//     {
//         UE_LOG(LogX_AssetNamingDelegates, Log, TEXT("跳过自动重命名：资产类型不匹配..."));
//         return false;
//     }
// }

// 添加备用检测逻辑：
if (!bIsLikelyFactoryCreation)
{
    // ========== 备用通道：文件时间戳检测 ==========
    FString PackagePath = AssetData.PackagePath.ToString();
    FString DiskPath = FPackageName::LongPackageNameToFilename(PackagePath, TEXT(".uasset"));

    // 检查文件是否存在
    if (IPlatformFile::GetPlatformPhysical().FileExists(*DiskPath))
    {
        FDateTime CreationTime = IPlatformFile::GetPlatformPhysical().GetCreationTime(*DiskPath);
        FDateTime ModifiedTime = IPlatformFile::GetPlatformPhysical().GetTimeStamp(*DiskPath);
        FDateTime CurrentTime = FDateTime::Now();
        FTimespan TimeSinceCreation = CurrentTime - CreationTime;

        // 文件时间窗：比 Factory 时间窗稍大，捕获更多场景
        const float FileTimeWindow = FactoryTimeWindow * 1.5f;

        // 检查条件：
        // 1. 文件创建时间在时间窗内
        // 2. 创建时间等于修改时间（新创建的文件，未被后续修改）
        if (TimeSinceCreation.GetTotalSeconds() <= FileTimeWindow && CreationTime == ModifiedTime)
        {
            UE_LOG(LogX_AssetNamingDelegates, Log,
                TEXT("备用通道命中：文件时间戳检测 (创建于 %.1f 秒前) - %s"),
                TimeSinceCreation.GetTotalSeconds(), *AssetData.AssetName.ToString());

            // 确认为导入操作，继续执行重命名
            bIsLikelyFactoryCreation = true;
        }
        else
        {
            UE_LOG(LogX_AssetNamingDelegates, Verbose,
                TEXT("备用通道未命中：文件时间过期或已被修改 (创建于 %.1f 秒前, 创建==修改: %d) - %s"),
                TimeSinceCreation.GetTotalSeconds(), CreationTime == ModifiedTime, *AssetData.AssetName.ToString());
        }
    }
    else
    {
        UE_LOG(LogX_AssetNamingDelegates, Warning,
            TEXT("备用通道：文件不存在，无法检测时间戳 - %s"), *DiskPath);
    }
}

// 在所有检测完成后，统一判断是否继续
if (!bIsLikelyFactoryCreation)
{
    UE_LOG(LogX_AssetNamingDelegates, Verbose,
        TEXT("所有检测通道均未命中，跳过自动重命名: %s"), *AssetData.AssetName.ToString());
    return false;
}

// ========== 修改 3：降低启动延迟 ==========

// 文件: X_AssetEditor/Private/Settings/X_AssetEditorSettings.cpp
// 函数: UX_AssetEditorSettings::UX_AssetEditorSettings()

// 修改前：
StartupActivationDelay = 30.0f;
FactoryCreationTimeWindow = 5.0f;

// 修改后：
StartupActivationDelay = 5.0f;   // 启动后 5 秒开始处理（而非 30 秒）
FactoryCreationTimeWindow = 10.0f; // Factory 时间窗扩大到 10 秒（捕获更多场景）

// ========== 修改 4：添加详细日志便于诊断 ==========

// 文件: X_AssetNamingDelegates.cpp
// 函数: OnAssetAdded

// 在函数开头添加详细日志：
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("========== OnAssetAdded 开始 =========="));
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("资产: %s"), *AssetData.AssetName.ToString());
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("类型: %s"), *AssetData.AssetClassPath.ToString());
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("包路径: %s"), *AssetData.PackagePath.ToString());
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("设置 - bAutoRenameOnCreate: %d"), Settings ? Settings->bAutoRenameOnCreate : false);
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("状态 - bIsActive: %d, bIsAssetRegistryReady: %d"), bIsActive, bIsAssetRegistryReady);
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("Factory - LastFactoryCreationTime: %.3f, TimeWindow: %.1f"),
    LastFactoryCreationTime, FactoryTimeWindow);
UE_LOG(LogX_AssetNamingDelegates, Verbose,
    TEXT("========================================"));

// ========== 测试指南 ==========

/*
修复后测试场景：

1. 拖拽导入测试：
   - 从 Windows 资源管理器拖拽 .fbx 文件到 Content Browser
   - 预期：资产自动重命名为 SM_xxx
   - 观察：查看输出日志中的 "备用通道命中" 消息

2. 批量导入测试：
   - 同时拖入多个文件
   - 预期：所有文件都被正确重命名
   - 观察：确认无日志错误

3. 启动导入测试：
   - 编辑器启动后立即拖入文件
   - 预期：5 秒后可以正常重命名
   - 观察：查看 "延迟激活完成" 日志

4. 复制粘贴测试：
   - 复制现有资产
   - 预期：不会触发自动重命名（避免误操作）
   - 观察：查看 "跳过自动重命名" 日志

日志关键字：
- "Factory 时间窗命中" - 主要通道成功
- "备用通道命中：文件时间戳检测" - 备用通道成功
- "所有检测通道均未命中" - 未触发重命名
- "跳过自动重命名" - 因其他原因跳过
*/

// ========== 配置调优建议 ==========

/*
如果问题仍然存在，可以尝试调整以下参数：

1. 增大 Factory 时间窗（捕获更慢的导入流程）：
   FactoryCreationTimeWindow = 15.0f; // 或 20.0f

2. 增大文件时间窗（文件时间戳备用通道）：
   修改代码中的 FileTimeWindow = FactoryTimeWindow * 1.5f;
   改为 FileTimeWindow = FactoryTimeWindow * 2.0f;

3. 完全禁用 Factory 时间窗（仅使用文件时间戳）：
   注释掉 Factory 时间窗检查，仅保留备用通道

4. 降低启动延迟（立即激活）：
   StartupActivationDelay = 0.0f; // 注意：可能误处理启动时的资产加载
*/
