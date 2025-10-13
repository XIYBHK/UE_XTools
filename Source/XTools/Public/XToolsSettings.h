// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "XToolsSettings.generated.h"

/**
 * XTools 插件设置
 * 
 * 在编辑器中通过 项目设置 -> 插件 -> XTools 访问
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="XTools"))
class XTOOLS_API UXToolsSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UXToolsSettings();

    //~ Begin UDeveloperSettings Interface
    virtual FName GetCategoryName() const override;
#if WITH_EDITOR
    virtual FText GetSectionText() const override;
#endif
    //~ End UDeveloperSettings Interface

public:
    // ========== 子系统模块开关 ==========

    /**
     * 启用对象池子系统
     * 
     * 功能：提供高性能的Actor对象池，减少频繁创建/销毁开销
     * 性能影响：启用后会在BeginPlay时分帧预热对象池
     * 建议：仅在需要频繁生成/销毁Actor的项目中启用（如射击游戏、弹幕游戏）
     */
    UPROPERTY(Config, EditAnywhere, Category = "子系统", meta = (
        DisplayName = "启用对象池子系统",
        ToolTip = "启用后提供Actor对象池功能，适合需要频繁生成/销毁Actor的项目\n默认关闭，按需启用"))
    bool bEnableObjectPoolSubsystem = false;

    /**
     * 启用增强代码流子系统 (EnhancedCodeFlow)
     * 
     * 功能：提供高级异步执行、协程、延迟任务等代码流控制
     * 性能影响：内存占用极小（<10KB），提供强大的异步编程能力
     * 建议：默认启用，该子系统非常轻量且功能强大
     */
    UPROPERTY(Config, EditAnywhere, Category = "子系统", meta = (
        DisplayName = "启用增强代码流子系统",
        ToolTip = "提供异步执行、协程等高级代码流功能\n默认启用（轻量级子系统）"))
    bool bEnableEnhancedCodeFlowSubsystem = true;

    // ========== 性能优化选项 ==========

    /**
     * 对象池：每帧最大预热数量
     * 
     * 数值越大，预热越快，但可能造成卡顿
     * 推荐值：10-20（流畅） | 30-50（平衡） | 100+（快速）
     */
    UPROPERTY(Config, EditAnywhere, Category = "性能优化|对象池", meta = (
        DisplayName = "对象池每帧最大预热数量",
        ToolTip = "控制预热速度和流畅度的平衡\n• 10 = 流畅优先（默认）\n• 30 = 平衡模式\n• 50 = 快速预热",
        ClampMin = "1", ClampMax = "200",
        EditCondition = "bEnableObjectPoolSubsystem"))
    int32 ObjectPoolMaxPrewarmPerFrame = 10;

    /**
     * 对象池：默认池初始大小
     * 
     * 注册池时的默认初始大小
     */
    UPROPERTY(Config, EditAnywhere, Category = "性能优化|对象池", meta = (
        DisplayName = "对象池默认初始大小",
        ToolTip = "注册新池时的默认初始大小",
        ClampMin = "0", ClampMax = "500",
        EditCondition = "bEnableObjectPoolSubsystem"))
    int32 ObjectPoolDefaultInitialSize = 10;

    /**
     * 对象池：默认池最大大小
     * 
     * 池的默认最大限制，0表示无限制
     */
    UPROPERTY(Config, EditAnywhere, Category = "性能优化|对象池", meta = (
        DisplayName = "对象池默认最大大小",
        ToolTip = "池的默认最大限制（0=无限制）",
        ClampMin = "0", ClampMax = "2000",
        EditCondition = "bEnableObjectPoolSubsystem"))
    int32 ObjectPoolDefaultMaxSize = 100;

    // ========== 蓝图函数库工具 ==========

    /**
     * 启用蓝图函数库清理工具
     * 
     * 提供清理蓝图函数库中多余World Context参数的功能
     * 使用方法：在蓝图中调用 XBlueprintLibraryCleanupTool 的静态函数
     */
    UPROPERTY(Config, EditAnywhere, Category = "蓝图函数库工具", meta = (
        DisplayName = "启用蓝图函数库清理工具",
        ToolTip = "启用后可以清理蓝图函数库中复制时自动添加的多余World Context参数\n使用方法：在蓝图中调用 XBlueprintLibraryCleanupTool 的静态函数"))
    bool bEnableBlueprintLibraryCleanup = true;

    // ========== 调试选项 ==========

    /**
     * 启用详细日志
     * 
     * 启用后会输出各模块的详细运行信息
     */
    UPROPERTY(Config, EditAnywhere, Category = "调试", meta = (
        DisplayName = "启用详细日志",
        ToolTip = "启用后输出详细的运行日志，用于调试"))
    bool bEnableVerboseLogging = false;

    /**
     * 对象池：启用统计信息
     * 
     * 启用后会收集对象池的使用统计
     */
    UPROPERTY(Config, EditAnywhere, Category = "调试|对象池", meta = (
        DisplayName = "对象池启用统计",
        ToolTip = "启用后收集对象池统计信息（轻微性能开销）",
        EditCondition = "bEnableObjectPoolSubsystem"))
    bool bEnableObjectPoolStats = true;

public:
    /** 获取全局设置实例 */
    static const UXToolsSettings* Get();

    /** 获取全局设置实例（可修改） */
    static UXToolsSettings* GetMutable();
};

