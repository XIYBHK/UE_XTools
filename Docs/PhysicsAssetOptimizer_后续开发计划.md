# PhysicsAssetOptimizer 后续开发计划

> 本文档记录 PhysicsAssetOptimizer 模块的后续开发任务和实现方案

---

## 当前实现状态

### ✅ 已完成
- ✅ 模块架构搭建（Editor Only, Win64）
- ✅ 三通道骨骼识别系统（命名规则 + 拓扑链 + 几何校验占位符）
- ✅ P0 规则完全实现（Rule01/04/06/07）
- ✅ P1 规则完全实现（Rule02/03/05/12）
- ✅ P2 规则部分实现（Rule08/09 完成，Rule10/11 占位符）
- ✅ 核心 API 设计（C++ 调用接口）
- ✅ 性能统计系统

### ⏸️ 未完成
- ⏸️ 编辑器 UI 集成
- ⏸️ Rule10 (LSV 生成) 完整实现
- ⏸️ Rule11 (长骨凸包) 完整实现
- ⏸️ 骨骼识别通道3（几何校验）
- ⏸️ 蓝图 API 暴露
- ⏸️ 性能测试和验证
- ⏸️ 用户文档

---

## 📋 任务清单

### 阶段 1: 编辑器 UI 集成（高优先级）

#### 1.1 扩展 Physics Asset Editor 工具栏

**目标**: 在物理资产编辑器中添加"一键优化"按钮

**实现方案**:
```cpp
// 文件: Source/PhysicsAssetOptimizer/Private/PhysicsAssetEditorExtension.h
class FPhysicsAssetEditorExtension
{
public:
    static void RegisterMenus();
    static void UnregisterMenus();

private:
    static void OnOptimizePhysicsAsset();
    static TSharedRef<SWidget> GenerateOptimizeMenuContent();
};

// 在 PhysicsAssetOptimizer.cpp 的 StartupModule() 中注册
void FPhysicsAssetOptimizerModule::StartupModule()
{
    FPhysicsAssetEditorExtension::RegisterMenus();
}
```

**依赖模块**:
- `ToolMenus` - 扩展工具栏
- `PhysicsAssetEditor` - 获取当前编辑的物理资产

**预计工作量**: 4-6 小时

**参考资料**:
- `Plugins/Editor/AssetManagerEditor` - 工具栏扩展示例
- `Engine/Source/Editor/PhysicsAssetEditor` - 物理资产编辑器源码

---

#### 1.2 创建优化配置面板

**目标**: Slate UI 面板，允许用户自定义优化参数

**UI 设计**:
```
┌─────────────────────────────────────────┐
│  物理资产优化设置                        │
├─────────────────────────────────────────┤
│  基础设置                                │
│  ☑ 移除小骨骼 (< 8.0 cm)                │
│  ☑ 配置阻尼 (Linear: 0.2, Angular: 0.8) │
│  ☑ 禁用核心碰撞                         │
│                                          │
│  高级设置                                │
│  ☑ 优化形状                             │
│  ☑ 质量分布 (基础质量: 80 kg)           │
│  ☑ 约束限制                             │
│  ☑ Sleep 优化 (阈值: 0.05)              │
│                                          │
│  实验性功能                              │
│  ☐ 生成 LSV (分辨率: 64)                │
│  ☑ 长骨凸包 (阈值: 40 cm)               │
│                                          │
│  [预览优化结果]  [应用优化]  [取消]     │
└─────────────────────────────────────────┘
```

**实现文件**:
```cpp
// Source/PhysicsAssetOptimizer/Private/Slate/SPhysicsOptimizerPanel.h
class SPhysicsOptimizerPanel : public SCompoundWidget
{
    SLATE_BEGIN_ARGS(SPhysicsOptimizerPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UPhysicsAsset* InPhysicsAsset);

private:
    FPhysicsOptimizerSettings Settings;
    TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;

    FReply OnOptimizeClicked();
    FReply OnPreviewClicked();
    FReply OnCancelClicked();
};
```

**预计工作量**: 8-12 小时

**参考资料**:
- `Plugins/Runtime/GeometryProcessing` - Slate 配置面板示例
- UE Slate UI 文档

---

#### 1.3 实现优化预览功能

**目标**: 在应用优化前显示预计变化

**功能**:
- 预计移除的骨骼列表
- 预计禁用的碰撞对数量
- 预计质量分布变化

**实现方案**:
```cpp
// 新增函数到 FPhysicsOptimizerCore
struct FPhysicsOptimizationPreview
{
    TArray<FName> BonesToRemove;
    TArray<TPair<FName, FName>> CollisionPairsToDisable;
    TMap<FName, float> MassChanges;
    int32 EstimatedCollisionPairReduction;
};

static FPhysicsOptimizationPreview PreviewOptimization(
    UPhysicsAsset* PA,
    USkeletalMesh* Mesh,
    const FPhysicsOptimizerSettings& Settings
);
```

