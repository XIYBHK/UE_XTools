# UE C++ 核心最佳实践 (2025年版)

## 📋 目录
- [C++标准和编译设置](#c标准和编译设置)
- [UE内置API优先原则](#ue内置api优先原则)
- [代码规范和约定](#代码规范和约定)
- [性能优化要点](#性能优化要点)
- [错误处理模式](#错误处理模式)

---

## 🔧 C++标准和编译设置

### **推荐的C++标准设置**

```cpp
// MyPlugin.Build.cs - 2025年推荐配置
public class MyPluginCore : ModuleRules
{
    public MyPluginCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // ✅ 使用稳定的C++17，除非确实需要C++20特性
        CppStandard = CppStandardVersion.Cpp17;
        
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
2. **稳定性优先**：使用C++17而非激进的C++20
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

**本指南基于UE5.4+和2025年的最佳实践，持续更新中...**

**最后更新: 2025年7月**
