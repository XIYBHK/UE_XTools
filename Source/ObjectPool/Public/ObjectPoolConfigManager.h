// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DeveloperSettings.h"

// ✅ 对象池模块依赖
#include "ObjectPoolTypes.h"

#include "ObjectPoolConfigManager.generated.h"

// ✅ 前向声明
class UObjectPoolSubsystem;

/**
 * 对象池配置模板
 * 预定义的配置组合，便于快速应用
 */
USTRUCT(BlueprintType, meta = (
    DisplayName = "对象池配置模板",
    ToolTip = "预定义的对象池配置组合，包含常用的配置参数组合"))
struct OBJECTPOOL_API FObjectPoolConfigTemplate
{
    GENERATED_BODY()

    /** 模板名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "模板信息")
    FString TemplateName;

    /** 模板描述 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "模板信息")
    FString Description;

    /** 基础池配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    FObjectPoolConfig PoolConfig;

    /** 预分配配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    FObjectPoolPreallocationConfig PreallocationConfig;

    /** 回退配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    FObjectPoolFallbackConfig FallbackConfig;

    /** 生命周期配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    FObjectPoolLifecycleConfig LifecycleConfig;

    /** 重置配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "配置")
    FActorResetConfig ResetConfig;

    FObjectPoolConfigTemplate()
    {
        TemplateName = TEXT("默认模板");
        Description = TEXT("标准的对象池配置");
    }
};

/**
 * 对象池开发者设置
 * 利用UE5内置的UDeveloperSettings系统实现配置持久化
 */
UCLASS(Config = ObjectPool, DefaultConfig, meta = (
    DisplayName = "对象池设置",
    ToolTip = "对象池系统的全局配置设置"))
class OBJECTPOOL_API UObjectPoolSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UObjectPoolSettings();

    /** 获取设置实例 */
    static UObjectPoolSettings* Get();

    /** UDeveloperSettings interface */
    virtual FName GetCategoryName() const override;
    virtual FText GetSectionText() const override;
    virtual FText GetSectionDescription() const override;

#if WITH_EDITOR
    /** 配置更改时的回调 */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
    /** 是否启用对象池系统 */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "全局设置", meta = (
        DisplayName = "启用对象池",
        ToolTip = "是否启用对象池系统"))
    bool bEnableObjectPool = true;

    /** 默认配置模板 */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "默认配置", meta = (
        DisplayName = "默认配置模板",
        ToolTip = "新建对象池时使用的默认配置模板"))
    FObjectPoolConfigTemplate DefaultTemplate;

    /** 预设配置模板列表 */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "配置模板", meta = (
        DisplayName = "预设模板",
        ToolTip = "预定义的配置模板列表"))
    TArray<FObjectPoolConfigTemplate> PresetTemplates;

    /** 是否启用热重载 */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "开发设置", meta = (
        DisplayName = "启用热重载",
        ToolTip = "配置更改时是否自动应用到运行中的对象池"))
    bool bEnableHotReload = true;

    /** 是否启用详细日志 */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "开发设置", meta = (
        DisplayName = "启用详细日志",
        ToolTip = "是否输出详细的配置管理日志"))
    bool bEnableVerboseLogging = false;

    /** 配置文件保存路径 */
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "高级设置", meta = (
        DisplayName = "配置保存路径",
        ToolTip = "自定义配置文件的保存路径"))
    FString ConfigSavePath;

private:
    /** 初始化预设模板 */
    void InitializePresetTemplates();

    /** 创建高性能模板 */
    FObjectPoolConfigTemplate CreateHighPerformanceTemplate();

    /** 创建内存优化模板 */
    FObjectPoolConfigTemplate CreateMemoryOptimizedTemplate();

    /** 创建调试模板 */
    FObjectPoolConfigTemplate CreateDebugTemplate();
};

/**
 * 对象池配置管理器
 * 单一职责：专门负责配置的管理、应用和持久化
 */
class OBJECTPOOL_API FObjectPoolConfigManager
{
public:
    FObjectPoolConfigManager();
    ~FObjectPoolConfigManager();

    /** 初始化配置管理器 */
    void Initialize();

    /** 清理配置管理器 */
    void Shutdown();

    /** 应用配置模板到子系统 */
    bool ApplyConfigTemplate(const FObjectPoolConfigTemplate& Template, UObjectPoolSubsystem* Subsystem);

    /** 应用预设模板 */
    bool ApplyPresetTemplate(const FString& TemplateName, UObjectPoolSubsystem* Subsystem);

    /** 保存当前配置为模板 */
    bool SaveCurrentConfigAsTemplate(const FString& TemplateName, const FString& Description, UObjectPoolSubsystem* Subsystem);

    /** 获取所有可用的模板名称 */
    TArray<FString> GetAvailableTemplateNames() const;

    /** 获取指定模板 */
    bool GetTemplate(const FString& TemplateName, FObjectPoolConfigTemplate& OutTemplate) const;

    /** 热重载配置 */
    void HotReloadConfigs();

    /** 重置为默认配置 */
    void ResetToDefaults(UObjectPoolSubsystem* Subsystem);

    /** 验证配置有效性 */
    bool ValidateConfig(const FObjectPoolConfigTemplate& Template, FString& OutErrorMessage) const;

private:
    /** 是否已初始化 */
    bool bIsInitialized = false;

    /** 配置更改委托句柄 */
    FDelegateHandle ConfigChangedHandle;

    /** 配置更改回调 */
    void OnConfigChanged();

    /** 应用单个配置组件 */
    void ApplyPoolConfig(const FObjectPoolConfig& Config, UObjectPoolSubsystem* Subsystem);
    void ApplyPreallocationConfig(const FObjectPoolPreallocationConfig& Config, UObjectPoolSubsystem* Subsystem);
    void ApplyFallbackConfig(const FObjectPoolFallbackConfig& Config, UObjectPoolSubsystem* Subsystem);
    void ApplyLifecycleConfig(const FObjectPoolLifecycleConfig& Config, UObjectPoolSubsystem* Subsystem);
    void ApplyResetConfig(const FActorResetConfig& Config, UObjectPoolSubsystem* Subsystem);

    /** 日志输出 */
    void LogConfigChange(const FString& Message) const;
};
