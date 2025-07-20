// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 遵循IWYU原则的头文件包含
#include "ObjectPoolConfigManager.h"

// ✅ UE核心依赖
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/ConfigCacheIni.h"

// ✅ 对象池模块依赖
#include "ObjectPool.h"
#include "ObjectPoolSubsystem.h"

// ✅ UObjectPoolSettings实现

UObjectPoolSettings::UObjectPoolSettings()
{
    // ✅ 设置配置文件名
    CategoryName = TEXT("XTools");
    SectionName = TEXT("ObjectPool");
    
    // ✅ 初始化默认值
    bEnableObjectPool = true;
    bEnableHotReload = true;
    bEnableVerboseLogging = false;
    ConfigSavePath = TEXT("Config/ObjectPool/");
    
    // ✅ 初始化预设模板
    InitializePresetTemplates();
}

UObjectPoolSettings* UObjectPoolSettings::Get()
{
    return GetMutableDefault<UObjectPoolSettings>();
}

FName UObjectPoolSettings::GetCategoryName() const
{
    return TEXT("XTools");
}

FText UObjectPoolSettings::GetSectionText() const
{
    return NSLOCTEXT("ObjectPool", "ObjectPoolSettingsSection", "对象池");
}

FText UObjectPoolSettings::GetSectionDescription() const
{
    return NSLOCTEXT("ObjectPool", "ObjectPoolSettingsDescription", "配置对象池系统的全局设置和预设模板");
}

#if WITH_EDITOR
void UObjectPoolSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    // ✅ 配置更改时触发热重载
    if (bEnableHotReload)
    {
        if (UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::GetGlobal())
        {
            // 通知子系统配置已更改
            OBJECTPOOL_LOG(Log, TEXT("配置已更改，触发热重载"));
        }
    }
}
#endif

void UObjectPoolSettings::InitializePresetTemplates()
{
    PresetTemplates.Empty();
    
    // ✅ 添加预设模板
    PresetTemplates.Add(CreateHighPerformanceTemplate());
    PresetTemplates.Add(CreateMemoryOptimizedTemplate());
    PresetTemplates.Add(CreateDebugTemplate());
}

FObjectPoolConfigTemplate UObjectPoolSettings::CreateHighPerformanceTemplate()
{
    FObjectPoolConfigTemplate Template;
    Template.TemplateName = TEXT("高性能模板");
    Template.Description = TEXT("优化性能的配置，适合高频使用的对象池");
    
    // ✅ 高性能池配置
    Template.PoolConfig.InitialSize = 50;
    Template.PoolConfig.HardLimit = 200;
    Template.PoolConfig.bAutoExpand = true;
    Template.PoolConfig.bAutoShrink = false; // 不自动收缩以保持性能
    
    // ✅ 立即预分配策略
    Template.PreallocationConfig.Strategy = EObjectPoolPreallocationStrategy::Immediate;
    Template.PreallocationConfig.PreallocationCount = 30;
    Template.PreallocationConfig.MaxAllocationsPerFrame = 10;
    Template.PreallocationConfig.bEnableMemoryBudget = false; // 不限制内存预算
    
    // ✅ 永不失败回退策略
    Template.FallbackConfig.Strategy = EObjectPoolFallbackStrategy::NeverFail;
    Template.FallbackConfig.bAllowDefaultActorFallback = true;
    
    return Template;
}

FObjectPoolConfigTemplate UObjectPoolSettings::CreateMemoryOptimizedTemplate()
{
    FObjectPoolConfigTemplate Template;
    Template.TemplateName = TEXT("内存优化模板");
    Template.Description = TEXT("优化内存使用的配置，适合内存敏感的环境");
    
    // ✅ 内存优化池配置
    Template.PoolConfig.InitialSize = 10;
    Template.PoolConfig.HardLimit = 50;
    Template.PoolConfig.bAutoExpand = true;
    Template.PoolConfig.bAutoShrink = true; // 启用自动收缩
    
    // ✅ 渐进式预分配策略
    Template.PreallocationConfig.Strategy = EObjectPoolPreallocationStrategy::Progressive;
    Template.PreallocationConfig.PreallocationCount = 5;
    Template.PreallocationConfig.MaxAllocationsPerFrame = 2;
    Template.PreallocationConfig.bEnableMemoryBudget = true;
    Template.PreallocationConfig.MaxMemoryBudgetMB = 32; // 限制内存使用
    
    // ✅ 保守的回退策略
    Template.FallbackConfig.Strategy = EObjectPoolFallbackStrategy::StrictMode;
    Template.FallbackConfig.bLogFallbackWarnings = true;
    
    return Template;
}

