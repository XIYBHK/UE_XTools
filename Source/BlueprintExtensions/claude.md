# BlueprintExtensions 可复用规则

适用目录：`Source/BlueprintExtensions`
目标：新增/修改 K2 节点时，统一实现风格，降低边界 bug 与回归风险。

## 1. 连接规则（强制）
- 统一使用 `K2NodeHelpers::TryConnect(CompilerContext, A, B)`。
- 禁止在新代码中直接使用 `MakeLinkTo`。
- 禁止在新代码中直接散落 `CompilerContext.GetSchema()->TryCreateConnection(...)`。

## 2. ExpandNode 骨架（强制）
- 入口先调用 `K2NodeHelpers::BeginExpandNode(...)` 校验必需引脚。
- 中间节点展开期间，不调用 `Super::ExpandNode(...)`（避免基类提前断链）。
- 末尾统一调用 `K2NodeHelpers::EndExpandNode(this)` 收尾断链。
- 失败分支（如关键输入未连接）要记录 `MessageLog`，并终止展开。

## 3. 重建后取 Pin（强制）
- 任何 `PostReconstructNode()` 之后，必须重新获取 pin。
- 统一使用 `K2NodeHelpers::ReconstructAndFindPin(Node, PinName)`。
- 不要复用重建前缓存的 pin 指针。

## 4. 类型传播与 Wildcard（建议）
- 无连接时，优先保留已序列化恢复的确定类型；仅在确实是 wildcard 时重置。
- 有连接时，优先从容器输入引脚推断元素类型。
- 引脚类型变化后再触发 `GetGraph()->NotifyGraphChanged()`。

## 5. Delay 语义（循环节点）
- 统一语义：`Delay <= 0` 等同无延迟路径。
- 仅 `Delay > 0` 进入 `Delay` 节点。
- 保证执行顺序为：循环体 ->（可选延迟）-> 计数器更新 -> 下一轮判断。

## 6. 结构一致性（建议）
- 按固定段落组织：参数校验 -> 中间节点创建 -> 连线 -> MovePinLinks -> EndExpandNode。
- 减少“同功能多写法”（连接方式、重建时序、收尾逻辑）。

## 7. 变更自检清单（提交前）
- 检查是否仍有新增 `MakeLinkTo`。
- 检查是否有 `PostReconstructNode` 后未重新取 pin。
- 检查 `Break` 路径是否能正确流向 `Completed`。
- 编译验证：
  - `D:/UE/UE_5.3/Engine/Build/BatchFiles/Build.bat cpp0623Editor Win64 Development -Project=D:/Github/cpp0623/cpp0623.uproject -FromMsBuild`

## 8. 审查优先级
- P0：崩溃/空指针/野指针（尤其是重建后 pin）。
- P1：执行流错误（Break、Completed、Delay 路径）。
- P2：类型传播错误（Wildcard 回退、容器类型不一致）。
- P3：风格不一致但功能正确。
