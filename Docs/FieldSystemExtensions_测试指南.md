# XFieldSystemActor 完整测试指南

## 📋 文档信息
- **模块**: FieldSystemExtensions
- **版本**: XTools Plugin v1.0
- **测试目标**: 验证GeometryCollection的Tag筛选功能
- **预计时间**: 45-60分钟

---

## 🎯 测试目标

验证XFieldSystemActor能否实现：
- ✅ 触发器A只影响带Tag "GroupA"的Chaos集
- ✅ 触发器B只影响带Tag "GroupB"的Chaos集
- ✅ 两组互不干扰，Tag筛选正确工作

---

## 📦 前置条件

### 环境要求
- Unreal Engine 5.3+
- XTools插件已编译并启用
- FieldSystemExtensions模块可用

### 知识储备
- 基础蓝图编辑
- Chaos破坏系统基础
- Field System基础概念

---

## 第一步：场景搭建（5分钟）

### 1.1 创建测试关卡
1. **File → New Level → Empty Level**
2. 保存为 `TestFieldSystem`（或任意名称）

### 1.2 添加基础光照
**必需**：
- Directional Light（位置任意，Rotation: (-45, 0, 0)）
- Sky Atmosphere

**可选**：
- Sky Light（提升环境光）
- Exponential Height Fog（增加氛围）

### 1.3 添加地板
1. **Place Actors → Shapes → Plane**（或Cube拉伸）
2. **Transform**:
   - Location: (0, 0, 0)
   - Scale: (10, 10, 1)
3. **Materials**: 应用任意材质（可选）

---

## 第二步：创建Chaos集（10分钟）

### 2.1 创建GeometryCollection资产

**方法1：从Cube创建**
1. **Content Browser → 右键 → Fracture → Geometry Collection**
2. 选择 **Static Mesh → Cube**
3. 命名: `GC_TestCube`
4. 打开资产 → **Fracture** 标签
5. 使用 **Uniform** 或 **Radial** 破碎模式
6. 点击 **Generate** 生成破碎

**方法2：从现有模型创建**
1. 导入或使用现有Static Mesh
2. 右键 → **Create → Geometry Collection**
3. 进行破碎设置

### 2.2 创建A组Chaos集（3个实例）

**放置到场景**：
1. 拖拽 `GC_TestCube` 到场景3次
2. 重命名为:
   - `GC_CubeA_1`
   - `GC_CubeA_2`
   - `GC_CubeA_3`

**设置位置**：
```
GC_CubeA_1: (-500, 0, 200)
GC_CubeA_2: (-300, 0, 200)
GC_CubeA_3: (-100, 0, 200)
```

**添加Tag（重要）**：
对每个GC_CubeA_*：
1. 选中Actor
2. **Details面板 → Tags**
3. 点击 **[+]** 按钮
4. 输入: `GroupA`（区分大小写）
5. 按Enter确认

### 2.3 创建B组Chaos集（3个实例）

重复上述步骤：
1. 再拖拽3个GC实例到场景
2. 重命名为:
   - `GC_CubeB_1`
   - `GC_CubeB_2`
   - `GC_CubeB_3`

**设置位置**：
```
GC_CubeB_1: (100, 0, 200)
GC_CubeB_2: (300, 0, 200)
GC_CubeB_3: (500, 0, 200)
```

**添加Tag**：
- Tag名称: `GroupB`

### 2.4 配置Chaos物理属性

**选中所有6个Chaos集**（Ctrl+点击）:

**Details → Chaos Physics**:
```
✅ Simulate Physics = true
✅ Enable Clustering = true
   Object Type = Dynamic
   Enable Damage = false（测试不需要）
```

**Details → Collision**:
```
✅ Simulation Generates Hit Events = false（可选）
✅ Enable Gravity = true
   Collision Enabled = Query and Physics
```

---

## 第三步：创建XFieldSystemActor（15分钟）

### 3.1 放置XFieldSystemActor_A

1. **Place Actors 面板 → 搜索 "XFieldSystemActor"**
2. 拖到场景中
3. **重命名**: `XFieldSystemActor_A`
4. **位置**: (-300, 0, 100)（A组中间上方）

### 3.2 配置Field节点

**选中 XFieldSystemActor_A**:

**Details → Field System Component → Construction Commands**:

1. 展开 **Construction Commands**
2. 点击 **[+ Add Field Command]**

**配置Field命令**:
```
☑ Enable Field = true

Physics Type: Linear Force

Field Node: 点击下拉 → 
  └─ Create New Asset → 
      └─ Radial Vector
          ├─ Magnitude: 5000
          └─ Position: (0, 0, 0)（相对于Actor位置）

Meta Data: None（留空）
```

### 3.3 配置Chaos原生筛选

**Details → Field System Component → Filter Settings**:
```
☑ Enable Filtering = true

Object Type: Destruction（或All）
Filter Type: Include
Position Type: Vertices（默认）
```

### 3.4 配置Tag筛选（关键）

**Details → Field | Runtime Filtering**:
```
☑ Enable Actor Tag Filter = true

Include Actor Tags:
  └─ [0]: GroupA

Exclude Actor Tags:
  └─ （留空）
```

### 3.5 配置GC注册模式

**Details → Field | GeometryCollection | Auto**:
```
☐ bAutoRegisterToGCs = false（触发器模式）
```

**⚠️ 重要说明**:
- `false` = 需要手动触发（适合触发器场景）
- `true` = 自动注册，持续影响（适合重力场等）

### 3.6 创建XFieldSystemActor_B

**快速复制**:
1. 选中 `XFieldSystemActor_A`
2. **Ctrl+D** 复制
3. 重命名为 `XFieldSystemActor_B`

**修改配置**:
```
位置: (300, 0, 100)（B组中间上方）

Details → Field | Runtime Filtering:
  └─ Include Actor Tags: 
      └─ [0]: GroupB（改为GroupB）
```

其他配置保持不变。

---

## 第四步：创建触发器（10分钟）

### 4.1 创建触发器A

1. **Place Actors → 搜索 "Box Trigger"**
2. 拖到场景
3. **重命名**: `Trigger_A`
4. **位置**: (-300, -500, 100)
5. **Scale**: (2, 2, 2)

### 4.2 编写触发器A蓝图

**选中 Trigger_A → Details → Blueprint/Add Script**

**Event Graph 蓝图节点连接**:

```
┌─────────────────────────────┐
│ Event ActorBeginOverlap     │
│ (Other Actor)               │
└──────────┬──────────────────┘
           │
           ▼
┌─────────────────────────────┐
│ Print String                │
│ ├─ Text: "触发器A激活"        │
│ ├─ Duration: 5.0            │
│ └─ Text Color: (0,1,0,1)    │  ← 绿色
└──────────┬──────────────────┘
           │
           ▼
┌─────────────────────────────┐
│ Get Actor of Class          │
│ └─ Actor Class:             │
│    XFieldSystemActor        │
└──────────┬──────────────────┘
           │ Return Value
           ▼
┌─────────────────────────────┐
│ Apply Current Field to      │
│ Filtered GCs                │
│ (调用在XFieldSystemActor上)  │
└─────────────────────────────┘
```

**详细步骤**:

1. **添加事件**:
   - 右键空白处 → **Add Event → Event ActorBeginOverlap**

2. **添加调试信息**（可选但推荐）:
   - 拖出执行引脚 → 搜索 `Print String`
   - 配置:
     ```
     Text: "触发器A激活"
     Duration: 5.0
     Text Color: (R=0, G=1, B=0, A=1)  绿色
     ```

3. **获取XFieldSystemActor_A**:
   - 拖出执行引脚 → 搜索 `Get Actor of Class`
   - **Actor Class**: 选择 `XFieldSystemActor`
   - **Return Value** → 拖出 → 搜索 `Cast to XFieldSystemActor`

4. **调用Field应用方法**:
   - 从Cast结果拖出 → 搜索 `Apply Current Field to Filtered GCs`
   - 连接执行引脚

**或者简化方式**（如果场景只有一个XFieldSystemActor_A）:
```
Event ActorBeginOverlap
  ↓
Get Actor of Class (XFieldSystemActor)
  ↓
Apply Current Field to Filtered GCs
```

### 4.3 创建触发器B

**快速复制**:
1. 在World Outliner中选中 `Trigger_A`
2. **Ctrl+D** 复制
3. 重命名为 `Trigger_B`
4. **位置**: (300, -500, 100)

