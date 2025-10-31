# PointSampling 模块设计方案

## 📋 设计目标

将XToolsLibrary中2000+行的泊松采样代码拆分为独立模块，遵循单一职责原则，便于未来扩展更多采样算法。

---

## 🏗️ 推荐的模块结构

### 方案一：独立模块（推荐）✅

```
UE_XTools/
├── Source/
│   ├── PointSampling/              # 新增独立模块
│   │   ├── PointSampling.Build.cs
│   │   ├── Public/
│   │   │   ├── PointSamplingTypes.h        # 公共类型定义
│   │   │   ├── PointSamplingLibrary.h      # 蓝图函数库（主接口）
│   │   │   │
│   │   │   ├── Algorithms/                 # 算法接口
│   │   │   │   ├── PoissonDiskSampling.h   # 泊松圆盘采样
│   │   │   │   ├── HaltonSequence.h        # Halton序列（未来）
│   │   │   │   ├── BlueNoiseSampling.h     # 蓝噪声采样（未来）
│   │   │   │   └── StratifiedSampling.h    # 分层采样（未来）
│   │   │   │
│   │   │   └── Core/                       # 核心系统
│   │   │       ├── SamplingCache.h         # 缓存系统
│   │   │       └── SamplingHelpers.h       # 通用辅助函数
│   │   │
│   │   └── Private/
│   │       ├── PointSamplingLibrary.cpp    # 蓝图函数库实现
│   │       │
│   │       ├── Algorithms/
│   │       │   ├── PoissonDiskSampling.cpp
│   │       │   └── ...
│   │       │
│   │       └── Core/
│   │           ├── SamplingCache.cpp
│   │           └── SamplingHelpers.cpp
│   │
│   └── XTools/                     # 原模块
│       └── ... (移除泊松采样代码)
```

**优点**：
- ✅ 完全独立，职责清晰
- ✅ 易于测试和维护
- ✅ 可单独编译和分发
- ✅ 符合UE插件最佳实践

**缺点**：
- 需要修改插件配置
- 增加模块数量

---

### 方案二：子文件夹组织（简化版）

如果不想增加模块，可以在XTools内部重组：

```
XTools/
├── Public/
│   ├── Sampling/                           # 采样功能
│   │   ├── PointSamplingTypes.h
│   │   ├── PointSamplingLibrary.h
│   │   ├── PoissonDiskSampling.h
│   │   └── SamplingCache.h
│   │
│   ├── XToolsLibrary.h                     # 保留其他功能
│   └── ...
│
└── Private/
    ├── Sampling/
    │   ├── PointSamplingLibrary.cpp
    │   ├── PoissonDiskSampling.cpp
    │   ├── PoissonSamplingHelpers.cpp
    │   └── SamplingCache.cpp
    │
    ├── XToolsLibrary.cpp                   # 移除采样代码
    └── ...
```

**优点**：
- ✅ 实施简单
- ✅ 不增加模块数量

**缺点**：
- 仍在同一模块内
- 编译依赖未完全隔离

---

## 📁 详细文件设计（方案一）

### 1. **PointSamplingTypes.h** - 公共类型定义

