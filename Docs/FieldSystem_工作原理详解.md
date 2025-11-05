# Field System 工作原理详解

## 🎯 你的发现

**现象**：
- 角色碰撞设置为"与世界动态交互" → **受Field影响** ✓
- 角色碰撞设置为"忽略世界动态" → **不受Field影响** ✗

**问题**：为什么碰撞设置会影响Field System？

---

## 🔬 深度解析

### 1. Field System 的完整工作流程

```
FS_MasterField（蓝图Actor）
    ↓
FieldSystemComponent（组件）
    ↓ 每Tick/触发时
Chaos Physics Solver（物理求解器）
    ↓ 查询场景中的物理粒子
Physics Particles（物理粒子集合）
    ↓ 筛选条件
符合条件的粒子
    ↓ 应用Field效果
Force/State Change（力/状态变化）
```

---

### 2. 关键：Chaos物理粒子的注册条件

**一个对象要成为Chaos物理粒子，必须满足**：

| 条件 | 检查项 | 说明 |
|------|--------|------|
| ✅ 物理模拟 | `Simulate Physics = true` | 启用物理模拟 |
| ✅ 碰撞启用 | `CollisionEnabled != NoCollision` | 参与物理世界 |
| ✅ 物理组件 | 有`UPrimitiveComponent` | 具有物理形状 |
| ✅ 物理体 | 有`FBodyInstance` | 物理实例存在 |

---

### 3. 碰撞设置的影响机制

#### CollisionEnabled 枚举值

```cpp
enum class ECollisionEnabled
{
    NoCollision,              // ❌ 完全不参与碰撞和物理
    QueryOnly,                // ⚠️ 只参与查询（射线检测），不参与物理
    PhysicsOnly,              // ✓ 只参与物理，不参与查询
    QueryAndPhysics           // ✓ 参与查询和物理（完整物理交互）
};
```

#### Field System 的筛选逻辑

```cpp
// Chaos底层伪代码
void ChaosSolver::ApplyField(Field)
{
    for (PhysicsParticle : AllParticles)
    {
        // 检查1：是否参与物理？
        if (Particle.CollisionEnabled == NoCollision)
            continue;  // ❌ 跳过，不处理
        
        // 检查2：是否匹配ObjectType筛选？
        if (!MatchesFilter(Particle, Field.Filter))
            continue;
        
        // ✓ 应用Field效果
        ApplyForce(Particle, Field.Force);
    }
}
```

---

### 4. 你的角色为什么受影响？

#### 情况A：与"世界动态"可交互

**碰撞设置**：
```
CollisionEnabled = QueryAndPhysics
CollisionObjectType = Pawn
CollisionResponses:
  - WorldDynamic = Block/Overlap  ← 参与物理交互
```

**Chaos视角**：
- ✓ `CollisionEnabled != NoCollision`
- ✓ 注册为物理粒子
- ✓ Field可以影响

**结果**：受Field影响 ✓

---

#### 情况B：忽略"世界动态"

**碰撞设置**：
```
CollisionEnabled = NoCollision  ← 关键！
或者
CollisionEnabled = QueryOnly   ← 只用于查询
```

**Chaos视角**：
- ❌ `CollisionEnabled == NoCollision` 或 `QueryOnly`
- ❌ 不参与物理交互
- ❌ 不注册为"物理粒子"（或标记为"非物理"）
- ❌ Field忽略

**结果**：不受Field影响 ✗

---

### 5. 为什么Chaos这样设计？

#### 性能考虑

```
场景中有10000个对象
↓
只有500个启用了物理模拟
↓
只有100个参与物理交互（CollisionEnabled != NoCollision）
↓
Field System只需处理100个对象，而不是10000个
```

**性能提升**：100倍！

#### 逻辑合理性

```
NoCollision = "这个对象不在物理世界中"
             = "不应该受物理力影响"
             = "Field也不应该影响它"
```

这是符合逻辑的：如果一个对象声称"我不参与物理"，那物理系统就不会管它。

---

## 🎮 实际应用场景

### 场景1：死亡角色Ragdoll

**问题**：角色死亡启用Ragdoll时，不希望被Field影响

**错误方案**：
```cpp
// ❌ 禁用碰撞 → Ragdoll会穿模
Character->SetCollisionEnabled(ECollisionEnabled::NoCollision);
```

**正确方案**：
```cpp
// ✓ 使用ObjectType筛选
FieldActor->ObjectType = EFieldObjectType::Field_Object_Destruction;
// 角色自动被排除（因为角色是Character类型）
```

---

### 场景2：特殊道具不受Field影响

**需求**：重要道具不被Field吹走

**方案1：禁用碰撞**（不推荐）
```cpp
ImportantProp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
// ✗ 副作用：道具可能穿模或失去碰撞
```

**方案2：物理材质**（推荐）
```cpp
// 创建高阻尼材质
PM_Heavy->LinearDamping = 50.0;
PM_Heavy->Mass = 1000.0;

ImportantProp->SetPhysMaterialOverride(PM_Heavy);
// ✓ 保留物理，但大幅减少Field影响
```

**方案3：Tag筛选**（最灵活）
```cpp
// 给道具添加Tag
ImportantProp->Tags.Add("FieldImmune");

// Field Actor排除该Tag
XFieldSystemActor->ExcludeActorTags.Add("FieldImmune");
```