**修改蓝图**:
1. 打开 `Trigger_B` 蓝图
2. 修改 `Print String`:
   ```
   Text: "触发器B激活"
   Text Color: (R=0, G=0, B=1, A=1)  蓝色
   ```
3. **确保Get Actor of Class获取的是XFieldSystemActor_B**
   - 如果场景中有多个XFieldSystemActor，使用 `Get All Actors of Class` + 筛选
   - 或直接在Trigger_B中添加对XFieldSystemActor_B的引用变量

**推荐：使用直接引用**（更可靠）:
1. 删除 `Get Actor of Class` 节点
2. 在World Outliner中选中 `XFieldSystemActor_B`
3. 在蓝图中右键 → **Create a Reference to XFieldSystemActor_B**
4. 从引用拖出 → `Apply Current Field to Filtered GCs`

---

## 第五步：准备测试环境（5分钟）

### 5.1 添加玩家起始点

1. **Place Actors → Player Start**
2. **位置**: (0, -1000, 100)
3. **Rotation**: (0, 0, 0)（面向Chaos集）

### 5.2 添加调试相机（可选）

如果需要固定视角观察：
1. **Place Actors → Camera Actor**
2. **位置**: (0, -1500, 500)
3. **Rotation**: (Pitch=-20, Yaw=0, Roll=0)

### 5.3 配置游戏模式（可选）

**World Settings → Game Mode**:
- 确保可以控制角色移动
- 或使用 **Third Person** / **First Person** 模板

### 5.4 最终检查清单

打开 **World Outliner**，逐项确认：

```
场景对象检查清单：

□ 地板（Plane/Cube）
□ 光照（Directional Light + Sky Atmosphere）

Chaos集 - A组：
□ GC_CubeA_1 (Tag: GroupA)
□ GC_CubeA_2 (Tag: GroupA)
□ GC_CubeA_3 (Tag: GroupA)

Chaos集 - B组：
□ GC_CubeB_1 (Tag: GroupB)
□ GC_CubeB_2 (Tag: GroupB)
□ GC_CubeB_3 (Tag: GroupB)

Field System：
□ XFieldSystemActor_A
  └─ Tag筛选: GroupA
  └─ bAutoRegisterToGCs: false
  └─ Field配置完成

□ XFieldSystemActor_B
  └─ Tag筛选: GroupB
  └─ bAutoRegisterToGCs: false
  └─ Field配置完成

触发器：
□ Trigger_A (蓝图: 调用A的方法)
□ Trigger_B (蓝图: 调用B的方法)

其他：
□ Player Start
□ Camera Actor（可选）
```

---

## 第六步：执行测试（10分钟）

### 6.1 测试准备

**打开Output Log**:
1. **Window → Developer Tools → Output Log**
2. 筛选器设置为显示 `LogTemp` 类别

**保存所有**:
1. **Ctrl+Shift+S** 保存所有资产
2. 确保蓝图已编译（无错误）

### 6.2 测试1：触发器A单独测试

**执行步骤**:
1. 点击 **Play** 按钮（或 Alt+P）
2. 控制角色移动到 **Trigger_A**（左侧触发器）
3. 观察效果

**预期结果**:
```
✅ 屏幕左上角显示绿色文字 "触发器A激活"
✅ GroupA的3个Chaos集（左侧）受力移动/碎裂
✅ GroupB的3个Chaos集（右侧）完全不动
✅ Output Log显示:
   LogTemp: XFieldSystemActor: Collected 3 GeometryCollections
   LogTemp: XFieldSystemActor: Applied N construction fields to 3 GeometryCollections
```

**判定标准**:
- ✅ **成功**: 只有左侧3个Chaos集受影响
- ❌ **失败**: 右侧Chaos集也动了（Tag筛选失败）
- ❌ **失败**: 所有Chaos集都不动（Field未生效）

### 6.3 测试2：触发器B单独测试

**执行步骤**:
1. 点击 **Stop**（Esc）停止Play
2. 重新点击 **Play**
3. 控制角色移动到 **Trigger_B**（右侧触发器）
4. 观察效果

