# UE C++ 核心最佳实践 (2025年版)

## 📋 目录
- [C++标准和编译设置](#c标准和编译设置)
- [UE内置API优先原则](#ue内置api优先原则)
- [代码规范和约定](#代码规范和约定)
- [性能优化要点](#性能优化要点)
- [错误处理模式](#错误处理模式)

---

## 🔧 C++标准和编译设置

> **重要更新**：Epic Games 从 UE5.3 开始将引擎默认 C++ 标准从 C++17 升级到 C++20。
> 所有引擎模块现在强制使用 C++20 编译，插件也应保持一致以避免兼容性问题。

### **推荐的C++标准设置**

```cpp
// MyPlugin.Build.cs - 2025年推荐配置
public class MyPluginCore : ModuleRules
{
    public MyPluginCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // ✅ UE5.3+ 默认使用C++20，保持与引擎一致
        // 注意：UE5.3+ 引擎模块强制使用C++20，插件也应保持一致
        CppStandard = CppStandardVersion.Default;
        
        // ✅ Unity Build根据项目需要决定，默认关闭更安全
        bUseUnity = false;
        
        // ✅ UE标准设置
        bEnableExceptions = false;
        bUseRTTI = false;
        
        // ✅ 强制执行"包含你所使用的"，Epic官方极力推行的实践
        bEnforceIWYU = true;
    }
}
```

### **IWYU的重要性**

**为什么Epic官方极力推行IWYU？**
- **编译速度**：确保每个文件只包含真正需要的头文件，大幅减少编译依赖
- **迭代效率**：加快增量编译速度，提升开发体验
- **代码质量**：暴露隐藏的依赖关系，让代码结构更清晰

### **Unity Build权衡**

**推荐策略：**
- **开发阶段**：关闭Unity Build，确保代码质量
- **CI/CD构建**：可以考虑开启Unity Build，加快完整构建速度

---

## 🛠️ UE内置API优先原则

> **核心原则**：优先使用UE经过优化和测试的内置API，避免重复造轮子

### **1. 容器类型**

```cpp
// ✅ 使用UE优化的容器
TArray<int32> Numbers;                    // 动态数组，内存连续，缓存友好
TMap<FString, int32> StringToInt;         // 哈希表，针对UE类型优化
TSet<FString> UniqueStrings;              // 哈希集合
```

### **2. 智能指针**

```cpp
// ✅ 使用UE内存管理
TSharedPtr<FMyData> SharedData;           // 引用计数智能指针
TUniquePtr<FMyData> UniqueData;           // 独占智能指针
TWeakPtr<FMyData> WeakData;               // 弱引用指针
```

### **3. 字符串处理**

```cpp
// ✅ 使用UE字符串系统
FString MyString = TEXT("Hello World");   // 标准字符串
FName MyName = TEXT("MyIdentifier");      // 优化的标识符
FText MyText = LOCTEXT("Key", "Value");   // 本地化文本
```

---

## 📝 代码规范和约定

### **1. 命名约定**

```cpp
// ✅ 遵循UE命名规范
class MYPLUGINCORE_API FMyPluginManager    // F前缀：普通类
{
public:
    void DoSomething();                     // 动词开头的方法名
    bool IsValid() const;                   // bool返回值用Is/Has/Can开头
    
private:
    int32 ItemCount;                        // 成员变量：驼峰命名
    bool bIsEnabled;                        // bool变量：b前缀
};

UCLASS()
class MYPLUGINCORE_API UMyPluginService : public UObject  // U前缀：UObject派生类
{
    GENERATED_BODY()
    
public:
    UPROPERTY(BlueprintReadOnly, Category = "MyPlugin")
    int32 MaxItems = 100;                   // UPROPERTY：蓝图可见属性
};
```

### **2. 前向声明优先**

```cpp
// MyPluginManager.h
#pragma once

// ✅ 优先使用前向声明
class UMyPluginService;
class FMyPluginData;

class MYPLUGINCORE_API FMyPluginManager
{
public:
    void RegisterService(UMyPluginService* Service);
    void ProcessData(const FMyPluginData& Data);
    
private:
    TArray<UMyPluginService*> Services;
};
```

### **3. const正确性**

```cpp
// ✅ 正确使用const
class MYPLUGINCORE_API FMyPluginService
{
public:
    // const成员函数：不修改对象状态
    int32 GetItemCount() const { return ItemCount; }
    bool IsValid() const { return bIsValid; }
    
    // const参数：避免意外修改
    void ProcessData(const FMyData& Data);
    
    // BlueprintPure：蓝图中的纯函数
    UFUNCTION(BlueprintPure, Category = "MyPlugin")
    int32 GetMaxItems() const { return MaxItems; }
    
private:
    int32 ItemCount = 0;
    bool bIsValid = false;
    int32 MaxItems = 100;
};
```

---

## ⚡ 性能优化要点

### **1. 使用Emplace代替Add**

```cpp
// ❌ 会创建临时对象
MyArray.Add(FMyData(10, TEXT("Test")));

// ✅ 直接在容器内存中构造对象
MyArray.Emplace(10, TEXT("Test"));
```

### **2. 字符串拼接优化**

```cpp
// ❌ 在循环中性能较差
FString Result;
for (int32 i = 0; i < 100; ++i)
{
    Result += FString::Printf(TEXT("Number %d, "), i);
}

// ✅ 高效的字符串拼接
TStringBuilder<1024> Builder;
for (int32 i = 0; i < 100; ++i)
{
    Builder.Appendf(TEXT("Number %d, "), i);
}
FString Result = Builder.ToString();
```

### **3. 避免不必要的拷贝**

```cpp
// ✅ 使用const引用传递大对象
void ProcessData(const TArray<FLargeData>& DataArray);

// ✅ 返回const引用避免拷贝
const FString& GetCachedString() const { return CachedString; }
```

---

## 🛡️ 错误处理模式

### **1. TResult模式**

```cpp
// ✅ 现代化的错误处理
TResult<FMyData, FString> ProcessData(const FInputData& Input)
{
    if (!Input.IsValid())
    {
        return TResult<FMyData, FString>::MakeError(TEXT("Invalid input"));
    }
    
    FMyData Result = DoProcessing(Input);
    return TResult<FMyData, FString>::MakeSuccess(Result);
}
```

### **2. UE引擎常见模式**

```cpp
// ✅ 布尔返回值 + Out参数（引擎中最常见）
bool TryGetPlayerData(int32 PlayerIndex, FPlayerData& OutData)
{
    if (PlayerIndex >= 0 && PlayerIndex < Players.Num())
    {
        OutData = Players[PlayerIndex];
        return true;
    }
    return false;
}

// ✅ TOptional模式
TOptional<FVector> GetPlayerLocation(int32 PlayerIndex)
{
    if (PlayerIndex >= 0 && PlayerIndex < Players.Num())
    {
        return Players[PlayerIndex].Location;
    }
    return {}; // 返回空的Optional
}
```

### **3. Check vs Ensure vs Verify**

```cpp
// ✅ check: 不可恢复的程序员错误
void ProcessArray(const TArray<int32>& Array)
{
    check(Array.Num() > 0); // 如果数组为空，程序应该崩溃
}

// ✅ ensure: 可恢复的错误，会触发断点但程序继续执行
bool LoadConfiguration()
{
    if (!ensure(ConfigFile.IsValid()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Config file invalid, using defaults"));
        LoadDefaultConfig();
        return false;
    }
    return true;
}

// ✅ verify: 既检查条件又执行表达式
void InitializeSystem()
{
    verify(CreateSubsystem() != nullptr); // Shipping版本中仍会调用CreateSubsystem()
}
```

---

## 🎯 总结

### **核心原则**

1. **UE API优先**：优先使用UE内置的容器、算法、数学库等
2. **标准一致性**：使用C++20与UE5.3+引擎保持一致
3. **IWYU强制执行**：确保每个文件只包含真正需要的头文件
4. **前向声明优先**：在头文件中优先使用前向声明而非include
5. **const正确性**：正确使用const提升代码安全性和性能
6. **性能意识**：使用Emplace、TStringBuilder等优化技术
7. **错误处理一致性**：遵循UE引擎的错误处理模式

### **避免的常见错误**

- ❌ 使用STL容器替代UE容器
- ❌ 手动内存管理替代UE智能指针
- ❌ 重复实现UE已有的算法和工具
- ❌ 忽视UE的命名约定和代码风格
- ❌ 在头文件中过度使用include而非前向声明
- ❌ 忽视const正确性
- ❌ 在循环中使用低效的字符串拼接
- ❌ 使用Add/Push而非Emplace添加复杂对象

---

## 🔄 代码重构关键经验教训

> **重要提醒**：基于实际项目优化经验总结的关键教训，避免常见重构陷阱

### **1. 重构是系统性工程**
- **核心原则**: 不能只改一部分，必须完整更新所有相关代码
- **实践要点**:
  - 使用IDE的"查找所有引用"功能定位所有使用点
  - 制作修改清单，确保不遗漏任何引用
  - 重构后进行全面的编译验证

```cpp
// ❌ 错误做法 - 遗漏引用更新
// 将成员变量移动到结构体后，遗漏了某些访问点的更新
if (bPaused.Load()) // 变量已移动到StateManager，但此处未更新

// ✅ 正确做法 - 完整更新所有引用
if (StateManager.bPaused.Load()) // 所有访问点都更新为新路径
```

### **2. UE特殊性 - 函数返回值处理**
- **核心原则**: UE的某些函数有特殊的返回值处理要求
- **典型问题**: `SetTimer` 函数返回 `void`，不能用于条件判断

```cpp
// ❌ 错误做法 - SetTimer返回void，不能用于条件判断
if (!World->GetTimerManager().SetTimer(...)) // 编译错误：无法从void转换为bool
bool bResult = World->GetTimerManager().SetTimer(...); // 编译错误：无法从void初始化bool

// ✅ 正确做法 - 调用SetTimer后检查TimerHandle的有效性
World->GetTimerManager().SetTimer(TimerHandle, this, &UClass::Function, Interval, true);
if (!TimerHandle.IsValid())
{
    // 处理定时器设置失败的情况
    UE_LOG(LogTemp, Error, TEXT("定时器设置失败"));
}
```

### **3. 命名规范重要性**
- **核心原则**: 良好的命名约定可以避免很多冲突问题
- **关键实践**: 避免局部变量与类成员变量同名

```cpp
// ❌ 错误做法 - 变量名冲突
class UAsyncTools
{
    float CurveValue; // 成员变量

    void SomeFunction()
    {
        const float CurveValue = ...; // C4458警告：声明隐藏了类成员
    }
};

// ✅ 正确做法 - 使用不同的变量名
void SomeFunction()
{
    const float CalculatedCurveValue = ...; // 避免冲突
}
```

### **4. 渐进式重构**
- **核心原则**: 大型重构应该分步进行，每步都要编译验证
- **推荐流程**:
  1. **阶段1**: 重构前准备 - 识别所有修改点
  2. **阶段2**: 分步执行 - 每个小步骤后立即编译
  3. **阶段3**: 验证测试 - 功能测试确保无回归

```cpp
// 推荐的重构步骤示例：
// Step 1: 添加新的状态管理结构
struct FStateManager { ... };

// Step 2: 添加新成员变量，保留旧变量
FStateManager StateManager;
TAtomic<bool> bPaused; // 暂时保留

// Step 3: 逐步更新引用点，编译验证
// Step 4: 移除旧变量，最终编译验证
```

### **重构检查清单**
- [ ] 使用"查找所有引用"确保无遗漏
- [ ] UE函数返回值正确处理
- [ ] 检查变量命名冲突
- [ ] 分步重构，每步编译验证
- [ ] 功能测试确保无回归

---

**本指南基于UE5.4+和2025年的最佳实践，持续更新中...**

**最后更新: 2025年7月 - 新增代码重构经验教训**
