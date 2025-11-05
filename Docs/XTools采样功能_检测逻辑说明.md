# XTools采样功能 - 检测逻辑完整说明

## 核心检测流程

```
1. 查找目标StaticMeshComponent
   ↓
2. 获取Component的ObjectType
   ↓
3. 在BoundingBox内生成网格点
   ↓
4. 对每个点执行SphereTrace（按ObjectType检测）
   ↓
5. 验证命中的是否是目标Component
   ↓
6. 添加有效点
```

## 目标Mesh的必要属性

### 1. 组件类型要求 ✅ 必须

**要求**: Actor必须包含 `UStaticMeshComponent`

**代码位置**: `XToolsLibrary.cpp` 第1012行
```cpp
UStaticMeshComponent* TargetMeshComponent = TargetActor->FindComponentByClass<UStaticMeshComponent>();
```

**说明**:
- 如果Actor没有StaticMeshComponent，会返回错误
- 如果有多个StaticMeshComponent，只会使用第一个找到的

**检查方法**:
- 在UE编辑器中选中Actor
- Details面板查看Components
- 确保有 `Static Mesh Component`

---

### 2. 碰撞启用状态 ✅ 必须

**要求**: `Collision Enabled` 必须设置为以下之一：
- `Query Only`（仅查询）
- `Physics Only`（仅物理）- 不推荐
- `Collision Enabled`（查询和物理）✓ 推荐

**不可用的设置**:
- `No Collision`（无碰撞）❌ - 无法被检测

**原因**: `SphereTraceSingleForObjects` 是查询操作（Query），需要启用Query

**检查方法**:
```
选中Actor → Details面板 → Static Mesh Component → Collision 栏
找到 "Collision Enabled" 属性
```

**UE界面位置**:
```
Details
└─ Static Mesh Component
   └─ Collision
      └─ Collision Enabled: [Collision Enabled (Query and Physics)]
```

---

### 3. 对象类型（Object Type）✅ 重要

**当前逻辑**: 自动获取目标Component的ObjectType

**代码位置**: `XToolsLibrary.cpp` 第1030-1032行
```cpp
const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { 
    UEngineTypes::ConvertToObjectType(TargetMeshComponent->GetCollisionObjectType()) 
};
```

**常见Object Type**:
- `WorldStatic` - 静态世界对象（建筑、雕像）
- `WorldDynamic` - 动态世界对象（可移动物体）
- `Pawn` - 角色
- `PhysicsBody` - 物理体
- `Vehicle` - 载具
- `Destructible` - 可破坏物体

**重要说明**:
- 函数会自动获取目标的ObjectType用于检测
- 场景中其他相同ObjectType的对象也会被检测到
- 但最后会验证命中的是否是目标组件（第974行）

**检查方法**:
```
Details → Static Mesh Component → Collision
找到 "Object Type" 属性
```

---

### 4. 碰撞复杂度设置 ⚙️ 可选

**参数**: `bUseComplexCollision`（默认值通常为false）

**Simple Collision（简单碰撞）** - `bUseComplexCollision = false`:
- 使用碰撞盒/球/胶囊体
- 性能好 ✓
- 精度低（特别是复杂模型）
- 适合：方块、球体、简单形状

**Complex Collision（复杂碰撞）** - `bUseComplexCollision = true`:
- 使用模型的实际多边形
- 性能差（逐三角形检测）
- 精度高 ✓
- 适合：雕像、复杂形状、精确采样

**检查方法**:
```
Details → Static Mesh Component → Collision
查看 "Collision Complexity" 属性：
- Use Simple Collision As Complex: 强制使用简单碰撞
- Use Complex Collision As Simple: 强制使用复杂碰撞
- Project Default: 使用项目默认设置
```

**建议**:
- 如果采样点分布不准确（悬空或缺失），尝试启用 `bUseComplexCollision`
- 如果性能有问题，尝试禁用 `bUseComplexCollision` 并优化简单碰撞形状

---

### 5. 碰撞响应（Collision Response） ⚠️ 不影响

**重要**: 碰撞响应设置（Block/Overlap/Ignore）**不影响**此功能

**原因**: 
- 我们使用的是 `SphereTraceForObjects`（按对象类型检测）
- 不是 `SphereTraceForChannel`（按通道检测）
- ObjectType检测不受Collision Response影响

**示例**:
```
即使设置了：
- Block All
- Ignore All
- Custom设置
都不会影响采样检测
```

---

## 检测逻辑详解

### 第1阶段：粗筛（可选）

**代码**: `XToolsLibrary.cpp` 第906-914行

```cpp
if (bEnableBoundsCulling)
{
    TargetBounds = TargetMeshComponent->Bounds.GetBox();
    const float ExpansionRadius = TraceRadius + (Noise > 0.0f ? Noise * 1.73205f : 0.0f);
    TargetBounds = TargetBounds.ExpandBy(ExpansionRadius);
}
```

**作用**:
- 使用模型的AABB（轴对齐包围盒）快速剔除远离目标的点
- 大幅提升性能（特别是大范围采样）

**参数**: `bEnableBoundsCulling`（默认true，建议保持）