**预计工作量**: 6-8 小时

---

### 阶段 2: 高级功能完善（中优先级）

#### 2.1 完整实现 Rule10 - LSV 生成

**当前状态**: 占位符（需要 LevelSetHelpers Private API）

**实现步骤**:

1. **启用 LevelSetHelpers 头文件**
```cpp
// PhysicsOptimizerCore.cpp
#if WITH_EDITOR
    #include "LevelSetHelpers.h"  // 取消注释
#endif
```

2. **实现袖口骨骼识别**
```cpp
// BoneIdentificationSystem.cpp
bool FBoneIdentificationSystem::IsCuffBone(USkeletalMesh* Mesh, int32 BoneIndex)
{
    // 检查骨骼长度 < 5cm
    // 检查法向突变 > 60°
    // 基于顶点法向量分析
    return false;
}
```

3. **调用 LSV 生成 API**
```cpp
// PhysicsOptimizerCore.cpp - Rule10
void FPhysicsOptimizerCore::Rule10_GenerateLSV(...)
{
    for (const FName& CuffBone : CuffBones)
    {
        const int32 BodyIndex = PA->FindBodyIndex(CuffBone);
        if (BodyIndex != INDEX_NONE)
        {
            // 调用内部 API
            LevelSetHelpers::CreateLevelSetForBone(
                PA,
                Mesh,
                CuffBone,
                Resolution
            );
        }
    }
}
```

**风险**:
- LevelSetHelpers 是 Private API，可能在未来版本变更
- 需要测试 UE 5.3-5.7 兼容性

**预计工作量**: 12-16 小时

**参考资料**:
- `D:\Program Files\Epic Games\UE_5.5\Engine\Source\Developer\PhysicsUtilities\Private\LevelSetHelpers.h`
- UE Chaos 物理文档

---

#### 2.2 完整实现 Rule11 - 长骨凸包

**当前状态**: 占位符（需要网格体顶点数据访问）

**实现步骤**:

1. **获取骨骼影响的顶点**
```cpp
// 新增辅助函数
TArray<FVector> FPhysicsOptimizerCore::GetBoneInfluencedVertices(
    USkeletalMesh* Mesh,
    int32 BoneIndex,
    float InfluenceThreshold = 0.5f
)
{
    TArray<FVector> Vertices;

    FSkeletalMeshModel* MeshModel = Mesh->GetImportedModel();
    if (!MeshModel || MeshModel->LODModels.Num() == 0)
    {
        return Vertices;
    }

    const FSkeletalMeshLODModel& LOD0 = MeshModel->LODModels[0];

    // 遍历 Soft Vertices，筛选受此骨骼影响的顶点
    for (const FSkelMeshSection& Section : LOD0.Sections)
    {
        for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
        {
            for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; ++i)
            {
                if (Vertex.InfluenceBones[i] == BoneIndex &&
                    Vertex.InfluenceWeights[i] > InfluenceThreshold * 255)
                {
                    Vertices.Add(Vertex.Position);
                    break;
                }
            }
        }
    }

    return Vertices;
}
```

2. **计算顶点凸包**
```cpp
// 使用 GeometryCore 模块的凸包算法
#include "CompGeom/ConvexHull3.h"

void FPhysicsOptimizerCore::Rule11_UseConvexForLongBones(...)
{
    for (USkeletalBodySetup* BodySetup : PA->SkeletalBodySetups)
    {
        // ... 判断是否为长骨 ...

        if (BoneLength > LengthThreshold)
        {
            TArray<FVector> Vertices = GetBoneInfluencedVertices(Mesh, BoneIndex);

            if (Vertices.Num() > 4)
            {
                // 计算凸包
                UE::Geometry::FConvexHull3d ConvexHull;
                ConvexHull.Solve(Vertices);

                // 创建 FKConvexElem
                FKConvexElem ConvexElem;
                ConvexElem.VertexData = ConvexHull.GetVertices();

                // 替换原有形状
                BodySetup->AggGeom.SphylElems.Empty();
                BodySetup->AggGeom.ConvexElems.Add(ConvexElem);
            }
        }
    }
}
```

**依赖模块**:
- `GeometryCore` - 凸包算法

**预计工作量**: 16-20 小时

**参考资料**:
- `Engine/Source/Runtime/GeometryCore/Public/CompGeom/ConvexHull3.h`
- `PhysicsAssetUtils::CreateCollisionFromBone()`

---

#### 2.3 完善骨骼识别通道3（几何校验）

**目标**: 使用顶点数据验证骨骼类型识别准确性