```cpp
#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.generated.h"

/** 采样坐标空间类型 */
UENUM(BlueprintType)
enum class ESamplingCoordinateSpace : uint8
{
    /** 世界空间 - 返回世界绝对坐标 */
    World       UMETA(DisplayName = "世界空间"),
    
    /** 局部空间 - 返回相对于采样区域中心的坐标 */
    Local       UMETA(DisplayName = "局部空间"),
    
    /** 原始空间 - 返回算法原始输出 */
    Raw         UMETA(DisplayName = "原始空间")
};

/** 采样算法类型 */
UENUM(BlueprintType)
enum class ESamplingAlgorithmType : uint8
{
    /** 泊松圆盘采样 - 均匀分布，保证最小距离 */
    PoissonDisk         UMETA(DisplayName = "泊松圆盘"),
    
    /** Halton序列 - 低差异序列 */
    Halton              UMETA(DisplayName = "Halton序列"),
    
    /** 蓝噪声采样 - 高质量随机分布 */
    BlueNoise           UMETA(DisplayName = "蓝噪声"),
    
    /** 分层采样 - 网格化均匀分布 */
    Stratified          UMETA(DisplayName = "分层采样")
};

/** 采样配置基类 */
USTRUCT(BlueprintType)
struct POINTSAMPLING_API FSamplingConfig
{
    GENERATED_BODY()

    /** 随机种子（0表示随机） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
    int32 RandomSeed = 0;

    /** 是否启用缓存 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling|Performance")
    bool bUseCache = true;

    /** 坐标空间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling")
    ESamplingCoordinateSpace CoordinateSpace = ESamplingCoordinateSpace::Local;
};

/** 泊松采样配置 */
USTRUCT(BlueprintType)
struct POINTSAMPLING_API FPoissonSamplingConfig : public FSamplingConfig
{
    GENERATED_BODY()

    /** 点之间的最小距离（<=0时根据目标点数计算） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson")
    float MinDistance = 50.0f;

    /** 目标点数量（<=0时由MinDistance控制） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson")
    int32 TargetPointCount = 0;

    /** 最大尝试次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Advanced")
    int32 MaxAttempts = 30;

    /** 扰动强度 0-1 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Advanced", meta = (ClampMin = "0", ClampMax = "1"))
    float JitterStrength = 0.0f;
};
```

---

### 2. **PoissonDiskSampling.h** - 泊松采样算法

```cpp
#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

/**
 * 泊松圆盘采样算法实现
 * 
 * 提供2D/3D泊松圆盘采样，确保点之间保持最小距离的同时均匀分布
 */
class POINTSAMPLING_API FPoissonDiskSampling
{
public:
    /**
     * 2D泊松圆盘采样
     * @param Width 采样区域宽度
     * @param Height 采样区域高度
     * @param Config 采样配置
     * @return 生成的2D点数组
     */
    static TArray<FVector2D> Generate2D(
        float Width,
        float Height,
        const FPoissonSamplingConfig& Config
    );

    /**
     * 3D泊松圆盘采样
     * @param Extent 采样区域半尺寸
     * @param Config 采样配置
     * @return 生成的3D点数组
     */
    static TArray<FVector> Generate3D(
        const FVector& Extent,
        const FPoissonSamplingConfig& Config
    );

    /**
     * 在盒体内采样（自动2D/3D）
     * @param BoxExtent 盒体半尺寸
     * @param Transform 变换
     * @param Config 采样配置
     * @return 生成的点数组
     */
    static TArray<FVector> GenerateInBox(
        const FVector& BoxExtent,
        const FTransform& Transform,
        const FPoissonSamplingConfig& Config
    );

    /**
     * 使用随机流采样（确定性）
     */
    static TArray<FVector> GenerateInBoxFromStream(
        const FRandomStream& RandomStream,
        const FVector& BoxExtent,
        const FTransform& Transform,
        const FPoissonSamplingConfig& Config
    );

private:
    // 内部辅助函数
    static float CalculateRadiusFromTargetCount(int32 TargetCount, float Width, float Height, float Depth, bool bIs2D);
    static void AdjustToTargetCount(TArray<FVector>& Points, int32 TargetCount, const FVector& BoxSize, float Radius, bool bIs2D);
    static void ApplyJitter(TArray<FVector>& Points, float Radius, float JitterStrength);
    static void ApplyTransform(TArray<FVector>& Points, const FTransform& Transform, ESamplingCoordinateSpace Space);
};
```

---

### 3. **SamplingCache.h** - 缓存系统

