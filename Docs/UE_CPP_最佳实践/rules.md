## C++标准和编译配置
- 使用C++20标准与UE5.3+引擎保持一致，优先使用稳定的C++20特性
- 在Build.cs文件中始终设置 `bEnforceIWYU = true` 强制执行"包含你所使用的"原则
- 开发时默认设置 `bUseUnity = false` 确保代码质量并暴露依赖问题
- 遵循UE标准设置 `bEnableExceptions = false` 和 `bUseRTTI = false`
- 使用 `PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs` 获得最佳编译性能

## UE内置API优先原则
> **核心理念**: 不要重复造轮子，UE已经提供了经过优化和测试的实现

- 始终优先使用UE内置容器而非STL：使用TArray、TMap、TSet而不是std::vector、std::map、std::set
- 使用UE智能指针：TSharedPtr、TUniquePtr、TWeakPtr而不是std智能指针
- 使用UE字符串类型：FString用于通用字符串，FName用于标识符，FText用于本地化文本
- 优先使用UE数学库和算法而不是自定义实现：FMath、FVector、FRotator、FTransform等
- 使用UE内存管理模式，避免手动内存管理
- 使用UE异步系统：Async()、AsyncTask()而不是std::thread或自定义线程
- 使用UE时间系统：FDateTime、FTimespan、FPlatformTime而不是std::chrono
- 使用UE文件系统：IFileManager、FPlatformFilemanager而不是std::filesystem
- 使用UE日志系统：UE_LOG而不是printf或std::cout
- 使用UE断言系统：check()、ensure()、verify()而不是assert()

## 命名约定
- 严格遵循UE命名约定：
  - 类：普通类使用F前缀，UObject派生类使用U前缀，AActor派生类使用A前缀
  - 接口：接口类使用I前缀
  - 枚举：枚举类型使用E前缀
  - 布尔变量：使用b前缀（如bIsEnabled）
  - 成员变量：使用帕斯卡命名法，不使用前缀
  - 函数：动作使用动词开头，布尔返回值使用Is/Has/Can开头

## 代码组织和依赖关系
- 在头文件中尽可能优先使用前向声明而不是#include
- 使用IWYU原则：每个文件只包含实际使用的内容
- 按顺序组织包含文件：自己的头文件、UE头文件、第三方头文件、标准库
- 保持头文件简洁，将实现细节移到.cpp文件中
- 使用const正确性：当函数不修改对象状态时标记为const

## 迭代器和模板使用注意事项
- **避免假设迭代器支持减法运算**：UE的迭代器不一定支持 `end - begin` 操作
- **使用循环计算距离**：用 `for` 循环计算迭代器距离而不是 `std::distance`
- **检查函数返回类型**：确认函数返回类型再用于条件判断
- **模板兼容性**：编写模板时考虑UE迭代器的特殊性

## IWYU实施注意事项

### 常见问题和解决方案
1. **前向声明不足**
   ```cpp
   // ❌ 错误 - 缺少前向声明
   #include "MyClass.h" // 仅为了指针使用
   
   // ✅ 正确 - 使用前向声明
   class UMyClass; // 前向声明
   ```

2. **模板特化问题**
   - 模板类可能需要完整定义
   - 使用`IWYU pragma: keep`注释保留必要包含

3. **UE宏依赖**
   - `UCLASS()`、`USTRUCT()`等宏可能需要额外包含
   - 确保包含`UObject/NoExportTypes.h`等必要头文件

## 性能优化模式
- 在容器中构造对象时使用Emplace()而不是Add()
- 使用TStringBuilder进行高效字符串拼接，而不是重复的FString操作
- 对大对象参数优先使用const引用以避免不必要的拷贝
- 对频繁分配/释放的对象使用对象池
- 当已知大小时预分配容器容量：使用Reserve()方法

## 高级性能优化模式

### 内存预分配策略
```cpp
// ✅ 智能预分配
void ProcessLargeDataSet(const TArray<FData>& InputData)
{
    TArray<FResult> Results;
    Results.Reserve(InputData.Num()); // 预分配避免重新分配
    
    for (const FData& Data : InputData)
    {
        Results.Emplace(ProcessData(Data)); // 使用Emplace避免拷贝
    }
}
```

### 缓存友好的数据结构
```cpp
// ✅ 结构体成员排序 - 减少内存碎片
struct FOptimizedData
{
    // 8字节对齐的成员放在前面
    double LargeValue;
    FVector Position;
    
    // 4字节对齐
    float SmallValue;
    int32 Index;
    
    // 1字节成员放在最后
    bool bIsActive;
    uint8 Flags;
};
```

## 错误处理模式
- 对可能失败的操作使用TResult<SuccessType, ErrorType>
- 遵循UE模式：简单情况使用bool返回值+out参数
- 对可能不存在的值使用TOptional<T>
- 对不可恢复的程序员错误使用check()（仅调试版本）
- 对可恢复的错误使用ensure()，在开发中触发断点
- 当需要在发布版本中执行表达式时使用verify()