---

### 第2阶段：精确检测

**代码**: `XToolsLibrary.cpp` 第956-970行

```cpp
const bool bHit = UKismetSystemLibrary::SphereTraceSingleForObjects(
    World,
    WorldPoint,        // 起点
    WorldPoint,        // 终点（相同=点检测）
    TraceRadius,       // 检测球体半径
    ObjectTypes,       // 目标的ObjectType
    bUseComplexCollision,
    TArray<AActor*>(), // 忽略列表（空）
    DebugDrawType,
    HitResult,
    true,              // 忽略自身
    ...
);
```

**关键参数**:
- `WorldPoint` 起点和终点相同 = 静态球体检测（不是射线）
- `TraceRadius` 检测半径：点周围TraceRadius范围内有碰撞即命中
- `ObjectTypes` 只检测目标的对象类型

---

### 第3阶段：组件验证（关键修复）

**代码**: `XToolsLibrary.cpp` 第974行

```cpp
if (bHit && HitResult.GetComponent() == TargetMeshComponent)
{
    ValidPoints.Add(WorldPoint);
}
```

**作用**: 
- 确保命中的是目标组件，而不是场景中其他相同ObjectType的对象
- 这是防止"地板上也出现采样点"的关键修复

**举例**:
```
场景：雕像(WorldStatic) + 地板(WorldStatic) + 墙壁(WorldStatic)

检测点A在雕像内：
  SphereTrace → 命中雕像组件 → GetComponent() == 雕像 → 添加 ✓

检测点B在地板上：
  SphereTrace → 命中地板组件 → GetComponent() == 地板 ≠ 雕像 → 跳过 ✓
```

---

## 常见问题排查

### 问题1：采样点为0

**可能原因**:
1. ❌ Collision Enabled = No Collision
   - **解决**: 改为 `Collision Enabled`

2. ❌ TraceRadius 太小
   - **解决**: 增大TraceRadius（建议≥5.0）

3. ❌ BoundingBox 没有覆盖模型
   - **解决**: 调整BoundingBox位置和大小

4. ❌ GridSpacing 太大，网格点太稀疏
   - **解决**: 减小GridSpacing

5. ❌ 使用Simple Collision但模型没有碰撞体
   - **解决**: 启用 `bUseComplexCollision` 或添加简单碰撞体

---

### 问题2：地板/墙壁上也有采样点

**原因**: 旧版本的Bug（已修复）

**解决**: 
- 确保使用最新版本（2025-11-05修复）
- 代码第974行应包含组件验证

---

### 问题3：采样点不准确（悬空或缺失）

**可能原因**: 使用Simple Collision，但碰撞形状不匹配模型

**解决方案**:
1. **方案A**: 启用复杂碰撞
   ```
   在蓝图中设置参数：
   Use Complex Collision = true
   ```

2. **方案B**: 优化简单碰撞
   ```
   打开Static Mesh编辑器 → Collision菜单
   → 重新生成碰撞体（Auto Convex Collision）
   → 调整精度
   ```

---

### 问题4：性能很慢

**可能原因**:
1. GridSpacing太小（点太多）
2. 启用了Complex Collision
3. 禁用了Bounds Culling

**解决方案**:
```
优化参数：
- GridSpacing: 增大（例如从5改为10）
- bUseComplexCollision: false（如果精度允许）
- bEnableBoundsCulling: true（保持启用）
- TraceRadius: 适度减小（但不要太小）
```

---

## 快速检查清单

在使用采样功能前，确保目标Actor满足：

- [ ] 有 `StaticMeshComponent`
- [ ] `Collision Enabled` ≠ `No Collision`
- [ ] `Object Type` 已设置（任意有效类型）
- [ ] BoundingBox 覆盖目标模型
- [ ] GridSpacing 合理（建议5-20）
- [ ] TraceRadius 合理（建议5-10）

**可选优化**:
- [ ] 启用 `bEnableBoundsCulling`（性能优化）
- [ ] 根据精度需求调整 `bUseComplexCollision`
- [ ] 启用调试绘制观察采样过程

---

## 参数推荐值

### 典型场景1：角色雕像（中等精度）
```
GridSpacing: 10.0
TraceRadius: 5.0
bUseComplexCollision: false
bEnableBoundsCulling: true
Noise: 2.0
```

### 典型场景2：复杂雕塑（高精度）
```
GridSpacing: 5.0
TraceRadius: 3.0
bUseComplexCollision: true
bEnableBoundsCulling: true
Noise: 1.0
```

### 典型场景3：大型建筑（性能优先）
```
GridSpacing: 20.0
TraceRadius: 10.0
bUseComplexCollision: false
bEnableBoundsCulling: true
Noise: 5.0
```

---

## 相关源码文件

- 主要实现: `Source/XTools/Private/XToolsLibrary.cpp`
- 公共接口: `Source/XTools/Public/XToolsLibrary.h`
- UE FHitResult: `Runtime/Engine/Classes/Engine/HitResult.h`
- UE Trace函数: `Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h`

---

**文档版本**: 2025-11-05
**对应代码版本**: XTools Plugin v1.0+