```cpp
#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

/**
 * 采样结果缓存系统
 * 
 * 使用LRU策略管理采样结果缓存，提升性能
 */
class POINTSAMPLING_API FSamplingCache
{
public:
    /** 获取单例 */
    static FSamplingCache& Get();

    /** 获取缓存结果 */
    template<typename TConfig>
    TOptional<TArray<FVector>> GetCached(const TConfig& Config);

    /** 存储结果到缓存 */
    template<typename TConfig>
    void Store(const TConfig& Config, const TArray<FVector>& Points);

    /** 清空缓存 */
    void ClearCache();

    /** 获取缓存统计 */
    void GetStats(int32& OutHits, int32& OutMisses, int32& OutEntries) const;

private:
    FSamplingCache() = default;
    
    // LRU缓存实现
    struct FCacheEntry
    {
        TArray<FVector> Points;
        double LastAccessTime;
    };

    void RemoveLRUEntry();

    static constexpr int32 MaxCacheSize = 50;
    
    FCriticalSection CacheLock;
    TMap<uint32, FCacheEntry> Cache;  // Hash -> Entry
    int32 CacheHits = 0;
    int32 CacheMisses = 0;
};
```

---

### 4. **PointSamplingLibrary.h** - 蓝图函数库

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PointSamplingTypes.h"
#include "PointSamplingLibrary.generated.h"

class UBoxComponent;

/**
 * 点采样蓝图函数库
 * 
 * 提供各种点阵采样算法的蓝图接口
 */
UCLASS()
class POINTSAMPLING_API UPointSamplingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ==================== 泊松圆盘采样 ====================
    
    /**
     * 在盒体内生成泊松采样点
     */
    UFUNCTION(BlueprintCallable, Category = "PointSampling|Poisson",
        meta = (DisplayName = "在盒体内生成泊松采样点",
               AdvancedDisplay = "Config"))
    static TArray<FVector> GeneratePoissonPointsInBox(
        UBoxComponent* BoundingBox,
        FPoissonSamplingConfig Config = FPoissonSamplingConfig()
    );

    /**
     * 在范围内生成泊松采样点（向量版）
     */
    UFUNCTION(BlueprintCallable, Category = "PointSampling|Poisson",
        meta = (DisplayName = "在范围内生成泊松采样点（向量版）"))
    static TArray<FVector> GeneratePoissonPointsByVector(
        FVector BoxExtent,
        FTransform Transform,
        FPoissonSamplingConfig Config = FPoissonSamplingConfig()
    );

    /**
     * 2D泊松采样
     */
    UFUNCTION(BlueprintCallable, Category = "PointSampling|Poisson",
        meta = (DisplayName = "生成泊松采样点2D"))
    static TArray<FVector2D> GeneratePoissonPoints2D(
        float Width = 1000.0f,
        float Height = 1000.0f,
        float MinDistance = 50.0f,
        int32 MaxAttempts = 15
    );

    /**
     * 3D泊松采样
     */
    UFUNCTION(BlueprintCallable, Category = "PointSampling|Poisson",
        meta = (DisplayName = "生成泊松采样点3D"))
    static TArray<FVector> GeneratePoissonPoints3D(
        float Width = 1000.0f,
        float Height = 1000.0f,
        float Depth = 1000.0f,
        float MinDistance = 50.0f,
        int32 MaxAttempts = 30
    );

    // ==================== 缓存管理 ====================
    
    /**
     * 清理采样缓存
     */
    UFUNCTION(BlueprintCallable, Category = "PointSampling|Cache",
        meta = (DisplayName = "清理点采样缓存"))
    static FString ClearSamplingCache();

    // ==================== 未来扩展接口 ====================
    
    /**
     * Halton序列采样（未来实现）
     */
    UFUNCTION(BlueprintCallable, Category = "PointSampling|Halton",
        meta = (DisplayName = "生成Halton序列点", DevelopmentOnly))
    static TArray<FVector> GenerateHaltonSequence(
        int32 PointCount,
        const FVector& Extent
    );
};
```

---

### 5. **PointSampling.Build.cs** - 模块配置

```cpp
using UnrealBuildTool;