**实现方案**:
```cpp
// BoneIdentificationSystem.cpp
void FBoneIdentificationSystem::ValidateWithGeometry(
    USkeletalMesh* Mesh,
    TMap<FName, EBoneType>& InOutBoneTypes)
{
    // 1. 头部验证：包围盒宽高比接近 1
    for (auto& Pair : InOutBoneTypes)
    {
        if (Pair.Value == EBoneType::Head)
        {
            FBox BoundingBox = CalculateBoneBoundingBox(Mesh, Pair.Key);
            float AspectRatio = BoundingBox.GetSize().X / BoundingBox.GetSize().Z;

            // 头部宽高比应在 0.8-1.2 范围
            if (AspectRatio < 0.6f || AspectRatio > 1.4f)
            {
                // 可能识别错误，标记为 Unknown
                Pair.Value = EBoneType::Unknown;
            }
        }
    }

    // 2. 袖口验证：法向突变检测
    // 3. 尾巴验证：方向向量检测
}

FBox FBoneIdentificationSystem::CalculateBoneBoundingBox(
    USkeletalMesh* Mesh,
    FName BoneName)
{
    // 获取受此骨骼影响的顶点，计算包围盒
    TArray<FVector> Vertices = GetBoneInfluencedVertices(Mesh, BoneName);
    return FBox(Vertices);
}
```

**预计工作量**: 8-10 小时

---

### 阶段 3: 蓝图 API 暴露（低优先级）

**目标**: 允许蓝图脚本调用优化功能

**实现方案**:
```cpp
// Source/PhysicsAssetOptimizer/Public/PhysicsOptimizerBlueprintLibrary.h
UCLASS()
class PHYSICSASSETOPTIMIZER_API UPhysicsOptimizerBlueprintLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    /**
     * 优化物理资产
     * @param PhysicsAsset 要优化的物理资产
     * @param SkeletalMesh 对应的骨骼网格体
     * @param Settings 优化设置
     * @param OutStats 输出统计信息
     * @return 是否优化成功
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|PhysicsAsset|优化",
        meta = (
            DisplayName = "优化物理资产",
            Keywords = "物理,Physics,优化,Optimize,Ragdoll",
            ToolTip = "使用预设规则自动优化物理资产，减少碰撞对、配置阻尼和质量分布"
        ))
    static bool OptimizePhysicsAsset(
        UPARAM(DisplayName="物理资产") UPhysicsAsset* PhysicsAsset,
        UPARAM(DisplayName="骨骼网格体") USkeletalMesh* SkeletalMesh,
        UPARAM(DisplayName="优化设置") const FPhysicsOptimizerSettings& Settings,
        UPARAM(DisplayName="统计信息") FPhysicsOptimizerStats& OutStats
    );
};
```

**注意事项**:
- 需要将 `FPhysicsOptimizerSettings` 和 `FPhysicsOptimizerStats` 标记为 `BlueprintType`
- 仅在 Editor 模式可用（运行时禁用）

**预计工作量**: 2-4 小时

---

### 阶段 4: 性能测试和验证（高优先级）

#### 4.1 自动化测试

**目标**: 创建自动化测试用例，验证优化效果

**测试用例**:
```cpp
// Source/PhysicsAssetOptimizer/Tests/PhysicsOptimizerTest.cpp
#include "Misc/AutomationTest.h"
#include "PhysicsOptimizerCore.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPhysicsOptimizerBasicTest,
    "XTools.PhysicsAssetOptimizer.BasicOptimization",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FPhysicsOptimizerBasicTest::RunTest(const FString& Parameters)
{
    // 1. 加载测试物理资产
    UPhysicsAsset* PA = LoadObject<UPhysicsAsset>(
        nullptr,
        TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin_PhysicsAsset")
    );
    TestNotNull(TEXT("物理资产加载"), PA);

    // 2. 执行优化
    FPhysicsOptimizerSettings Settings;
    FPhysicsOptimizerStats Stats;

    bool bSuccess = FPhysicsOptimizerCore::OptimizePhysicsAsset(
        PA, Mesh, Settings, Stats
    );
    TestTrue(TEXT("优化执行成功"), bSuccess);

    // 3. 验证结果
    TestTrue(TEXT("碰撞对减少"),
        Stats.FinalCollisionPairs < Stats.OriginalCollisionPairs);
    TestTrue(TEXT("优化耗时 < 1秒"),
        Stats.OptimizationTimeMs < 1000.0);

    return true;
}
```

**预计工作量**: 8-12 小时

---

#### 4.2 真实角色测试

**测试对象**:
- Mixamo Ybot/Xbot 角色
- UE Mannequin
- 项目中的角色资产

**测试指标**:
| 指标 | 优化前 | 优化后 | 目标 |
|------|--------|--------|------|
| Body 数量 | ~30 | ~20 | 减少 30%+ |
| 碰撞对数量 | ~892 | ~214 | 减少 60%+ |
| Settle Time (s) | ~1.8 | ~0.3 | < 0.5s |
| CPU 占用 (ms/frame) | ~2.5 | ~0.8 | 减少 50%+ |