## 错误处理最佳实践示例

### ✅ 推荐模式
```cpp
// 1. 简单操作 - bool + out参数
bool GetActorLocation(AActor* Actor, FVector& OutLocation)
{
    if (!IsValid(Actor))
    {
        return false;
    }
    OutLocation = Actor->GetActorLocation();
    return true;
}

// 2. 复杂操作 - TOptional
TOptional<FVector> FindNearestActor(const FVector& Location)
{
    // 实现查找逻辑
    if (bFound)
    {
        return FVector(X, Y, Z);
    }
    return {}; // 空值表示未找到
}

// 3. 可能失败的操作 - 自定义Result类型
struct FLoadResult
{
    bool bSuccess;
    FString ErrorMessage;
    UObject* LoadedObject = nullptr;
};

FLoadResult LoadAssetSafely(const FSoftObjectPath& AssetPath);
```

## 内存管理
- 使用RAII模式进行资源管理
- 实现适当的移动构造函数和赋值操作符
- 对UObject派生类使用UE垃圾回收
- 避免原始指针；使用智能指针或UE对象引用
- 对性能关键的临时对象实现对象池

## 线程和并发
- 使用UE的异步任务系统：Async()和AsyncTask()函数
- 简单锁定使用FCriticalSection，读取密集场景使用FRWLock
- 简单线程安全计数器使用原子操作（TAtomic）
- 优先使用不可变数据结构保证线程安全
- 可用时使用UE的线程安全容器

## 线程安全最佳实践

### UE线程安全容器
```cpp
// ✅ 线程安全的计数器
TAtomic<int32> ThreadSafeCounter{0};

// ✅ 读写锁用于读多写少场景
class FThreadSafeCache
{
private:
    mutable FRWLock CacheLock;
    TMap<FString, FData> CacheData;
    
public:
    TOptional<FData> GetData(const FString& Key) const
    {
        FReadScopeLock ReadLock(CacheLock);
        if (const FData* Found = CacheData.Find(Key))
        {
            return *Found;
        }
        return {};
    }
    
    void SetData(const FString& Key, const FData& Data)
    {
        FWriteScopeLock WriteLock(CacheLock);
        CacheData.Add(Key, Data);
    }
};
```

## 插件架构
- 清晰分离模块：Core（运行时）、Editor（仅编辑器）、Tests（测试）
- 使用接口驱动设计，接口类使用I前缀
- 实现适当的模块启动和关闭序列
- 使用UE子系统框架处理全局服务
- 设计事件系统实现模块间松耦合

