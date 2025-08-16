# 🚀 UE对象池测试 - 快速参考

## 💡 **核心问题**
在UE测试环境中，`UObjectPoolSubsystem::GetGlobal()` 经常返回 `nullptr`

## ✅ **解决方案**
使用 `FObjectPoolTestAdapter` 智能适配器，自动处理环境差异

## 📋 **快速使用模板**

### 基础测试模板
```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYourTest, "Category.TestName", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FYourTest::RunTest(const FString& Parameters)
{
    // 1️⃣ 初始化
    FObjectPoolTestAdapter::Initialize();
    
    // 2️⃣ 注册Actor类
    bool bRegistered = FObjectPoolTestAdapter::RegisterActorClass(AActor::StaticClass(), 5);
    TestTrue("注册应该成功", bRegistered);
    
    // 3️⃣ 使用对象池
    AActor* Actor = FObjectPoolTestAdapter::SpawnActorFromPool(AActor::StaticClass());
    TestTrue("生成应该成功", IsValid(Actor));
    
    if (Actor)
    {
        bool bReturned = FObjectPoolTestAdapter::ReturnActorToPool(Actor);
        TestTrue("归还应该成功", bReturned);
    }
    
    // 4️⃣ 清理
    FObjectPoolTestAdapter::Cleanup();
    
    return true;
}
```

### 环境检测模板
```cpp
// 检测当前环境
FObjectPoolTestAdapter::Initialize();
auto Environment = FObjectPoolTestAdapter::GetCurrentEnvironment();

switch (Environment)
{
case FObjectPoolTestAdapter::ETestEnvironment::Subsystem:
    AddInfo(TEXT("✅ 子系统模式 - 完整功能"));
    break;
case FObjectPoolTestAdapter::ETestEnvironment::DirectPool:
    AddInfo(TEXT("⚠️ 直接池模式 - 核心功能"));
    break;
case FObjectPoolTestAdapter::ETestEnvironment::Simulation:
    AddInfo(TEXT("🔧 模拟模式 - 基础验证"));
    break;
}
```

## 🔧 **常用API**

| 方法 | 功能 | 返回值 |
|------|------|--------|
| `Initialize()` | 初始化适配器 | void |
| `Cleanup()` | 清理资源 | void |
| `RegisterActorClass(Class, Size)` | 注册Actor类 | bool |
| `SpawnActorFromPool(Class)` | 生成Actor | AActor* |
| `ReturnActorToPool(Actor)` | 归还Actor | bool |
| `GetCurrentEnvironment()` | 获取环境类型 | ETestEnvironment |
| `GetPoolStats(Class)` | 获取统计信息 | FObjectPoolStats |

## 🎯 **必须包含的头文件**
```cpp
#include "ObjectPoolTestAdapter.h"
```

## 🚨 **常见错误和解决方案**

### ❌ 错误做法
```cpp
// 直接调用子系统 - 在测试环境中可能失败
UObjectPoolSubsystem* Subsystem = UObjectPoolSubsystem::GetGlobal();
if (Subsystem) // 可能为nullptr
{
    Subsystem->RegisterActorClass(ActorClass, 5);
}
```

### ✅ 正确做法
```cpp
// 使用适配器 - 自动处理环境差异
FObjectPoolTestAdapter::Initialize();
bool bSuccess = FObjectPoolTestAdapter::RegisterActorClass(ActorClass, 5);
TestTrue("注册应该成功", bSuccess);
```

## 🔍 **调试测试**

### 运行环境诊断
1. 打开 `Window > Developer Tools > Session Frontend`
2. 找到 `XTools.ObjectPool.EnvironmentDetection`
3. 运行测试查看环境类型

### 查看详细诊断
运行 `XTools.ObjectPool.SubsystemAvailability` 获取详细信息

## 📊 **环境类型说明**

| 环境 | 说明 | 性能 | 功能 |
|------|------|------|------|
| **Subsystem** | 子系统可用 | 最佳 | 完整 |
| **DirectPool** | 直接池管理 | 良好 | 核心 |
| **Simulation** | 模拟模式 | 基础 | 验证 |

## ⚡ **性能提示**

1. **预分配池大小** - 根据测试需求设置合适的初始大小
2. **批量操作** - 一次注册多个Actor类
3. **及时清理** - 测试结束后调用Cleanup()
4. **复用实例** - 在测试间复用Actor

## 🎨 **最佳实践**

### ✅ DO (推荐做法)
- 总是使用适配器而不是直接调用子系统
- 在测试开始时调用Initialize()
- 在测试结束时调用Cleanup()
- 检查所有返回值
- 使用详细的测试日志

### ❌ DON'T (避免做法)
- 不要直接调用UObjectPoolSubsystem::GetGlobal()
- 不要忘记清理资源
- 不要忽略返回值检查
- 不要在预处理器块中使用UCLASS
- 不要假设子系统总是可用

## 📞 **快速故障排除**

| 问题 | 检查项 | 解决方案 |
|------|--------|----------|
| 编译错误 | 头文件包含 | 添加 `#include "ObjectPoolTestAdapter.h"` |
| 注册失败 | 环境类型 | 运行环境检测测试 |
| 生成失败 | Actor类有效性 | 检查ActorClass是否为nullptr |
| 归还失败 | Actor有效性 | 检查Actor是否为IsValid |

## 🔗 **相关文件**

- **核心实现**: `ObjectPoolTestAdapter.h/cpp`
- **测试示例**: `ExampleUsage.cpp`
- **环境测试**: `ObjectPoolEnvironmentTest.cpp`
- **详细文档**: `SOLUTION_TestEnvironment.md`
- **使用指南**: `README_TestEnvironment.md`

---

**记住**: 使用适配器让你的测试在任何环境下都能稳定运行！🎯
