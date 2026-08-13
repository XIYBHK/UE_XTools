# SplineMovement 模块开发接手文档

## 📋 项目概述

将蓝图宏「沿样条线移动」（4个版本，最多84个节点）封装为C++异步节点，支持 `AddMovementInput` 和 `AIMoveTo` 双模式。

**目标**：完全还原蓝图宏的交互体验，包括"中断"输入引脚。

---

## ✅ 已完成部分

### 1. SplineMovement Runtime 模块（已完成并可编译）

**文件列表**：
```
Source/SplineMovement/
├── SplineMovement.Build.cs
├── Public/
│   ├── SplineMovementLog.h
│   ├── SplineMovementModule.h
│   └── SplineMoveAlongAction.h        # 核心异步节点
└── Private/
    ├── SplineMovementLog.cpp
    ├── SplineMovementModule.cpp
    └── SplineMoveAlongAction.cpp     # 核心算法实现
```

**核心类**：`USplineMoveAlongAction : public UBlueprintAsyncActionBase`

**工厂函数签名**：
```cpp
UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
    meta = (BlueprintInternalUseOnly = "true", DisplayName = "沿样条线移动"))
static USplineMoveAlongAction* SplineMoveAlong(
    UPARAM(DisplayName = "角色") APawn* Pawn,
    UPARAM(DisplayName = "样条线") USplineComponent* Spline,
    UPARAM(DisplayName = "刷新距离") float LookaheadDistance = 200.f,
    UPARAM(DisplayName = "向右偏移率") float RightOffsetRate = 0.f,
    UPARAM(DisplayName = "输入权重") float InputWeight = 1.f,
    UPARAM(DisplayName = "反向") bool bReverse = false,
    UPARAM(DisplayName = "移动模式") ESplineMoveMode MoveMode = ...);
```

**输出引脚（UPROPERTY BlueprintAssignable）**：
- `Then` - 首帧触发
- `OnTick(FVector TargetLocation, float DistanceAlongSpline)` - 每帧触发
- `OnSuccess` - 到达终点
- `OnInterrupted` - 中断时触发

**中断函数**：
```cpp
UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动")
void Interrupt();
```

**算法核心**（OnTicker）：
```cpp
每帧:
1. 找到 Pawn 在样条上的最近点 NearestDist
2. 计算前瞻目标 TargetDist = NearestDist + LookaheadDist * (反向 ? -1 : 1)
3. 获取目标位置（含右向偏移）
4. 驱动移动：
   - AddMovementInput 模式：Dir = Normalize2D(TargetPos - PawnPos) → AddMovementInput
   - AIMoveTo 模式：AIController->MoveToLocation(TargetPos)
5. 到达检测：NearestDist >= SplineLength - ArrivalThreshold
6. 广播 OnTick(TargetPos, TargetDist)
```

**已注册到**：
- `XTools.uplugin` - SplineMovement Runtime 模块
- `Docs/版本变更/UNRELEASED.md` - 3条变更记录

---

## ⏳ 待完成部分

### 2. SplineMovementEditor 模块（未完成）

**问题**：当前节点**缺少"中断"输入执行引脚**，用户需要手动保存节点返回值再调用 `Interrupt()`，不如蓝图宏直观。

**解决方案**：创建自定义 K2Node，添加"中断"输入引脚。

**文件结构**（需创建）：
```
Source/SplineMovementEditor/
├── SplineMovementEditor.Build.cs      ✅ 已创建
├── Public/
│   ├── SplineMovementEditorModule.h   ❌ 待创建
│   └── K2Node_SplineMoveAlong.h       ❌ 待创建
└── Private/
    ├── SplineMovementEditorModule.cpp ❌ 待创建
    └── K2Node_SplineMoveAlong.cpp     ❌ 待创建
```

**需注册到**：
- `XTools.uplugin` - 添加 SplineMovementEditor UncookedOnly 模块

---

## 🎯 K2Node 设计方案