public class PointSampling : ModuleRules
{
    public PointSampling(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // ✅ C++20标准
        CppStandard = CppStandardVersion.Default;
        
        // ✅ IWYU强制执行
        IWYUSupport = IWYUSupport.Full;
        
        // ✅ 开发配置
        bUseUnity = false;
        bEnableExceptions = false;
        bUseRTTI = false;
        
        PublicIncludePaths.Add(ModuleDirectory + "/Public");
        PrivateIncludePaths.Add(ModuleDirectory + "/Private");
        
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
        
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // 如果需要的话添加
        });
    }
}
```

---

## 🔄 迁移步骤

### 第一阶段：创建新模块

1. **创建模块目录结构**
   ```bash
   mkdir -p Source/PointSampling/Public/{Algorithms,Core}
   mkdir -p Source/PointSampling/Private/{Algorithms,Core}
   ```

2. **创建Build.cs文件**
   - 复制上面的PointSampling.Build.cs

3. **更新插件描述符**
   ```json
   // XTools.uplugin
   {
       "Modules": [
           {
               "Name": "PointSampling",
               "Type": "Runtime",
               "LoadingPhase": "Default"
           },
           // ... 其他模块
       ]
   }
   ```

---

### 第二阶段：提取代码

1. **提取类型定义**
   - 从XToolsLibrary.h提取EPoissonCoordinateSpace等
   - 移动到PointSamplingTypes.h

2. **提取缓存系统**
   - 从XToolsLibrary.cpp提取FPoissonResultCache
   - 重构为FSamplingCache

3. **提取算法实现**
   - 提取PoissonSamplingHelpers命名空间
   - 重构为FPoissonDiskSampling类

4. **创建蓝图函数库**
   - 提取泊松采样相关的UFUNCTION
   - 移动到UPointSamplingLibrary

---

### 第三阶段：清理和测试

1. **更新XToolsLibrary**
   - 移除泊松采样代码
   - 添加#include "PointSampling/PointSamplingLibrary.h"（如果需要兼容）

2. **更新依赖**
   - XTools.Build.cs添加对PointSampling的依赖

3. **编译测试**
   - 重新生成项目文件
   - 编译验证

4. **蓝图测试**
   - 验证所有蓝图节点仍然可用
   - 测试功能正确性

---

## 📊 代码行数预估

| 文件 | 预估行数 | 说明 |
|------|---------|------|
| PointSamplingTypes.h | 100-150 | 类型定义 |
| PoissonDiskSampling.h | 80-100 | 接口声明 |
| PoissonDiskSampling.cpp | 800-1000 | 核心算法 |
| SamplingCache.h | 60-80 | 缓存接口 |
| SamplingCache.cpp | 150-200 | 缓存实现 |
| PointSamplingLibrary.h | 100-120 | 蓝图接口 |
| PointSamplingLibrary.cpp | 200-300 | 蓝图实现 |
| **总计** | **~1500-2000行** | 与原代码相当 |

---

## 🎯 未来扩展计划

### 近期（v1.9）
- ✅ 完成泊松采样拆分
- ✅ 优化缓存系统

### 中期（v2.0）
- 🔲 实现Halton序列采样
- 🔲 实现分层采样
- 🔲 添加采样可视化工具

### 远期（v2.1+）
- 🔲 蓝噪声采样
- 🔲 Wang Tiles采样
- 🔲 GPU加速采样

---

## ✅ 优势总结

### 代码质量
- ✅ **单一职责**：每个类只负责一个功能
- ✅ **易于测试**：独立模块便于单元测试
- ✅ **清晰层次**：接口、实现、辅助分层清晰

### 可维护性
- ✅ **减少耦合**：模块间依赖明确
- ✅ **便于查找**：功能分类清晰
- ✅ **易于扩展**：新算法只需添加新文件

### 性能
- ✅ **独立编译**：修改采样算法不影响其他模块
- ✅ **按需加载**：可选择性加载模块
- ✅ **编译优化**：IWYU减少编译依赖

---

## 📝 注意事项

1. **向后兼容**
   - 如果有现有蓝图使用旧接口，考虑保留桥接代码
   - 使用UE的DeprecatedFunction标记

2. **命名空间**
   - 建议使用PointSampling命名空间避免冲突

3. **文档更新**
   - 同步更新README.md
   - 添加API文档

4. **版本管理**
   - 建议创建新分支进行重构
   - 完成后合并到主分支

---

**推荐实施方案一（独立模块）**，这样结构最清晰，最符合UE插件最佳实践！