FObjectPoolConfigTemplate UObjectPoolSettings::CreateDebugTemplate()
{
    FObjectPoolConfigTemplate Template;
    Template.TemplateName = TEXT("调试模板");
    Template.Description = TEXT("便于调试的配置，启用详细日志和统计");
    
    // ✅ 调试友好的池配置
    Template.PoolConfig.InitialSize = 5;
    Template.PoolConfig.HardLimit = 20;
    Template.PoolConfig.bAutoExpand = true;
    Template.PoolConfig.bAutoShrink = true;
    
    // ✅ 预测性预分配策略
    Template.PreallocationConfig.Strategy = EObjectPoolPreallocationStrategy::Predictive;
    Template.PreallocationConfig.PreallocationCount = 3;
    Template.PreallocationConfig.MaxAllocationsPerFrame = 1;
    Template.PreallocationConfig.bEnableMemoryBudget = true;
    Template.PreallocationConfig.MaxMemoryBudgetMB = 16;
    
    // ✅ 详细的生命周期配置
    Template.LifecycleConfig.bEnableLifecycleEvents = true;
    Template.LifecycleConfig.bLogEventErrors = true;
    Template.LifecycleConfig.EventTimeoutMs = 5000; // 更长的超时时间便于调试
    
    // ✅ 详细的重置配置
    Template.ResetConfig.bResetTransform = true;
    Template.ResetConfig.bResetPhysics = true;
    Template.ResetConfig.bResetAI = true;
    Template.ResetConfig.bResetAnimation = true;
    Template.ResetConfig.bClearTimers = true;
    
    return Template;
}

// ✅ FObjectPoolConfigManager实现

FObjectPoolConfigManager::FObjectPoolConfigManager()
{
    bIsInitialized = false;
}

FObjectPoolConfigManager::~FObjectPoolConfigManager()
{
    Shutdown();
}

void FObjectPoolConfigManager::Initialize()
{
    if (bIsInitialized)
    {
        return;
    }
    
    LogConfigChange(TEXT("配置管理器初始化"));
    
    // ✅ 注册配置更改回调
    if (UObjectPoolSettings* Settings = UObjectPoolSettings::Get())
    {
        // 在UE5中，配置更改通过PostEditChangeProperty处理
        LogConfigChange(TEXT("配置管理器初始化完成"));
    }
    
    bIsInitialized = true;
}

void FObjectPoolConfigManager::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }
    
    LogConfigChange(TEXT("配置管理器关闭"));
    
    // ✅ 清理回调
    if (ConfigChangedHandle.IsValid())
    {
        // 清理委托句柄
        ConfigChangedHandle.Reset();
    }
    
    bIsInitialized = false;
}

bool FObjectPoolConfigManager::ApplyConfigTemplate(const FObjectPoolConfigTemplate& Template, UObjectPoolSubsystem* Subsystem)
{
    if (!IsValid(Subsystem))
    {
        LogConfigChange(TEXT("应用配置模板失败：子系统无效"));
        return false;
    }
    
    FString ErrorMessage;
    if (!ValidateConfig(Template, ErrorMessage))
    {
        LogConfigChange(FString::Printf(TEXT("配置模板验证失败：%s"), *ErrorMessage));
        return false;
    }
    
    LogConfigChange(FString::Printf(TEXT("开始应用配置模板：%s"), *Template.TemplateName));
    
    // ✅ 应用各个配置组件（利用已有的子系统API）
    ApplyPoolConfig(Template.PoolConfig, Subsystem);
    ApplyPreallocationConfig(Template.PreallocationConfig, Subsystem);
    ApplyFallbackConfig(Template.FallbackConfig, Subsystem);
    ApplyLifecycleConfig(Template.LifecycleConfig, Subsystem);
    ApplyResetConfig(Template.ResetConfig, Subsystem);
    
    LogConfigChange(FString::Printf(TEXT("配置模板应用完成：%s"), *Template.TemplateName));
    return true;
}