### 引脚布局
```
输入执行引脚:
├─ Execute (开始)
└─ 中断

输入数据引脚:
├─ Pawn
├─ Spline  
├─ Lookahead Distance
├─ Right Offset Rate
├─ Input Weight
├─ Reverse
└─ Move Mode

输出执行引脚:
├─ Then
├─ 持续执行 (携带 Target Location, Distance Along Spline)
├─ 移动成功
└─ 移动中断
```

### 技术挑战与方案

**挑战 1**：`UBlueprintAsyncActionBase` 自动生成的节点（通过 `UK2Node_BaseAsyncTask`）只有一个隐式的 Execute，无法添加额外输入引脚。

**挑战 2**：委托输出引脚（Then, OnTick, OnSuccess, OnInterrupted）的绑定需要 `UK2Node_BaseAsyncTask` 的特殊机制（`UK2Node_AddDelegate` + 事件生成）。

**方案选择**：

#### 方案 A：纯 K2Node（不继承 UK2Node_BaseAsyncTask）
- ✅ 完全控制引脚布局
- ❌ 需手动实现委托→执行引脚的绑定逻辑（复杂，~300行）
- 参考：`K2Node_ForLoopWithDelay` 模式

#### 方案 B：继承 UK2Node_BaseAsyncTask + 扩展
- ✅ 自动处理委托绑定
- ❌ `Super::ExpandNode()` 会调用 `BreakAllNodeLinks()`，难以在之后获取 proxy 引用
- ❌ 需要深入 UE 内部机制

#### 方案 C（推荐）：参考 `K2Node_SpawnActorFromPool` 模式
- ✅ 继承 `UK2Node_SpawnActorFromClass`（SpawnActor 的节点基类），已有成熟参考
- ✅ 完全重写 `ExpandNode`，控制中间节点生成
- ✅ 项目内已有成功案例

---

## 📚 关键参考文件

### 项目内参考
1. **`Source/ObjectPoolEditor/Public/K2Node_SpawnActorFromPool.h`** - 继承 `UK2Node_SpawnActorFromClass`，完整 ExpandNode 实现
2. **`Source/BlueprintExtensions/Private/K2Nodes/K2Node_ForLoopWithDelay.cpp`** - Break 输入引脚模式，SingleFlightExecutionGuard
3. **`Source/BlueprintExtensions/Private/K2Nodes/K2NodeHelpers.h`** - 工具函数：`TryConnect`, `CreateSingleFlightExecutionGuard`, `RegisterNode`

### UE 官方参考
- `Engine/Source/Editor/BlueprintGraph/Classes/K2Node_BaseAsyncTask.h` - 异步任务节点基类
- `Engine/Source/Editor/BlueprintGraph/Private/K2Node_BaseAsyncTask.cpp` - ExpandNode 委托绑定逻辑

---

## 🔨 实现步骤（推荐方案C）

### Step 1: 创建 K2Node 类
```cpp
// K2Node_SplineMoveAlong.h
UCLASS()
class UK2Node_SplineMoveAlong : public UK2Node
{
    GENERATED_BODY()
    
    // UK2Node 接口
    virtual void AllocateDefaultPins() override;
    virtual void ExpandNode(FKismetCompilerContext&, UEdGraph*) override;
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar&) const override;
    virtual FText GetNodeTitle(ENodeTitleType::Type) const override;
    virtual FText GetMenuCategory() const override;
    
    // 引脚获取
    UEdGraphPin* GetExecutePin() const;
    UEdGraphPin* GetInterruptPin() const;
    UEdGraphPin* GetThenPin() const;
    UEdGraphPin* GetOnTickPin() const;
    UEdGraphPin* GetOnSuccessPin() const;
    UEdGraphPin* GetOnInterruptedPin() const;
    // ... 参数引脚获取函数
    
private:
    FNodeTextCache CachedNodeTitle;
};
```

