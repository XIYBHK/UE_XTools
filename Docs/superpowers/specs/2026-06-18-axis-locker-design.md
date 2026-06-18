# AxisLocker 物理轴向锁定模块 — 设计文档

- 日期：2026-06-18
- 状态：已批准设计，待写实现计划
- 参考：`ref/UE4_AxisLocker_Plugin`、官方源码 `Engine/.../PhysicsEngine/BodyInstance.{h,cpp}`

## 1. 背景与目标

为 XTools 新增一个**物理 Actor 轴向锁定**模块，让开启物理模拟的组件可以在**运行时、通过蓝图**锁定/解锁指定的位移与旋转自由度（DOF）。

参考插件 `ref/UE4_AxisLocker_Plugin` 提供了最小实现，但存在裸指针崩溃风险、不符合 XTools 规范、功能单一等问题。本模块在其验证过的核心机制基础上，做成一个合规、可长期维护的正式模块。

### 核心价值定位
UE 原生在编辑器 Details 面板已能配置 DOF 锁，会 C++ 的人三行代码也能调。因此本模块的**唯一差异化价值是"给纯蓝图用户的运行时便利"**：预设一键、状态查询、临时锁定/恢复——这些是 C++ 三行写不出来的。

### 已验证的关键事实
- `FBodyInstance` 的 DOF 锁定（`bLockXTranslation`/`bLockXRotation` 等 6 字段 + `SetDOFLock(EDOFMode::SixDOF)`）虽在官方源码标注 `[Physx Only]`，但**在 UE 5.3 的 Chaos 引擎下实测生效**（底层创建的是引擎无关的 `FConstraintInstance`，约束到 World 的隐藏硬约束）。
- 逐轴任意组合**必须**走 `SixDOF` 模式 + 逐字段 `bLockXXX`；平面模式（XYPlane 等）只能锁平面，无法表达任意组合。这是社区标准做法。
- `SetDOFLock` 每次调用都会 `TermConstraint` + 重建约束，**避免每帧调用**。

## 2. 范围

### 第一期（本设计）
1. **硬锁 / 解锁**：6 轴任意组合，底层 SixDOF
2. **查询状态**：纯函数读取当前锁定情况
3. **预设组合**：枚举一键应用高频物理玩法
4. **临时锁定 / 恢复**：Push/Pop 状态栈

### 第二期（不在本设计内，仅备忘）
- 局部空间动态锁（每帧重建约束，开销高）
- 软锁 / 弹簧（需独立 `UPhysicsConstraintComponent`，另一套机制）
- 批量操作、状态变化委托
- 调试可视化（PIE DrawDebug）
- 网络复制语义（服务器权威 + RPC 包装）

## 3. 模块结构

新模块 `AxisLocker`：
- 类型 `Runtime` / `LoadingPhase: Default`
- 平台：Win64、Mac、Linux、Android、iOS
- 依赖：`Core`、`CoreUObject`、`Engine`、`XToolsCore`

```
Source/AxisLocker/
├─ AxisLocker.Build.cs              # 标准配置（PCH/IWYU/bUseUnity=false）+ WITH_AXISLOCKER=1
├─ Public/
│  ├─ AxisLockerAPI.h               # DECLARE_LOG_CATEGORY_EXTERN(LogAxisLocker, ...)
│  ├─ AxisLockTypes.h               # EAxisLockPreset 枚举 + FAxisLockState 结构体
│  ├─ AxisLockLibrary.h             # UBlueprintFunctionLibrary（核心逻辑）
│  └─ AxisLockerComponent.h         # USceneComponent（薄适配器）
└─ Private/
   ├─ AxisLockerModule.cpp          # IMPLEMENT_MODULE + DEFINE_LOG_CATEGORY
   ├─ AxisLockLibrary.cpp
   └─ AxisLockerComponent.cpp
```

同时在 `XTools.uplugin` 的 `Modules` 数组注册该模块。

### 职责切分（隔离原则）

| 单元 | 职责 | 依赖 | 状态 |
|------|------|------|------|
| `UAxisLockLibrary` | 全部核心逻辑：锁/解锁/预设/查询，输入任意 `UPrimitiveComponent*` | 仅引擎物理 | 无状态（静态函数） |
| `UAxisLockerComponent` | 薄适配器：目标解析 + 声明式配置 + BeginPlay 自动应用 + 临时恢复栈 | 调用 `UAxisLockLibrary` | 有状态（恢复栈） |

组件不重复实现锁定逻辑，一律委托给函数库。

## 4. 公共 API

所有蓝图节点使用 Category `XTools|轴向锁定`，中文 DisplayName / ToolTip / Keywords。

### 4.1 类型（`AxisLockTypes.h`）

```cpp
// 锁定状态：既是查询返回值，也是恢复栈元素
USTRUCT(BlueprintType)
struct FAxisLockState
{
    bool bLockPositionX / Y / Z;   // 位移锁定
    bool bLockRotationX / Y / Z;   // 旋转锁定
};

// 预设组合
UENUM(BlueprintType)
enum class EAxisLockPreset : uint8
{
    None            // 不通过预设控制（组件改用 6 bool）
    StayUpright     // 站立不倒：锁 X/Y 旋转
    YawOnly         // 只绕 Z 轴：锁 X/Y 旋转 + 全部位移
    HorizontalOnly  // 只能水平移动：锁 Z 位移
    FreezeAll       // 完全冻结：6 轴全锁
};
```