bool FObjectPoolConfigManager::ApplyPresetTemplate(const FString& TemplateName, UObjectPoolSubsystem* Subsystem)
{
    FObjectPoolConfigTemplate Template;
    if (!GetTemplate(TemplateName, Template))
    {
        LogConfigChange(FString::Printf(TEXT("未找到配置模板：%s"), *TemplateName));
        return false;
    }
    
    return ApplyConfigTemplate(Template, Subsystem);
}

TArray<FString> FObjectPoolConfigManager::GetAvailableTemplateNames() const
{
    TArray<FString> Names;
    
    if (const UObjectPoolSettings* Settings = UObjectPoolSettings::Get())
    {
        for (const FObjectPoolConfigTemplate& Template : Settings->PresetTemplates)
        {
            Names.Add(Template.TemplateName);
        }
    }
    
    return Names;
}

bool FObjectPoolConfigManager::GetTemplate(const FString& TemplateName, FObjectPoolConfigTemplate& OutTemplate) const
{
    if (const UObjectPoolSettings* Settings = UObjectPoolSettings::Get())
    {
        for (const FObjectPoolConfigTemplate& Template : Settings->PresetTemplates)
        {
            if (Template.TemplateName == TemplateName)
            {
                OutTemplate = Template;
                return true;
            }
        }
    }
    
    return false;
}

bool FObjectPoolConfigManager::ValidateConfig(const FObjectPoolConfigTemplate& Template, FString& OutErrorMessage) const
{
    // ✅ 验证基础池配置
    if (!Template.PoolConfig.IsValid())
    {
        OutErrorMessage = TEXT("池配置无效：Actor类为空或初始大小无效");
        return false;
    }

    // ✅ 验证预分配配置
    if (Template.PreallocationConfig.PreallocationCount < 0)
    {
        OutErrorMessage = TEXT("预分配配置无效：预分配数量不能为负数");
        return false;
    }

    return true;
}

void FObjectPoolConfigManager::ResetToDefaults(UObjectPoolSubsystem* Subsystem)
{
    if (!IsValid(Subsystem))
    {
        LogConfigChange(TEXT("重置为默认配置失败：子系统无效"));
        return;
    }

    if (const UObjectPoolSettings* Settings = UObjectPoolSettings::Get())
    {
        LogConfigChange(TEXT("开始重置为默认配置"));
        ApplyConfigTemplate(Settings->DefaultTemplate, Subsystem);
        LogConfigChange(TEXT("默认配置重置完成"));
    }
    else
    {
        LogConfigChange(TEXT("重置为默认配置失败：无法获取设置"));
    }
}

void FObjectPoolConfigManager::LogConfigChange(const FString& Message) const
{
    if (const UObjectPoolSettings* Settings = UObjectPoolSettings::Get())
    {
        if (Settings->bEnableVerboseLogging)
        {
            OBJECTPOOL_LOG(Log, TEXT("ConfigManager: %s"), *Message);
        }
    }
}

// ✅ 配置应用的具体实现将在下一部分完成
void FObjectPoolConfigManager::ApplyPoolConfig(const FObjectPoolConfig& Config, UObjectPoolSubsystem* Subsystem) {}
void FObjectPoolConfigManager::ApplyPreallocationConfig(const FObjectPoolPreallocationConfig& Config, UObjectPoolSubsystem* Subsystem) {}
void FObjectPoolConfigManager::ApplyFallbackConfig(const FObjectPoolFallbackConfig& Config, UObjectPoolSubsystem* Subsystem) {}
void FObjectPoolConfigManager::ApplyLifecycleConfig(const FObjectPoolLifecycleConfig& Config, UObjectPoolSubsystem* Subsystem) {}
void FObjectPoolConfigManager::ApplyResetConfig(const FActorResetConfig& Config, UObjectPoolSubsystem* Subsystem) {}