### Step 2: AllocateDefaultPins
```cpp
void UK2Node_SplineMoveAlong::AllocateDefaultPins()
{
    // 输入执行引脚
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, TEXT("中断"));
    
    // 输入数据引脚（参考 SplineMoveAlong 工厂函数参数）
    UEdGraphPin* PawnPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, APawn::StaticClass(), TEXT("Pawn"));
    PawnPin->PinFriendlyName = FText::FromString(TEXT("角色"));
    
    // ... 创建所有参数引脚
    
    // 输出执行引脚
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TEXT("持续执行"));
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TEXT("移动成功"));
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TEXT("移动中断"));
    
    // 持续执行的输出数据引脚
    UEdGraphPin* OnTickPin = GetOnTickPin();
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), TEXT("Target Location"))->ParentPin = OnTickPin;
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float, TEXT("Distance Along Spline"))->ParentPin = OnTickPin;
}
```

### Step 3: ExpandNode（核心逻辑）

**设计思路**：
```
Execute → CallFunction(SplineMoveAlong) → TempVar(Proxy) → CallFunction(Activate)
                                                          ├→ BindDelegate(Then) → ThenPin
                                                          ├→ BindDelegate(OnTick) → 持续执行Pin
                                                          ├→ BindDelegate(OnSuccess) → 移动成功Pin
                                                          └→ BindDelegate(OnInterrupted) → 移动中断Pin

中断 → TempVar.Get → CallFunction(Interrupt)
```

**关键代码框架**：
```cpp
void UK2Node_SplineMoveAlong::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    using namespace K2NodeHelpers;
    
    // 1. 创建 SplineMoveAlong 调用节点
    UK2Node_CallFunction* CallCreateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    CallCreateNode->SetFromFunction(USplineMoveAlongAction::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(USplineMoveAlongAction, SplineMoveAlong)));
    CallCreateNode->AllocateDefaultPins();
    
    // 2. 创建临时变量存储 Proxy
    UK2Node_TemporaryVariable* ProxyVar = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
    ProxyVar->VariableType.PinCategory = UEdGraphSchema_K2::PC_Object;
    ProxyVar->VariableType.PinSubCategoryObject = USplineMoveAlongAction::StaticClass();
    ProxyVar->AllocateDefaultPins();
    
    // 3. 赋值 Proxy
    UK2Node_AssignmentStatement* AssignProxy = ...;
    TryConnect(CompilerContext, CallCreateNode->GetReturnValuePin(), AssignProxy->GetValuePin());
    TryConnect(CompilerContext, ProxyVar->GetVariablePin(), AssignProxy->GetVariablePin());
    
    // 4. 调用 Activate
    UK2Node_CallFunction* CallActivate = ...;
    TryConnect(CompilerContext, ProxyVar->GetVariablePin(), CallActivate->GetSelfPin());
    
    // 5. 绑定委托（复杂部分）
    // 参考 UK2Node_BaseAsyncTask::ExpandNode 的委托绑定逻辑
    // 或使用 UK2Node_AddDelegate + UK2Node_CustomEvent
    
    // 6. 处理中断引脚
    UK2Node_CallFunction* CallInterrupt = ...;
    TryConnect(CompilerContext, ProxyVar->GetVariablePin(), CallInterrupt->GetSelfPin());
    CompilerContext.MovePinLinksToIntermediate(*GetInterruptPin(), *CallInterrupt->GetExecPin());
    
    // 7. 移动原始引脚连接
    CompilerContext.MovePinLinksToIntermediate(*GetExecutePin(), *CallCreateNode->GetExecPin());
    // ... 移动所有参数引脚
    
    // 8. 断开原节点
    BreakAllNodeLinks();
}
```

### Step 4: 注册模块
```cpp
// XTools.uplugin
{
    "Name": "SplineMovementEditor",
    "Type": "UncookedOnly",
    "LoadingPhase": "PostDefault",
    "PlatformAllowList": ["Win64", "Mac", "Linux"],
    "AdditionalDependencies": ["SplineMovement"]
}
```

---

## ⚠️ 已知问题与注意事项