**预期结果**:
```
✅ 屏幕左上角显示蓝色文字 "触发器B激活"
✅ GroupB的3个Chaos集（右侧）受力移动/碎裂
✅ GroupA的3个Chaos集（左侧）完全不动
✅ Output Log显示:
   LogTemp: XFieldSystemActor: Collected 3 GeometryCollections
   LogTemp: XFieldSystemActor: Applied N construction fields to 3 GeometryCollections
```

### 6.4 测试3：快速连续触发

**执行步骤**:
1. Play后先触发 **Trigger_A**
2. 等待GroupA的Chaos集反应
3. 立即移动到 **Trigger_B**
4. 观察GroupB的反应

**预期结果**:
```
✅ 两组Chaos集独立反应
✅ 互不干扰
✅ 每组只受对应的XFieldSystemActor影响
```

### 6.5 测试4：同时触发（压力测试）

**修改测试场景**（可选）:
1. Stop Play
2. 将两个触发器移到重叠位置
3. 或将触发器Scale放大，覆盖所有Chaos集

**执行**:
1. Play后站在两个触发器重叠处
2. 同时触发A和B

**预期结果**:
```
✅ GroupA只受XFieldSystemActor_A影响
✅ GroupB只受XFieldSystemActor_B影响
✅ Tag筛选正确隔离两组
✅ Output Log显示两次Field应用（各3个GC）
```

---

## 第七步：故障排查（必读）

### 问题1: 所有Chaos集都受影响（无Tag筛选）

**症状**:
- 触发器A影响了所有6个Chaos集
- Tag筛选似乎没有工作

**原因分析**:
1. `bAutoRegisterToGCs = true`（自动注册模式）
2. Tag筛选未启用
3. Tag名称不匹配

**解决方案**:
```
1. 选中 XFieldSystemActor_A 和 B
2. Details → Field | GeometryCollection | Auto
   └─ 确认 bAutoRegisterToGCs = false

3. Details → Field | Runtime Filtering
   └─ 确认 Enable Actor Tag Filter = true
   └─ 确认 Include Actor Tags 包含正确的Tag

4. 重新Play测试
```

### 问题2: Chaos集完全不动

**症状**:
- 触发器触发后，所有Chaos集都没有反应
- Output Log显示Field已应用

**可能原因**:

**A. Chaos物理未启用**:
```
检查每个Chaos集:
Details → Chaos Physics
  └─ Simulate Physics = true？
  └─ Object Type = Dynamic？
```

**B. Field强度不够**:
```
检查XFieldSystemActor:
Details → Field System Component → Construction Commands
  └─ Field Node → Magnitude
      └─ 尝试增加到 10000 或更高
```

**C. Field位置问题**:
```
检查XFieldSystemActor位置:
  └─ 是否在Chaos集附近？
  └─ Radial Vector的Position是否正确（相对位置）？
```

**D. Chaos集已经落地/休眠**:
```
解决方案:
1. 将Chaos集位置设置更高（Z=500）
2. 或禁用Gravity
3. 确保在落地前触发Field
```

### 问题3: Output Log显示收集到0个GC

**症状**:
```
LogTemp: XFieldSystemActor: Collected 0 GeometryCollections
```

**原因**: Tag筛选失败

**逐步排查**:

1. **检查Tag拼写**:
   ```
   Chaos集的Tag: "GroupA"
   XFieldSystemActor的Include Tags: "GroupA"
   ↑ 必须完全一致（区分大小写）
   ```

2. **检查Tag筛选开关**:
   ```
   Details → Field | Runtime Filtering
     └─ Enable Actor Tag Filter = true？
   ```

3. **检查Chaos集类型**:
   ```
   确认是 GeometryCollectionActor，不是StaticMeshActor
   ```

4. **手动验证Tag**:
   ```
   选中Chaos集 → Details → Tags
     └─ 确认Tag列表中有正确的Tag
   ```

### 问题4: 触发器蓝图不执行

**症状**:
- "触发器A激活"文字没有显示
- Output Log没有Field应用的日志

**检查清单**:

1. **触发器碰撞设置**:
   ```
   Details → Collision
     └─ Generate Overlap Events = true？
     └─ Collision Enabled = Query Only 或 Query and Physics？
   ```

2. **角色碰撞设置**:
   ```
   角色需要能够触发Overlap事件
   检查角色的Collision设置
   ```