> 预设清单在实现前可再调整；映射关系以本表为准。

### 4.2 函数库 `UAxisLockLibrary`（命令式、无状态）

```cpp
// 锁定 6 轴任意组合（底层 SixDOF）
static void LockAxes(UPrimitiveComponent* Target,
    bool bLockPosX, bool bLockPosY, bool bLockPosZ,
    bool bLockRotX, bool bLockRotY, bool bLockRotZ);

// 应用预设
static void ApplyPreset(UPrimitiveComponent* Target, EAxisLockPreset Preset);

// 解除全部锁定（全 false + SetDOFLock）
static void UnlockAll(UPrimitiveComponent* Target);

// 查询当前锁定状态（纯函数）
static FAxisLockState GetLockState(UPrimitiveComponent* Target);

// 是否有任意轴被锁（纯函数）
static bool IsAnyAxisLocked(UPrimitiveComponent* Target);
```

### 4.3 组件 `UAxisLockerComponent`（声明式、有状态）

- 基类 `USceneComponent`（保留与参考插件等价的"挂到谁下面控制谁"工作流）。
- 目标解析顺序：**显式目标引用（若已设置）→ `GetAttachParent()` 的 PrimitiveComponent**。
- 声明式配置：`EAxisLockPreset Preset` + 6 个 bool；**Preset≠None 时用预设，否则用 6 bool**。
- `bAutoApplyOnBeginPlay`（默认 true）：BeginPlay 时自动应用配置。

```cpp
UPROPERTY(EditAnywhere) EAxisLockPreset Preset;
UPROPERTY(EditAnywhere) bool bLockPositionX / Y / Z;
UPROPERTY(EditAnywhere) bool bLockRotationX / Y / Z;
UPROPERTY(EditAnywhere) bool bAutoApplyOnBeginPlay = true;

UFUNCTION(BlueprintCallable) void ApplyConfiguredLock();              // 按配置应用
UFUNCTION(BlueprintCallable) void SetTargetComponent(UPrimitiveComponent* InTarget);
UFUNCTION(BlueprintCallable) void PushLockState();                    // 存当前状态入栈
UFUNCTION(BlueprintCallable) void PopLockState();                     // 弹出并恢复
```

目标引用以 `TWeakObjectPtr<UPrimitiveComponent>` 持有，**每次用时现取 `GetBodyInstance()`**，杜绝悬垂指针。

## 5. 核心机制（数据流）

所有写操作最终走已验证的三步：`取 BodyInstance → 写 6 个 bLockXXX → SetDOFLock(SixDOF)`。

| 操作 | 实现 |
|------|------|
| 锁定 | 6 bool 写入字段 → `SetDOFLock(SixDOF)`（内部 Term 旧约束 + 重建） |
| 解锁 | 6 bool 全 false → `SetDOFLock(SixDOF)`（源码 817 行：全 false 时不建约束 = 解除） |
| 查询 | 纯函数直接读 `BodyInstance` 的 6 个 `bLockXXX` 字段 → 填 `FAxisLockState` |
| 预设 | `EAxisLockPreset` → 映射成 6 bool → 走锁定流程 |
| 临时恢复 | 组件持 `TArray<FAxisLockState>` 栈；Push=`GetLockState` 存入，Pop=弹出并重新应用 |

**解锁明确用"全 false + SetDOFLock(SixDOF)"**，而非 `EDOFMode::None`——复用同一条代码路径，更稳。

## 6. 错误处理

统一走 `FXToolsErrorReporter`，使用 `LogAxisLocker` 日志分类。函数库静态函数**全部前置校验**，根除参考插件的裸指针崩溃：

- 目标为空 / 非 PrimitiveComponent → Warning，安全返回
- `BodyInstance` 为空 → Warning，返回
- 未开启物理模拟（`!IsSimulatingPhysics()`）→ Warning 提示"DOF 锁对非模拟体无效"，但仍写字段（之后开启模拟即生效）

参考插件的根因（`FBodyInstance* BodyInstance;` 未初始化）在本设计中不存在：函数库无该成员；组件用 `TWeakObjectPtr` 现取。

## 7. 跨版本兼容（UE 5.3–5.7）

- `bLockXXX` / `SetDOFLock` / `GetBodyInstance` / `EDOFMode` 在 5.3–5.7 **API 一致，无需版本宏**。
- 5.3 已实测生效；5.4–5.7 靠编译期保证 + `BuildPlugin-MultiUE.ps1` 打包验证。
- 全程 IWYU 显式包含头文件。

## 8. 测试策略

第一期以 **PIE 手动验证**为主（与项目现状一致，不引入依赖 world 的物理单测）：

1. 每个预设在编辑器中跑一遍，观察物理表现是否符合（站立不倒、只绕 Z 转、水平移动、完全冻结）。
2. 手动锁定/解锁/再锁定，确认状态正确切换。
3. 临时锁定 → 改 → 恢复，确认 Push/Pop 还原原配置。
4. 容错路径：传空目标、非物理体、未开模拟，确认不崩溃且有 Warning 日志。

## 9. 待实现时确认的开放项

- `EAxisLockPreset` 具体预设清单与映射（第 4.1 表为默认）。
- 组件是否需要在 `OnRegister`/编辑器下做任何预览（第一期默认不做）。