### 1. UHT 限制
- ❌ 委托宏参数名不能用中文：`DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSplineMoveTick, FVector, 当前前往位置, ...)` 会编译失败
- ✅ 必须用英文参数名：`FVector, TargetLocation, float, DistanceAlongSpline`
- ✅ 显示名通过 `UPARAM(DisplayName="...")` 实现

### 2. 委托绑定复杂度
- `UK2Node_BaseAsyncTask::ExpandNode` 的委托绑定逻辑约150行，涉及：
  - 为每个委托创建 `UK2Node_AddDelegate` 节点
  - 创建对应的 `UK2Node_CustomEvent` 节点
  - 生成事件调度逻辑
- 如果不想重新实现，可考虑：
  - 继承 `UK2Node_BaseAsyncTask` 并想办法扩展（困难）
  - 或简化设计：只保留核心输出引脚，其余通过其他方式暴露

### 3. Ticker vs Latent
- Runtime 使用 `FTSTicker`（C++ 异步），不是 UE Latent 节点
- K2Node 的 ExpandNode 不需要 `IsLatentGraphCompatible` 检查
- 但需要确保在事件图中使用（异步节点特性）

---

## 🧪 测试计划

1. **基础功能测试**：
   - 创建样条线组件
   - 放置 Character
   - 调用"沿样条线移动"节点
   - 验证移动轨迹正确

2. **中断测试**：
   - 移动过程中触发"中断"引脚
   - 验证 `OnInterrupted` 引脚触发
   - 验证移动立即停止

3. **双模式测试**：
   - AddMovementInput 模式 - 玩家控制角色
   - AIMoveTo 模式 - AI Pawn

4. **边界测试**：
   - 反向移动
   - 横向偏移
   - 样条长度为0
   - Pawn/Spline 运行时失效

---

## 📖 文档位置

- **本文档**：`Docs/开发日志/SplineMovement接手文档.md`
- **CLAUDE.md 已更新**：K2Node 开发要点章节
- **UNRELEASED.md 已更新**：3条变更记录

---

## 🔗 快速命令

```bash
# 编译测试（UE 编辑器关闭状态）
.\Scripts\BuildPlugin-MultiUE.ps1 -Follow

# 或使用 Live Coding（UE 编辑器运行状态）
# 直接在 VS Code 中保存文件后触发

# 清理中间文件
.\Scripts\Clean-UEPlugin.ps1
```

---

## 📞 联系与问题

如遇到问题，请参考：
1. 项目内 `Source/BlueprintExtensions/Private/K2Nodes/` 下的其他 K2Node 实现
2. `Source/ObjectPoolEditor/Private/K2Node_SpawnActorFromPool.cpp` - 最接近的参考实现
3. UE 官方文档：Custom K2Node Creation

**最后更新**：2026-08-07
**状态**：Runtime 与 Editor 模块均已完成，UE 5.3 编译验证通过

## 2026-08-07 蓝图逻辑核验结果

以导出文件 `D:\BaiduNetdiskDownload\AssetLibrary\Saved\XTools\BlueprintExports\_Game_蓝图功能库_函数库_MarL_沿样条线移动\MarL_沿样条线移动.md` 中的 `MarL_沿样条线移动v4` 为主契约，当前 C++ 节点已对齐以下逻辑：

- 首次 Tick 计算 Pawn 的最近样条距离，并保存为持久化的“当前长度”。
- 每轮按 `当前长度 + 刷新距离 * (反向 ? -1 : 1)` 计算目标距离。
- 正向在下一个距离到达样条长度时成功；反向在当前长度小于等于 0 时成功。
- 目标位置使用 `样条位置 + 右向量 * Scale.Y * 向右偏移率`。
- 目标点与 Pawn 的二维距离小于等于刷新距离时推进当前长度；移动输入每轮仍执行。
- 事件顺序为 `Then（仅一次） -> 移动 -> 持续执行`；中断在下一次 Tick 广播“移动中断”。

节点保留了原宏没有的 C++ 扩展：`InputWeight` 和 `AIMoveTo` 移动模式。默认值（AddMovementInput、权重 1.0、刷新距离 100）与 v4 的 AddMovementInput 路径一致。