3. **蓝图编译状态**:
   ```
   打开触发器蓝图
     └─ 左上角是否有 "Compile" 按钮？
     └─ 点击Compile，确保无错误
   ```

4. **蓝图节点连接**:
   ```
   确认执行引脚（白色三角）正确连接
   确认没有断开的节点
   ```

### 问题5: 编译错误

**症状**:
```
Error: XFieldSystemActor 未找到
或
Error: ApplyCurrentFieldToFilteredGCs 不存在
```

**解决步骤**:

1. **确认插件已启用**:
   ```
   Edit → Plugins
     └─ 搜索 "XTools"
     └─ 确认已勾选
   ```

2. **重新生成项目文件**:
   ```
   1. 关闭UE编辑器
   2. 右键 .uproject → Generate Visual Studio project files
   3. 打开 .sln
   4. Build Solution (Ctrl+Shift+B)
   5. 重新打开UE
   ```

3. **检查模块依赖**:
   ```
   YourProject.Build.cs:
   PublicDependencyModuleNames.AddRange(new string[] { 
       "FieldSystemExtensions"  // 确保添加了
   });
   ```

### 问题6: "Registered to 0/N GeometryCollections"

**症状**:
```
LogTemp: XFieldSystemActor: Collected 3 GeometryCollections
LogTemp: XFieldSystemActor: Registered to 0/3 GeometryCollections
```

**原因**: GC已经有InitializationFields了

**解决方案**:
```
这是正常的！如果GC之前已经注册过，就不会重复注册。
只要 "Collected N GeometryCollections" 的数量正确即可。

如果需要强制刷新:
  └─ 调用 RefreshGeometryCollectionCache()
```

---

## 第八步：进阶测试（可选）

### 8.1 测试不同Field类型

**目的**: 验证各种Field节点都支持Tag筛选

**步骤**:
1. 选中 `XFieldSystemActor_B`
2. 修改Field Node为其他类型:

**选项A: Uniform Vector（统一方向力）**:
```
Create New → Uniform Vector
  ├─ Magnitude: 5000
  └─ Direction: (0, 0, 1)  向上
```

**选项B: Box Falloff（盒体衰减）**:
```
Create New → Box Falloff
  ├─ Magnitude: 1.0
  ├─ Transform: 设置盒体位置
  └─ Falloff Type: Linear
```

**预期**: GroupB仍然只受B影响，使用新的Field效果

### 8.2 测试动态Spawn

**目的**: 验证运行时Spawn的GC也能正确筛选

**创建Spawn蓝图**:
```blueprint
Level Blueprint:

Event BeginPlay
  ↓
Delay (2.0)
  ↓
Spawn Actor from Class
  ├─ Class: GeometryCollectionActor
  ├─ Spawn Transform: (-300, 500, 300)
  └─ Return Value
      ↓
  Set Actor Tags
      └─ Tags: ["GroupA"]
```

**执行**:
1. Play后等待2秒
2. 新GC会Spawn并落下
3. 触发Trigger_A

**预期**:
- ✅ 新Spawn的GC也受XFieldSystemActor_A影响
- ✅ Spawn监听器自动添加到缓存

### 8.3 测试持久模式对比

**目的**: 理解触发模式vs持久模式的区别

**修改配置**:
```
选中 XFieldSystemActor_A:
  └─ bAutoRegisterToGCs = true（改为true）

保持 XFieldSystemActor_B:
  └─ bAutoRegisterToGCs = false（保持false）
```

**执行测试**:
1. Play（不需要触发器）

**预期**:
```
✅ GroupA立即受影响（持续受力）
✅ GroupB不受影响（需要触发器触发）
```

**对比**:
| 模式 | bAutoRegister | 行为 |
|------|---------------|------|
| 持久 | true | BeginPlay立即生效，持续影响 |
| 触发 | false | 需要蓝图调用才生效 |

### 8.4 性能测试

**目的**: 测试大量GC的性能

**步骤**:
1. 复制Chaos集到20-30个
2. 分别添加GroupA/GroupB Tag
3. 触发Field

**观察**:
- FPS变化
- Output Log中的处理时间
- Field应用是否仍然正确

---

## 📊 测试结果记录表

### 基础功能测试

