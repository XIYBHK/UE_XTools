# UE C++ 插件编码规则

本文汇总适用于 UE 5.3-5.8 插件开发的稳定规则。项目专用约定以仓库根 `AGENTS.md` 和模块目录内的 `AGENTS.md` 为准。

## 1. 模块边界

- Runtime 模块不得依赖 Editor 或 UncookedOnly 模块。
- K2Node、资产编辑器和 `UnrealEd` API 放在 Editor/UncookedOnly 模块。
- K2Node 需要的运行时逻辑放在对应 Runtime 模块，通过函数库或普通类型调用。
- 公共头暴露的类型通常要求 Public 模块依赖；仅在 `.cpp` 使用的类型优先 Private 依赖。
- 不为解决一个缺失符号加入整组“可能相关”的模块；按头文件和导出符号确认归属。

## 2. IWYU

- 每个源文件显式包含自己使用的类型和 API。
- 头文件能前向声明时优先前向声明，但值成员、继承和 inline 使用通常需要完整类型。
- 生成头 `*.generated.h` 必须是该头文件最后一个 include。
- 不依赖其他头的传递包含，也不通过聚合头掩盖缺失依赖。
- UE 5.4-5.8 使用 BuildPlugin `-StrictIncludes` 验证；UE 5.3 即使脚本不启用该参数，源码也遵循相同原则。

Unity Build 可以用于常规构建性能；诊断隐式 include、ODR 或文件独立性问题时再使用 Non-Unity 构建。不要把 `bUseUnity=false` 设为所有模块的永久通用规则。

## 3. UE 类型与所有权

- UObject 引用使用 `UPROPERTY`/`TObjectPtr` 或适合异步边界的 `TWeakObjectPtr`。
- Slate 共享所有权使用 `TSharedRef`/`TSharedPtr`；反向引用窗口或父控件时优先 `TWeakPtr`。
- 非 UObject 独占资源使用 `TUniquePtr`，共享资源使用 `TSharedPtr`，避免裸 `new/delete`。
- 容器、字符串、名称和本地化文本优先使用 UE 类型：`TArray`、`TMap`、`FString`、`FName`、`FText`。
- 不假定 Actor/UObject 可以从任意线程访问；创建、销毁和大多数状态变更必须回到游戏线程。

## 4. 生命周期与异步

- 跨帧 lambda 不捕获裸 UObject/Actor 指针；捕获弱引用并在执行时重新解析。
- `FTSTicker`、Timer、委托、异步句柄和 Slate 窗口必须有明确注销或销毁路径。
- World、GameInstance 或 Owner 失效后，异步动作应停止或安全跳过回调。
- 不在持锁期间调用可能重入本模块或执行 Blueprint 的外部回调。
- 委托句柄应由拥有者保存，并在生命周期结束前移除。

## 5. 线程安全

- 先识别共享状态和完整状态转换，再选择锁或原子类型。
- “读 → 计算 → 写”必须作为一个原子事务保护；分别给 getter/setter 加锁仍会丢失更新。
- 建立并遵守固定锁顺序，避免同时持有无关锁。
- Unreal 的 UObject/Actor 线程约束不会因容器加锁而消失。
- 并发测试断言最终不变量，并使用确定性输入，不依赖调度顺序。

## 6. Blueprint 与反射

- Blueprint 元数据保持项目约定的中文 `DisplayName`、`ToolTip`、分类和参数显示名。
- 反射类型签名使用 UHT 支持的类型，不在反射边界暴露不受支持的模板或智能指针。
- `BlueprintPure` 不应隐藏可观察的状态修改；需要更新缓存或状态时使用显式 ref 状态或非纯节点。
- K2Node 引脚连接通过 Schema 的 `TryCreateConnection` 或项目 helper 校验，不直接 `MakeLinkTo()`。
- 引脚重建后重新查找 Pin，不复用重建前的指针。
- 展开完成后按父类契约显式断开原节点连接。

## 7. 数值与算法

- 排序比较器必须满足严格弱序；浮点键显式定义 `NaN`、`±Inf` 和相等值语义。
- 随机算法允许固定种子或注入随机源，保证可复现测试。
- 缓存必须说明键、命中、失效、生命周期、线程安全和内存上界。
- 不为低频纯节点引入无生命周期归属的全局缓存。
- 性能结论使用 Unreal Insights 或可复现实验，不写未经测量的百分比。

## 8. 错误处理

- 用户可见错误使用项目统一的 `XTOOLS_LOG_*` 或 `FXToolsErrorReporter`。
- `check` 用于违反后无法继续的程序员错误；`ensure` 用于可恢复但应诊断的契约违反。
- 正常可失败输入返回明确结果，不用断言代替参数校验。
- Shipping 行为应显式确认，不假定日志、检查或自动化测试宏与 Editor 相同。

## 9. 跨版本适配

- 使用 `Source/XToolsCore/Public/XToolsVersionCompat.h` 的宏和兼容封装。
- 只在已证实的 API 差异处条件编译，不预写未验证的未来分支。
- 不在 `.Build.cs` 重复定义引擎版本宏。
- 不通过条件编译掩盖模块边界或缺失 include。
- 每个兼容分支保持相同外部语义，并在最低和最高支持版本验证。

## 10. 测试

- 修复缺陷先建立能复现风险的确定性测试，再修改生产代码。
- 纯逻辑用 Simple Test；异步用 Spec/Latent；World、Slate、导航和 Blueprint 使用真实 fixture。
- 测试公开行为，不复制生产算法作为期望值。
- 报告必须检查预期数量、`failed`、`notRun` 和 `inProcess`，不能只看进程退出码。
- 发布前验证 Editor、Game Development、Shipping 和 UE 5.3-5.8 BuildPlugin 矩阵。

## 11. 变更范围

- 修改只覆盖任务需要的代码和由本次改动产生的孤儿。
- 不顺手重构邻接模块或删除与任务无关的既有死代码。
- 公共 UCLASS/USTRUCT/UFUNCTION 删除前检查 C++、Blueprint 资产和序列化兼容面。
- 完成态计划、一次性审查流水和构建统计不作为长期文档保留。

相关文档：

- [常见错误检查清单](常见错误检查清单.md)
- [UE 代码审查与实证核验指南](../开发文档/UE代码审查与实证核验指南.md)
- [UE 全自动测试最佳实践](../测试相关/UE全自动测试最佳实践.md)
- [跨版本条件编译](../打包相关/UE_跨版本条件编译_问题与解决方案.md)
