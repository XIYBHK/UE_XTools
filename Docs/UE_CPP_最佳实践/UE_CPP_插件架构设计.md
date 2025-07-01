# UE C++ 插件架构设计 (2025年版)

## 📋 目录
- [现代化插件架构](#现代化插件架构)
- [模块化设计](#模块化设计)
- [配置系统](#配置系统)
- [UI组件设计](#ui组件设计)
- [国际化支持](#国际化支持)

---

## 🏗️ 现代化插件架构

### **1. 推荐的目录结构**

```
MyPlugin/
├── Source/
│   ├── MyPluginCore/          # 核心运行时模块
│   │   ├── Public/
│   │   │   ├── MyPluginCore.h
│   │   │   ├── Interfaces/
│   │   │   ├── Data/
│   │   │   └── Services/
│   │   ├── Private/
│   │   └── MyPluginCore.Build.cs
│   ├── MyPluginEditor/        # 编辑器模块
│   │   ├── Public/
│   │   ├── Private/
│   │   └── MyPluginEditor.Build.cs
│   └── MyPluginTests/         # 测试模块
├── Content/                   # 蓝图和资源
├── Config/                    # 配置文件
├── Resources/                 # 图标等资源
└── MyPlugin.uplugin          # 插件描述文件
```

### **2. 模块依赖设计**

```cpp
// MyPluginCore.Build.cs - 核心模块
public class MyPluginCore : ModuleRules
{
    public MyPluginCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnforceIWYU = true;
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
        
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "DeveloperSettings"
        });
    }
}

// MyPluginEditor.Build.cs - 编辑器模块
public class MyPluginEditor : ModuleRules
{
    public MyPluginEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnforceIWYU = true;
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "MyPluginCore"  // 依赖核心模块
        });
        
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "EditorStyle",
            "EditorWidgets",
            "ToolMenus"
        });
    }
}
```

### **3. 接口驱动设计**

```cpp
// IMyPluginService.h - 服务接口
class MYPLUGINCORE_API IMyPluginService
{
public:
    virtual ~IMyPluginService() = default;
    
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsReady() const = 0;
    
    virtual TFuture<bool> ProcessDataAsync(const FMyPluginData& Data) = 0;
};

// MyPluginServiceImpl.h - 具体实现
UCLASS()
class MYPLUGINCORE_API UMyPluginServiceImpl : public UObject, public IMyPluginService
{
    GENERATED_BODY()
    
public:
    // IMyPluginService interface
    virtual bool Initialize() override;
    virtual void Shutdown() override;
    virtual bool IsReady() const override;
    virtual TFuture<bool> ProcessDataAsync(const FMyPluginData& Data) override;
    
private:
    bool bIsInitialized = false;
};
```

---

## 🧩 模块化设计

### **1. 子系统架构**

```cpp
// MyPluginSubsystem.h - 主要子系统
UCLASS()
class MYPLUGINCORE_API UMyPluginSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()
    
public:
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    // 静态访问方法
    static UMyPluginSubsystem* Get();
    
    // 服务管理
    void RegisterService(TScriptInterface<IMyPluginService> Service);
    void UnregisterService(TScriptInterface<IMyPluginService> Service);
    
    // 蓝图可访问的API
    UFUNCTION(BlueprintCallable, Category = "MyPlugin")
    bool ProcessData(const FMyPluginData& Data);
    
private:
    UPROPERTY()
    TArray<TScriptInterface<IMyPluginService>> RegisteredServices;
};
```

### **2. 事件系统**

```cpp
// MyPluginEvents.h - 事件定义
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMyPluginDataProcessed, const FMyPluginData&, ProcessedData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMyPluginError, const FString&, ErrorMessage);

UCLASS(BlueprintType)
class MYPLUGINCORE_API UMyPluginEventBus : public UObject
{
    GENERATED_BODY()
    
public:
    // 事件广播
    UPROPERTY(BlueprintAssignable, Category = "MyPlugin Events")
    FMyPluginDataProcessed OnDataProcessed;
    
    UPROPERTY(BlueprintAssignable, Category = "MyPlugin Events")
    FMyPluginError OnError;
    
    // 静态访问
    static UMyPluginEventBus* Get();
    
    // 事件触发方法
    void BroadcastDataProcessed(const FMyPluginData& Data);
    void BroadcastError(const FString& ErrorMessage);
};
```

---

## ⚙️ 配置系统

### **1. 开发者设置**

```cpp
// MyPluginSettings.h
UCLASS(Config=MyPlugin, DefaultConfig, meta=(DisplayName="My Plugin Settings"))
class MYPLUGINCORE_API UMyPluginSettings : public UDeveloperSettings
{
    GENERATED_BODY()
    
public:
    UMyPluginSettings();
    
    // UDeveloperSettings interface
    virtual FName GetCategoryName() const override;
    virtual FText GetSectionText() const override;
    
    // 配置属性
    UPROPERTY(Config, EditAnywhere, Category = "General", 
              meta = (ClampMin = "1", ClampMax = "100"))
    int32 MaxConcurrentTasks = 10;
    
    UPROPERTY(Config, EditAnywhere, Category = "General")
    bool bEnableDetailedLogging = false;
    
    UPROPERTY(Config, EditAnywhere, Category = "Performance",
              meta = (ClampMin = "10", ClampMax = "1000"))
    int32 CacheSizeMB = 100;
    
    // 静态访问方法
    static const UMyPluginSettings* Get();
};
```

### **2. 运行时配置**

```cpp
// MyPluginRuntimeConfig.h
UCLASS(BlueprintType, Config=Game)
class MYPLUGINCORE_API UMyPluginRuntimeConfig : public UObject
{
    GENERATED_BODY()
    
public:
    // 运行时可修改的配置
    UPROPERTY(BlueprintReadWrite, Config, Category = "Runtime")
    bool bEnableFeatureX = true;
    
    UPROPERTY(BlueprintReadWrite, Config, Category = "Runtime")
    float ProcessingSpeed = 1.0f;
    
    // 配置变更通知
    UFUNCTION(BlueprintCallable, Category = "MyPlugin")
    void ApplySettings();
    
    // 重置为默认值
    UFUNCTION(BlueprintCallable, Category = "MyPlugin")
    void ResetToDefaults();
};
```

---

## 🎨 UI组件设计

### **1. 编辑器工具窗口**

```cpp
// MyPluginEditorWidget.h
class MYPLUGINEDITOR_API SMyPluginEditorWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMyPluginEditorWidget) {}
    SLATE_END_ARGS()
    
    void Construct(const FArguments& InArgs);
    
private:
    // UI回调
    FReply OnProcessButtonClicked();
    FReply OnClearButtonClicked();
    
    // UI状态
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<SProgressBar> ProgressBar;
    
    // 数据绑定
    FText GetStatusText() const;
    TOptional<float> GetProgressPercent() const;
};
```

### **2. 蓝图节点**

```cpp
// MyPluginBlueprintNodes.h
UCLASS()
class MYPLUGINEDITOR_API UK2Node_MyPluginAsyncAction : public UK2Node_BaseAsyncTask
{
    GENERATED_BODY()
    
public:
    // UK2Node interface
    virtual FText GetTooltipText() const override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetMenuCategory() const override;
    
    // UK2Node_BaseAsyncTask interface
    virtual FString GetCppType() const override;
    virtual UClass* GetProxyFactoryClass() const override;
};
```

---

## 🌍 国际化支持

### **1. 本地化文本定义**

```cpp
// MyPluginLocalization.h
#define LOCTEXT_NAMESPACE "MyPlugin"

// 常用文本定义
namespace MyPluginTexts
{
    static const FText PluginName = LOCTEXT("PluginName", "My Plugin");
    static const FText ProcessingData = LOCTEXT("ProcessingData", "Processing data...");
    static const FText ProcessingComplete = LOCTEXT("ProcessingComplete", "Processing complete");
    static const FText ErrorOccurred = LOCTEXT("ErrorOccurred", "An error occurred");
}

#undef LOCTEXT_NAMESPACE
```

### **2. 动态文本格式化**

```cpp
// 使用FText进行格式化
FText FormatProcessingStatus(int32 Current, int32 Total)
{
    return FText::Format(
        LOCTEXT("ProcessingStatus", "Processing {0} of {1} items"),
        FText::AsNumber(Current),
        FText::AsNumber(Total)
    );
}

// 复杂格式化
FText FormatDetailedStatus(const FString& Operation, float Progress, const FTimespan& TimeRemaining)
{
    return FText::Format(
        LOCTEXT("DetailedStatus", "{Operation}: {Progress}% complete, {TimeRemaining} remaining"),
        FText::FromString(Operation),
        FText::AsPercent(Progress / 100.0f),
        FText::AsTimespan(TimeRemaining)
    );
}
```

---

## 🎯 总结

### **架构设计核心原则**

1. **模块化分离**：Core/Editor/Tests清晰分离
2. **接口驱动**：使用接口定义服务契约
3. **子系统集成**：利用UE子系统框架
4. **事件解耦**：通过事件系统减少模块间耦合
5. **配置灵活性**：支持开发时和运行时配置
6. **国际化就绪**：从设计阶段考虑多语言支持

### **设计检查清单**

- ✅ 模块依赖关系清晰且最小化
- ✅ 接口定义完整且稳定
- ✅ 子系统正确注册和初始化
- ✅ 事件系统设计合理
- ✅ 配置系统易用且类型安全
- ✅ UI组件遵循Slate最佳实践
- ✅ 所有用户可见文本支持本地化

---

**本指南基于UE5.4+和2025年的最佳实践，持续更新中...**

**最后更新: 2025年7月**