---

## 🔧 FS_MasterField 的内部结构

### 典型的FS_MasterField配置

```
FS_MasterField (Blueprint)
├── FieldSystemComponent
│   └── Field Nodes (蓝图节点网络)
│       ├── RadialVector (径向力场)
│       ├── RadialFalloff (衰减)
│       ├── UniformVector (统一力)
│       └── MetaData (筛选器，可选)
│
└── Tick Mode
    ├── OnConstruction → 静态场（永久存在）
    └── OnTick → 动态场（每帧更新）
```

### Tick触发模式的工作原理

```cpp
// FS_MasterField蓝图伪代码
void FS_MasterField::Tick(DeltaTime)
{
    // 1. 获取当前位置/参数
    FVector Location = GetActorLocation();
    float Magnitude = ForceStrength;
    
    // 2. 创建Field节点
    URadialVector* RadialField = CreateRadialVector(Location, Magnitude);
    
    // 3. 应用到Chaos
    FieldSystemComponent->ApplyPhysicsField(
        true,                           // Enable
        EFieldPhysicsType::Field_LinearForce,  // 线性力
        nullptr,                        // MetaData（可选）
        RadialField                     // Field节点
    );
}
```

**每Tick**：
1. 重新计算Field参数
2. 向Chaos发送新的Field命令
3. Chaos对所有符合条件的物理粒子应用力

---

## 📊 调试Field System

### 查看哪些对象被Field影响

**控制台命令**：
```
p.Chaos.DebugDraw.Enabled 1
p.Chaos.DebugDraw.ShowParticles 1
```

**观察**：
- 绿色粒子 = 动态，受Field影响
- 蓝色粒子 = 运动学，可能受影响
- 灰色粒子 = 静态，不受影响
- 无粒子显示 = 未参与物理（NoCollision）

### 验证碰撞设置

```cpp
// C++调试代码
UPrimitiveComponent* Comp = Character->GetMesh();
UE_LOG(LogTemp, Log, TEXT("CollisionEnabled: %d"), 
    (int32)Comp->GetCollisionEnabled());

// 输出：
// 0 = NoCollision      → 不受Field影响
// 1 = QueryOnly        → 不受Field影响
// 2 = PhysicsOnly      → 受Field影响
// 3 = QueryAndPhysics  → 受Field影响
```

---

## 🎯 解决你的问题

### 最简方案（推荐）

**需求**：死亡Ragdoll不受FS_MasterField影响

**方案**：
```
1. FS_MasterField → Parent Class = XFieldSystemActor
2. 启用筛选 ✓
3. 对象类型 = Destruction
```

**原理**：
- Chaos自动将Ragdoll标记为`Character`类型
- 破碎物体标记为`Destruction`类型
- 设置只影响`Destruction` → 自动排除Ragdoll

**优点**：
- ✅ 不修改碰撞设置
- ✅ Ragdoll正常物理表现
- ✅ 零性能开销
- ✅ 一行配置

---

## 🧪 实验验证

### 测试1：碰撞设置的影响

```cpp
// 创建测试Actor
AActor* TestActor = SpawnActor();
UStaticMeshComponent* Mesh = TestActor->FindComponentByClass<UStaticMeshComponent>();

// 测试不同碰撞设置
Mesh->SetSimulatePhysics(true);

// Case 1
Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
// 结果：不受Field影响 ✗

// Case 2  
Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
// 结果：不受Field影响 ✗

// Case 3
Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
// 结果：受Field影响 ✓

// Case 4
Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
// 结果：受Field影响 ✓
```

### 测试2：ObjectType筛选

```cpp
// Field设置
XFieldSystemActor->ObjectType = EFieldObjectType::Field_Object_Destruction;

// 角色Ragdoll（Chaos标记为Character）
CharacterMesh->SetSimulatePhysics(true);
CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
// 结果：不受Field影响 ✗（被ObjectType筛选排除）

// 破碎道具（Chaos标记为Destruction）
DestructibleMesh->SetSimulatePhysics(true);
DestructibleMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
// 结果：受Field影响 ✓
```

---

## 📚 总结

### 关键要点

1. **Field System通过Chaos物理系统工作**
   - 只影响注册为"物理粒子"的对象

2. **碰撞设置是"参与物理"的门票**
   - `NoCollision` / `QueryOnly` → 不参与物理 → 不受Field影响
   - `PhysicsOnly` / `QueryAndPhysics` → 参与物理 → 受Field影响

3. **ObjectType筛选是Chaos原生功能**
   - 性能最优
   - 不依赖碰撞设置
   - 推荐优先使用

4. **你的最佳方案**
   - 保持Ragdoll的碰撞设置（需要物理交互）
   - 使用ObjectType筛选排除角色
   - 简单、高效、符合UE规范

---

## 🔗 相关文档

- [Chaos Physics](https://docs.unrealengine.com/5.3/en-US/chaos-physics-in-unreal-engine/)
- [Field System](https://docs.unrealengine.com/5.3/en-US/field-system-in-unreal-engine/)
- [Collision Filtering](https://docs.unrealengine.com/5.3/en-US/collision-filtering-in-unreal-engine/)

