# BlueprintExtensions - K2Node 编辑器模块

UncookedOnly。14+ 自定义蓝图节点，运行时函数在 BlueprintExtensionsRuntime。

## STRUCTURE

```
Public/
├── K2Nodes/                # 14+ K2Node 类
│   ├── K2Node_CasePairedPinsNode.h  # 基类：多分支配对Pin (MultiBranch/MultiConditionalSelect/ConditionalSequence)
│   ├── K2Node_Assign.h              # 通配符赋值 (FKCHandler_Assign)
│   ├── K2Node_ForEachLoopWithDelay.h # 参考实现：延迟循环
│   ├── K2Node_ForEachMap.h           # Map遍历 (Key/Value通配符)
│   ├── K2Node_ForEachSet.h           # Set遍历
│   ├── K2Node_Map*.h                 # Map操作 (Add/Remove/Find/Identical/Append)
│   └── K2Node_Multi*.h              # 多分支控制流
├── K2NodePinTypeHelpers.h  # Pin类型推断+传播工具
└── K2NodeSafetyHelpers.h   # 安全校验工具
Private/
└── K2Nodes/K2NodeHelpers.h # 核心Helper (TryConnect/BeginExpandNode/EndExpandNode)
```

## K2NODE EXPANDNODE PATTERN (强制)

```cpp
void ExpandNode(FKismetCompilerContext& Ctx, UEdGraph* Graph) override
{
    // 1. 入口校验
    if (!K2NodeHelpers::BeginExpandNode(Ctx, this, {RequiredPins...}, ErrMsg))
        return;

    // 2. 创建中间节点
    auto* Call = Ctx.SpawnIntermediateNode<UK2Node_CallFunction>(this, Graph);
    Call->SetFromFunction(Func);
    Call->AllocateDefaultPins();

    // 3. 连线 (只用 TryConnect)
    K2NodeHelpers::TryConnect(Ctx, SrcPin, DstPin);

    // 4. 迁移原始连线
    Ctx.MovePinLinksToIntermediate(*OldPin, *NewPin);

    // 5. 收尾断链
    K2NodeHelpers::EndExpandNode(this);
}
```

## COMPILATION PATTERN (FNodeHandlingFunctor)

Assign/MultiBranch/MapAdd 等使用 `CreateNodeHandler()` 返回自定义 `FNodeHandlingFunctor`。
需在 `RegisterNets()` 注册终端，`Compile()` 生成 KCST 语句。

## WILDCARD TYPE PROPAGATION

1. `NotifyPinConnectionListChanged()` 检测类型变化
2. `FK2NodePinTypeHelpers::ResetPinToWildcard()` 重置
3. 有连接→从容器Pin推断元素类型；无连接→保留已序列化类型
4. 类型变化后调 `GetGraph()->NotifyGraphChanged()`
5. 存储已解析类型到 `#if WITH_EDITORONLY_DATA` 属性

## PRAGMA REGION 组织 (建议)

```cpp
#pragma region PinManagement
#pragma region NodeAppearance
#pragma region BlueprintCompile
#pragma region NodeProperties
```

## REVIEW PRIORITY

- P0: 崩溃/空指针 (重建后Pin、中间节点创建失败)
- P1: 执行流错误 (Break→Completed路径、Delay语义)
- P2: 类型传播 (Wildcard回退、容器类型不一致)
- P3: 风格不一致但功能正确