## 最优插件目录结构
- 使用以下标准目录结构：
  ```
  MyPlugin/
  ├── Source/
  │   ├── MyPluginCore/          # 核心运行时模块
  │   │   ├── Public/
  │   │   │   ├── MyPluginCore.h
  │   │   │   ├── Interfaces/    # 接口定义
  │   │   │   ├── Data/          # 数据结构
  │   │   │   └── Services/      # 服务类
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

## 模块依赖最佳实践
- Core模块依赖保持最小：仅依赖Core、CoreUObject、Engine
- Editor模块依赖Core模块，添加UnrealEd、EditorStyle、ToolMenus等编辑器依赖
- 在Build.cs中设置正确的PCH和IWYU配置：
  ```cpp
  PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
  bEnforceIWYU = true;
  ```
- 使用适当的模块类型：Runtime、Editor、Developer、ThirdParty
- 实现正确的模块加载阶段：PreDefault、Default、PostEngineInit等

## 插件文件组织规范
- 接口定义放在Public/Interfaces/目录下，使用I前缀命名
- 数据结构放在Public/Data/目录下，使用F前缀命名结构体
- 服务类放在Public/Services/目录下，使用U前缀命名UObject派生类
- 私有实现放在Private/目录下，按功能模块分子目录
- 配置文件放在Config/目录下，使用Default前缀命名
- 编辑器资源（图标等）放在Resources/目录下
- 蓝图和内容资源放在Content/目录下

## 插件模块初始化模式
- 实现IModuleInterface接口的StartupModule()和ShutdownModule()方法
- 在StartupModule()中注册子系统、命令、菜单等
- 在ShutdownModule()中清理资源、注销回调等
- 使用IMPLEMENT_MODULE宏正确导出模块
- 为Editor模块实现延迟初始化，避免启动时阻塞

## 插件子系统设计
- 使用UEngineSubsystem作为全局服务的基类
- 使用UEditorSubsystem作为编辑器专用服务的基类
- 实现Initialize()和Deinitialize()方法管理子系统生命周期
- 提供静态Get()方法便于访问子系统实例
- 使用事件委托实现子系统间的松耦合通信

## 配置和设置
- 使用UDeveloperSettings处理编辑器可配置设置
- 使用适当的Config=说明符实现正确的配置文件组织
- 支持开发设置的热重载
- 为所有配置选项提供合理的默认值
- 使用meta标签生成适当的编辑器UI

## 蓝图集成
- 使用UFUNCTION(BlueprintCallable)标记适当的函数
- 对暴露的变量使用UPROPERTY(BlueprintReadOnly/BlueprintReadWrite)
- 提供有意义的Category和DisplayName meta标签
- 对无副作用的函数使用BlueprintPure
- 实现适当的蓝图节点标题和工具提示

## 本地化和国际化
- 对所有用户可见字符串使用LOCTEXT宏
- 在文件开头定义LOCTEXT_NAMESPACE
- 使用FText::Format进行动态文本格式化
- 避免对本地化文本使用字符串拼接
- 支持带参数的适当文本格式化

## 代码文档
- 使用/** */样式为公共API提供清晰的文档注释
- 为函数文档使用@param和@return标签
- 在复杂API文档中包含使用示例
- 记录公共方法的线程安全保证
- 保持文档简洁但完整

## 要避免的反模式
> **核心原则**: 不要重复造轮子，充分利用UE已有的优化实现

### 容器和数据结构
- 当UE等效容器存在时不要使用STL容器（TArray vs std::vector, TMap vs std::map）
- 不要自定义实现已有的数据结构（如优先队列、哈希表等）
- 不要忽视UE容器的特殊方法（如Emplace、Reserve、Reset等）

### 内存和资源管理
- 当UE系统可以处理时不要手动管理内存
- 不要使用原始指针进行对象所有权管理
- 不要忽视UE的RAII模式和智能指针
- 不要自定义实现对象池，优先使用UE的对象管理系统

### 算法和工具
- 当UE提供优化版本时不要实现自定义算法（数学运算、字符串处理、文件操作等）
- 不要重新实现UE已有的工具类（如FMath、FString、FDateTime等）
- 不要忽视UE的异步和多线程系统，避免自定义线程管理

### 编码规范
- 不要忽视const正确性
- 避免使用过于前沿的C++20特性，优先使用稳定特性
- 不要绕过IWYU要求
- 不要在模块间创建循环依赖
- 不要在UE代码中使用异常
- 不要忽视UE编码标准和命名约定

### 迭代器和模板陷阱
- **不要假设迭代器支持减法**：避免使用 `end - begin`，UE迭代器可能不支持
- **不要直接使用std::distance**：在模板中使用循环计算距离
- **不要忽视函数返回类型**：检查函数是否返回void再用于条件判断
- **不要在模板中使用STL特定操作**：考虑UE迭代器的兼容性

## 测试和调试
- 为关键功能编写自动化测试
- 使用UE内置的性能分析工具（Insights、Stats系统）
- 使用适当的日志类别实现正确的日志记录
- 使用性能计数器监控关键路径
- 使用不同的构建配置进行测试（Debug、Development、Shipping）

## 模块依赖
- 保持Core模块依赖最小化（Core、CoreUObject、Engine）
- Editor模块应该依赖Core模块，而不是相反
- 避免模块间的循环依赖
- 使用适当的加载阶段进行模块初始化
- 记录模块依赖的理由

---

## 🎯 核心开发理念总结

### "不要重复造轮子" - UE已有实现优先原则

**为什么要遵循这个原则？**
1. **性能优化**: UE的实现经过了大量优化和测试
2. **兼容性保证**: 与引擎其他部分完美集成
3. **维护成本**: 减少自定义代码的维护负担
4. **团队协作**: 统一的API让团队成员更容易理解和维护代码
5. **未来兼容**: 跟随引擎更新自动获得改进

**实践检查清单**
- [ ] 在实现新功能前，先查阅UE文档确认是否已有现成实现
- [ ] 优先使用UE容器、智能指针、字符串类型
- [ ] 使用UE的数学库、时间系统、文件系统
- [ ] 采用UE的异步和多线程模式
- [ ] 遵循UE的错误处理和日志系统
- [ ] 使用UE的内存管理和对象生命周期管理

**记住**: 每当你想要自定义实现某个功能时，先问自己："UE是否已经提供了这个功能？"

## C++20特性使用指导
### ✅ 推荐使用的稳定特性
- `constexpr` 增强 - 编译时计算
- `consteval` - 强制编译时求值
- `constinit` - 编译时初始化
- `std::span` - 安全的数组视图（需要UE支持确认）
- 概念（Concepts）- 模板约束（谨慎使用）

### ⚠️ 谨慎使用的特性
- 协程（Coroutines）- UE有自己的异步系统
- 模块（Modules）- UE使用传统头文件系统
- `std::format` - 使用UE的FString::Printf

### ❌ 避免使用的特性
- `std::jthread` - 使用UE的FRunnable
- `std::atomic_ref` - 使用UE的TAtomic





