# BlueprintPro 项目蓝图功能扩展手册

> **核心思想**: 本手册基于优秀的开源项目 `BlueprintPro`，分析并展示其为蓝图系统提供的核心功能扩展，为XTools开发提供参考。
> **参考源**: [BlueprintPro项目](https://github.com/xusjtuer/BlueprintPro/tree/main)
> **适用于**: 想要深入理解蓝图扩展技术的开发者

> 💡 **相关文档**:
> - [UE K2Node 开发指南](UE%20K2Node%20开发指南.md) - 系统学习K2Node开发
> - [高级K2Node开发案例分析](NoteUE4%20项目高级K2Node开发手册.md) - 更多实战案例
> - [XTools源码](../Source/) - 查看XTools的实际实现

---

## 📋 目录
1.  [数据结构扩展：TMap 与 TSet](#1-数据结构扩展tmap-与-tset) - 弥补蓝图数据结构操作的不足
2.  [异步资源加载](#2-异步资源加载) - 避免同步加载导致的卡顿
3.  [高级字符串操作：正则表达式](#3-高级字符串操作正则表达式) - 强大的字符串处理
4.  [反射与类工具](#4-反射与类工具) - 运行时类型信息获取

---

## 1. 数据结构扩展：TMap 与 TSet

蓝图原生对 `TMap` (字典) 和 `TSet` (集合) 的操作支持非常有限，例如无法直接获取所有键（Keys）或值（Values）。`BlueprintPro` 极大地弥补了这一短板。

#### 🔹 目标
- 为蓝图添加一系列 `TMap` 和 `TSet` 的实用工具节点。
- 实现获取 `TMap` 的所有键/值、`TSet` 与 `TArray` 的相互转换等功能。

#### 🔹 技术拆解
- **泛型函数**: 通过C++模板和通配符（Wildcard）元数据，实现能处理任意类型 `TMap` 和 `TSet` 的泛型蓝图函数。
- **`CustomThunk`**: 对于需要直接操作内存的泛型函数（如 `GetMapKeys`），使用 `CustomThunk` 手动解析参数，并调用一个 `void*` 版本的底层实现，以达到真正的泛型。
- **传引用参数 (`&`)**: 对于输出数组的参数，使用引用传递（如 `TArray<int32>& OutArray`），这在蓝图中会表现为输出数据引脚。

#### 🔹 参考文件
- **接口声明**: `/Plugins/BlueprintPro/Source/BlueprintPro/Public/BPProFunctionLibrary.h`
- **实现细节**: `/Plugins/BlueprintPro/Source/BlueprintPro/Private/BPProFunctionLibrary.cpp`

#### 🔹 关键代码片段

**1. 获取 TMap 的所有键 (`BPProFunctionLibrary.h`)**:
```cpp
UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Get Map Keys", CompactNodeTitle = "KEYS", ArrayParm = "Keys"), Category = "BlueprintPro|Map")
static void GetMapKeys(const TMap<int32, int32>& TargetMap, TArray<int32>& Keys);
```
- **`CustomThunk`**: 表明此函数有自定义的底层实现，以处理任意类型的Key。
- **`ArrayParm = "Keys"`**: 告诉引擎 `Keys` 参数是主要的通配符数组输出。

**2. 获取 TMap 的所有值 (`BPProFunctionLibrary.h`)**:
```cpp
UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "Get Map Values", CompactNodeTitle = "VALUES", ArrayParm = "Values"), Category = "BlueprintPro|Map")
static void GetMapValues(const TMap<int32, int32>& TargetMap, TArray<int32>& Values);
```

**3. TSet 转 TArray (`BPProFunctionLibrary.h`)**:
```cpp
UFUNCTION(BlueprintCallable, CustomThunk, meta = (DisplayName = "To Array", CompactNodeTitle = "->", ArrayParm = "OutArray"), Category = "BlueprintPro|Set")
static void SetToArray(const TSet<int32>& TargetSet, TArray<int32>& OutArray);
```

**4. `CustomThunk` 的实现 (`BPProFunctionLibrary.cpp`)**:
所有这些函数的 `exec` 版本都调用了一个通用的底层函数 `GenericGetMapKeys` / `GenericGetMapValues`。
```cpp
// BPProFunctionLibrary.cpp
// 以 GetMapKeys 为例
DECLARE_FUNCTION(execGetMapKeys)
{
    // ... 手动解析通配符TMap参数 ...
    Stack.Step(Stack.Object, NULL);
    FMapProperty* MapProperty = ExactCastField<FMapProperty>(Stack.MostRecentProperty);
    void* MapAddress = Stack.MostRecentPropertyAddress;

    // ... 手动解析输出数组参数 ...
    Stack.Step(Stack.Object, NULL);
    FArrayProperty* ArrayProperty = ExactCastField<FArrayProperty>(Stack.MostRecentProperty);
    void* ArrayAddress = Stack.MostRecentPropertyAddress;

    P_FINISH;
    P_NATIVE_BEGIN;
    // 调用真正的泛型实现
    GenericGetMapKeys(MapAddress, MapProperty, ArrayAddress, ArrayProperty);
    P_NATIVE_END;
}
```

---

## 2. 异步资源加载

在蓝图中同步加载（`Load Asset`）大型资源会导致游戏线程卡顿。`BlueprintPro` 提供了一个简单易用的异步加载节点。

#### 🔹 目标
- 创建一个 `Load Asset Async` 节点。
- 节点执行后立即返回，不会阻塞游戏。
- 当资源在后台加载完成后，通过一个委托（Delegate）回调来通知蓝图。

#### 🔹 技术拆解
- **`StreamableManager` 和 `FStreamableDelegate`**: 这是UE用于管理异步加载的核心系统。通过请求一个异步加载，并绑定一个回调委托，可以在资源加载完成后执行指定逻辑。
- **自定义委托**: 声明一个动态多播委托 `FBPProLoadAssetDelegate`，并将其作为函数参数暴露给蓝图，这样蓝图就可以将一个自定义事件连接到这个委托上。

#### 🔹 参考文件
- **接口声明**: `/Plugins/BlueprintPro/Source/BlueprintPro/Public/BPProFunctionLibrary.h`
- **实现细节**: `/Plugins/BlueprintPro/Source/BlueprintPro/Private/BPProFunctionLibrary.cpp`

#### 🔹 关键代码片段

**1. 声明委托和函数 (`BPProFunctionLibrary.h`)**:
```cpp
// 声明一个动态多播委托，它带有一个UObject*参数
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBPProLoadAssetDelegate, UObject*, LoadedAsset);

UFUNCTION(BlueprintCallable, meta = (DisplayName = "Load Asset Async"), Category = "BlueprintPro|Asset")
static void LoadAssetAsync(TSoftObjectPtr<UObject> Asset, FBPProLoadAssetDelegate OnLoaded);
```
- **`TSoftObjectPtr`**: 使用软对象指针作为输入，这是一种轻量级的、不会在加载前引用资源的指针。
- **`FBPProLoadAssetDelegate OnLoaded`**: 将委托作为函数参数。在蓝图中，这会显示为一个可连线的事件输出引脚。

**2. 实现异步加载逻辑 (`BPProFunctionLibrary.cpp`)**:
```cpp
// BPProFunctionLibrary.cpp
void UBPProFunctionLibrary::LoadAssetAsync(TSoftObjectPtr<UObject> Asset, FBPProLoadAssetDelegate OnLoaded)
{
    // 如果软指针无效，直接返回
    if (Asset.IsNull())
    {
        return;
    }

    // 1. 获取资源加载管理器
    FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
    
    // 2. 创建一个在加载完成后执行的Lambda函数
    FStreamableDelegate Delegate = FStreamableDelegate::CreateLambda([OnLoaded, Asset]()
    {
        // 广播委托，并传入加载好的资产
        OnLoaded.ExecuteIfBound(Asset.Get());
    });
    
    // 3. 请求异步加载
    StreamableManager.RequestAsyncLoad(Asset.ToSoftObjectPath(), Delegate);
}
```

---

## 3. 高级字符串操作：正则表达式

蓝图缺乏对正则表达式（Regex）的支持，这使得复杂的字符串匹配和验证变得困难。`BlueprintPro` 提供了简单直接的正则匹配节点。

#### 🔹 目标
- 创建 `Regex Match` 节点。
- 输入一个字符串和一个正则表达式模式（Pattern）。
- 输出匹配到的所有结果。

#### 🔹 技术拆解
- **`FRegexMatcher`**: 这是UE内置的处理正则表达式的类。通过它来设置模式并对输入字符串进行查找。

#### 🔹 参考文件
- **接口声明**: `/Plugins/BlueprintPro/Source/BlueprintPro/Public/BPProFunctionLibrary.h`
- **实现细节**: `/Plugins/BlueprintPro/Source/BlueprintPro/Private/BPProFunctionLibrary.cpp`

#### 🔹 关键代码片段

**1. 声明函数 (`BPProFunctionLibrary.h`)**:
```cpp
UFUNCTION(BlueprintCallable, Category = "BlueprintPro|String")
static bool RegexMatch(const FString& Pattern, const FString& Input, TArray<FString>& OutMatches);
```
- `Pattern`: 正则表达式。
- `Input`: 要匹配的源字符串。
- `OutMatches`: 输出一个字符串数组，包含所有匹配到的子串。

**2. 实现正则匹配逻辑 (`BPProFunctionLibrary.cpp`)**:
```cpp
// BPProFunctionLibrary.cpp
bool UBPProFunctionLibrary::RegexMatch(const FString& Pattern, const FString& Input, TArray<FString>& OutMatches)
{
    // 1. 创建正则表达式模式
    const FRegexPattern RegexPattern(Pattern);
    // 2. 创建匹配器
    FRegexMatcher Matcher(RegexPattern, Input);

    OutMatches.Empty();
    
    // 3. 循环查找所有匹配项
    while (Matcher.FindNext())
    {
        // 将找到的匹配组添加到输出数组
        OutMatches.Add(Matcher.GetCaptureGroup(0));
    }

    return OutMatches.Num() > 0;
}
```
---

## 4. 反射与类工具

提供在运行时获取类信息的工具，例如查找一个基类的所有子类。

#### 🔹 目标
- 创建一个 `Get All Child Classes` 节点。
- 输入一个父类。
- 输出一个数组，包含项目中所有继承自该父类的子类。

#### 🔹 技术拆解
- **`TObjectIterator`**: 这是UE中用于遍历内存中所有UObject实例的迭代器。可以指定一个类，来遍历该类的所有对象。
- **`IsChildOf`**: 用于检查一个类是否是另一个类的子类。

#### 🔹 参考文件
- **接口声明**: `/Plugins/BlueprintPro/Source/BlueprintPro/Public/BPProFunctionLibrary.h`
- **实现细节**: `/Plugins/BlueprintPro/Source/BlueprintPro/Private/BPProFunctionLibrary.cpp`

#### 🔹 关键代码片段

**1. 声明函数 (`BPProFunctionLibrary.h`)**:
```cpp
UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get All Child Classes Of"), Category = "BlueprintPro|Class")
static TArray<UClass*> GetAllChildClassesOf(UClass* ParentClass);
```

**2. 实现查找逻辑 (`BPProFunctionLibrary.cpp`)**:
```cpp
// BPProFunctionLibrary.cpp
TArray<UClass*> UBPProFunctionLibrary::GetAllChildClassesOf(UClass* ParentClass)
{
    TArray<UClass*> Results;
    if (!ParentClass)
    {
        return Results;
    }

    // 1. 遍历内存中所有的UClass对象
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        
        // 2. 检查条件：
        // a. 是 ParentClass 的子类
        // b. 不是 ParentClass 本身
        // c. 不是一个抽象类或即将被销毁的类
        if (Class->IsChildOf(ParentClass) &&
            Class != ParentClass &&
            !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists))
        {
            Results.Add(Class);
        }
    }
    return Results;
}