**测试步骤**:
1. 记录优化前物理资产性能基线
2. 执行优化
3. 在编辑器中测试 Ragdoll 效果（Physics Asset Editor 模拟模式）
4. 在运行时测试（创建多个 Ragdoll 实例）
5. 使用 Unreal Insights 分析性能

**预计工作量**: 12-16 小时

---

### 阶段 5: 文档完善（中优先级）

#### 5.1 用户手册

**文件**: `Docs/PhysicsAssetOptimizer_用户手册.md`

**内容大纲**:
```markdown
# PhysicsAssetOptimizer 用户手册

## 1. 功能概述
- 什么是物理资产优化
- 优化的必要性
- 预期效果

## 2. 快速开始
- 在编辑器中打开物理资产
- 点击"优化"按钮
- 查看优化结果

## 3. 优化规则说明
- P0 规则详解
- P1 规则详解
- P2 规则详解

## 4. 参数配置
- 基础设置
- 高级设置
- 实验性功能

## 5. 最佳实践
- 推荐配置
- 常见问题
- 性能优化建议

## 6. 故障排除
- 优化后碰撞异常
- 优化后质量分布不合理
- 骨骼识别错误
```

**预计工作量**: 6-8 小时

---

#### 5.2 技术文档

**文件**: `Docs/PhysicsAssetOptimizer_技术文档.md`

**内容大纲**:
```markdown
# PhysicsAssetOptimizer 技术文档

## 1. 架构设计
- 模块依赖关系
- 核心类设计
- 数据流

## 2. 骨骼识别系统
- 三通道融合算法
- 骨骼类型定义
- 识别准确率

## 3. 优化规则实现
- 每条规则的详细实现
- API 使用示例
- 性能优化技巧

## 4. 扩展开发
- 添加自定义规则
- 修改骨骼识别逻辑
- 集成到自定义工具

## 5. API 参考
- FPhysicsOptimizerCore
- FBoneIdentificationSystem
- FPhysicsOptimizerSettings
```

**预计工作量**: 8-10 小时

---

#### 5.3 编辑器截图和演示视频

**内容**:
- 优化前后对比截图
- 编辑器 UI 操作截图
- 优化参数配置截图
- 性能对比图表
- 操作流程演示视频（2-3 分钟）

**工具**:
- Unreal Insights（性能分析）
- OBS Studio（录屏）
- After Effects（视频编辑）

**预计工作量**: 4-6 小时

---

## 📊 工作量总结

| 阶段 | 预计工作量 | 优先级 |
|------|-----------|--------|
| 阶段 1: 编辑器 UI 集成 | 18-26 小时 | 🔴 高 |
| 阶段 2: 高级功能完善 | 36-46 小时 | 🟡 中 |
| 阶段 3: 蓝图 API 暴露 | 2-4 小时 | 🟢 低 |
| 阶段 4: 性能测试和验证 | 20-28 小时 | 🔴 高 |
| 阶段 5: 文档完善 | 18-24 小时 | 🟡 中 |
| **总计** | **94-128 小时** | - |

**推荐实施顺序**:
1. 阶段 4（性能测试）- 验证当前实现效果
2. 阶段 1（编辑器 UI）- 提升用户体验
3. 阶段 5（文档）- 便于用户使用
4. 阶段 2（高级功能）- 增强功能完整性
5. 阶段 3（蓝图 API）- 扩展使用场景

---

## 🎯 版本规划

### v1.0 - 核心功能（当前版本）
- ✅ P0/P1 规则完全实现
- ✅ P2 规则部分实现
- ✅ C++ API 可用

### v1.1 - 可用性增强（预计 2 周）
- 编辑器 UI 集成
- 性能测试验证
- 基础文档

### v1.2 - 功能完善（预计 4 周）
- Rule10/11 完整实现
- 骨骼识别通道3
- 蓝图 API 暴露

### v1.3 - 正式发布（预计 6 周）
- 完整测试覆盖
- 完整文档
- 演示视频

---

## 📝 备注

### 技术风险
1. **LevelSetHelpers API 依赖**: Private API 可能在未来版本变更
2. **跨版本兼容性**: UE 5.3-5.7 的 API 差异需要持续适配
3. **性能开销**: 凸包计算和 LSV 生成可能耗时较长

### 缓解措施
1. 使用条件编译处理版本差异
2. 提供降级方案（Rule10/11 可选）
3. 异步执行耗时操作，显示进度条

### 依赖外部资源
- UE 官方文档（Physics Asset、Chaos）
- Mixamo 测试角色
- 社区反馈和测试

---

**文档版本**: 1.0
**创建日期**: 2025-01-XX
**最后更新**: 2025-01-XX
**维护者**: XIYBHK
