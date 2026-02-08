/*
* 改进方案：多层级导入检测机制
*
* 问题：当前仅依赖 OnAssetPostImport + Factory 时间窗，部分导入场景无法触发
*
* 解决方案：
* 1. 添加 FAssetRegistry::OnPathAdded 监听 - 检测新文件夹/文件添加
* 2. 添加 FCoreUObjectDelegates::OnAssetLoaded 监听 - 更底层的加载事件
* 3. 改进 Factory 时间窗机制 - 降低对 OnNewAssetCreated 的依赖
* 4. 添加文件系统监听 - 检测 .uasset 文件创建
*
* 来源：
* - Epic Developer Community: UFactory::FactoryCanImport
* - UE 论坛: "Where can I write modifications to the asset import pipeline?"
* - FAssetRegistry 文档: OnAssetAdded, OnPathAdded 委托
*/

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

/**
 * 改进后的资产导入检测策略
 *
 * 核心思路：使用多个独立的检测通道，每个通道都能独立识别导入操作
 * 不再单一依赖 Factory 时间窗，提高可靠性
 */
class FX_AssetNamingDetectionStrategy
{
public:
    /**
     * 检测通道 1: Factory 时间窗（原有机制，保留作为主要通道）
     * 适用场景：通过 Content Browser 导入、新建资产
     * 可靠性：高（在正确场景下）
     */
    static bool DetectViaFactoryTimeWindow(const FAssetData& AssetData, float FactoryTimeWindow);

    /**
     * 检测通道 2: 临时包检测（新增）
     * 原理：导入的资产最初会在 /Temp/ 路径下创建，然后移动到目标路径
     * 适用场景：拖拽导入、批量导入
     * 可靠性：中高
     *
     * 检测方法：
     * 1. 检查资产是否最近创建（通过 AssetRegistry 的扫描时间）
     * 2. 检查资产的包路径是否最近变更过
     * 3. 检查资产的磁盘文件是否最近创建
     */
    static bool DetectViaTemporaryPackage(const FAssetData& AssetData);

    /**
     * 检测通道 3: 文件系统时间戳（新增）
     * 原理：直接检查 .uasset 文件的创建时间
     * 适用场景：所有导入方式
     * 可靠性：高
     *
     * 检测方法：
     * 1. 获取资产的磁盘文件路径
     * 2. 检查文件创建时间是否在时间窗内
     * 3. 检查文件修改时间是否等于创建时间（新创建的文件）
     */
    static bool DetectViaFileTimestamp(const FAssetData& AssetData, float TimeWindow = 10.0f);

    /**
     * 检测通道 4: AssetRegistry 状态（新增）
     * 原理：新导入的资产在 AssetRegistry 中有特定的状态标记
     * 适用场景：所有导入方式
     * 可靠性：中
     *
     * 检测方法：
     * 1. 检查资产的 IsRedirector() 状态
     * 2. 检查资产的 Tag 值（导入的资产有特定标记）
     * 3. 检查资产的 PackageFlags
     */
    static bool DetectViaRegistryState(const FAssetData& AssetData);

    /**
     * 综合检测：多通道 OR 逻辑
     * 只要任一通道返回 true，就认为是导入操作
     *
     * 优先级：Factory 时间窗 > 文件时间戳 > Registry 状态 > 临时包
     * 原因：Factory 时间窗最准确（用户主动操作），其他通道作为补充
     */
    static bool IsImportedAsset(const FAssetData& AssetData, float FactoryTimeWindow = 5.0f);

private:
    /**
     * 辅助函数：获取资产的磁盘文件路径
     */
    static FString GetAssetDiskPath(const FAssetData& AssetData);

    /**
     * 辅助函数：检查文件是否最近创建
     */
    static bool IsFileRecentlyCreated(const FString& FilePath, float TimeWindow);

    /**
     * 辅助函数：检查资产的 Package 是否最近创建
     */
    static bool IsPackageRecentlyCreated(const FAssetData& AssetData, float TimeWindow);
};

// 实现示例（仅供参考，实际使用时需要集成到 X_AssetNamingDelegates.cpp）

/*
bool FX_AssetNamingDetectionStrategy::DetectViaFileTimestamp(const FAssetData& AssetData, float TimeWindow)
{
    // 获取资产的磁盘文件路径
    FString PackagePath = AssetData.PackageName.ToString();
    FString DiskPath = FPackageName::LongPackageNameToFilename(PackagePath, TEXT(".uasset"));

    // 检查文件是否存在
    if (!IPlatformFile::GetPlatformPhysical().FileExists(*DiskPath))
    {
        return false;
    }

    // 获取文件创建时间和修改时间
    FDateTime CreationTime = IPlatformFile::GetPlatformPhysical().GetCreationTime(*DiskPath);
    FDateTime ModifiedTime = IPlatformFile::GetPlatformPhysical().GetTimeStamp(*DiskPath);
    FDateTime CurrentTime = FDateTime::Now();

    // 计算时间差
    FTimespan TimeSinceCreation = CurrentTime - CreationTime;

    // 如果文件创建时间在时间窗内，且创建时间等于修改时间（新文件）
    if (TimeSinceCreation.GetTotalSeconds() <= TimeWindow && CreationTime == ModifiedTime)
    {
        UE_LOG(LogX_AssetNamingDelegates, Log,
            TEXT("检测到导入资产（文件时间戳）: %s (创建于 %.1f 秒前)"),
            *AssetData.AssetName.ToString(), TimeSinceCreation.GetTotalSeconds());
        return true;
    }

    return false;
}
*/