| 测试项 | 预期结果 | 实际结果 | 通过 | 备注 |
|--------|---------|---------|------|------|
| 触发器A影响GroupA | 只有3个GC受影响 | | ☐ | |
| 触发器A不影响GroupB | GroupB不动 | | ☐ | |
| 触发器B影响GroupB | 只有3个GC受影响 | | ☐ | |
| 触发器B不影响GroupA | GroupA不动 | | ☐ | |
| 同时触发互不干扰 | 各自独立工作 | | ☐ | |
| Output Log正确 | 显示3个GC | | ☐ | |

### 配置验证

| 配置项 | 期望值 | 实际值 | 正确 |
|--------|--------|--------|------|
| GC_CubeA_* Tag | GroupA | | ☐ |
| GC_CubeB_* Tag | GroupB | | ☐ |
| XFieldSystemActor_A Tag筛选 | GroupA | | ☐ |
| XFieldSystemActor_B Tag筛选 | GroupB | | ☐ |
| bAutoRegisterToGCs | false | | ☐ |
| Enable Actor Tag Filter | true | | ☐ |

### 性能指标

| 指标 | 数值 | 备注 |
|------|------|------|
| BeginPlay时间 | | ms |
| Field应用时间 | | ms |
| 帧率（FPS） | | Play时 |
| GC收集数量 | 3/3 | 正确 |

---

## 🎓 成功标准

测试完全成功的标志：

### 功能完整性
- ✅ 所有6项基础功能测试通过
- ✅ Tag筛选准确无误
- ✅ Output Log输出正确
- ✅ 无编译/运行时错误

### 性能标准
- ✅ BeginPlay时间 < 500ms
- ✅ Field应用时间 < 100ms
- ✅ FPS保持稳定（>30）

### 用户体验
- ✅ 触发反馈明显（Print String显示）
- ✅ Chaos集反应清晰可见
- ✅ 两组区分明确

---

## 🔧 常见配置参考

### 推荐Field配置

**爆炸效果**:
```
Radial Vector
  ├─ Magnitude: 10000
  └─ Position: (0, 0, 0)
```

**上升力**:
```
Uniform Vector
  ├─ Magnitude: 5000
  └─ Direction: (0, 0, 1)
```

**推力**:
```
Uniform Vector
  ├─ Magnitude: 3000
  └─ Direction: (1, 0, 0)
```

### 推荐Chaos配置

**易碎效果**:
```
Chaos Physics:
  ├─ Enable Damage = true
  ├─ Damage Threshold = 100
  └─ Cluster Connection Type = Delaunay
```

**漂浮效果**:
```
Chaos Physics:
  ├─ Enable Gravity = false
  ├─ Linear Damping = 0.5
  └─ Angular Damping = 0.5
```

---

## 📞 支持与反馈

### 遇到问题？

1. **检查Output Log**: 
   - `LogTemp` 类别的XFieldSystemActor日志
   - 查看收集到的GC数量

2. **检查配置清单**: 
   - 使用本文档的检查清单逐项确认

3. **查看源码注释**: 
   - `XFieldSystemActor.h` 中有详细的参数说明

### 测试结论模板

```
测试日期: ____/____/____
测试人员: ________________
UE版本: __________________
XTools版本: ______________

基础功能: ☐ 通过 ☐ 失败
Tag筛选: ☐ 通过 ☐ 失败
性能表现: ☐ 良好 ☐ 一般 ☐ 差

问题记录:
_________________________________
_________________________________

改进建议:
_________________________________
_________________________________
```

---

## 附录A：快速参考

### 关键蓝图节点

```
ApplyCurrentFieldToFilteredGCs
  └─ 应用已配置的Field到筛选后的GC

RefreshGeometryCollectionCache
  └─ 刷新GC缓存（如果有新Spawn）

ApplyFieldToFilteredGeometryCollections
  └─ 应用手动创建的Field节点
```

### 关键配置属性

```
bAutoRegisterToGCs (bool)
  └─ false: 触发模式
  └─ true: 持久模式

Enable Actor Tag Filter (bool)
  └─ 启用Tag筛选

Include Actor Tags (Array<Name>)
  └─ 包含的Tag列表

Exclude Actor Tags (Array<Name>)
  └─ 排除的Tag列表
```

---

**文档版本**: 1.0  
**最后更新**: 2025-01-04  
**维护者**: XTools Team

