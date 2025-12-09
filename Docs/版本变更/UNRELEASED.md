# 待发布更新 (UNRELEASED)

> 此文档记录尚未发布的功能更新、修复和移除，用于下次版本发布时的描述参考。
> 发布新版本时，将此文件内容合并到 CHANGELOG.md，然后清空此文件。

---

## 📋 日志格式说明

### 模块分类
按模块名称分为独立章节，每个章节内使用简洁的列表格式记录变更

### 记录格式
```markdown
## 模块名

- 类型 简要描述
- 类型 简要描述
- 类型 简要描述
```

**类型说明**（单字或双字）:
- **增加** / 新增功能
- **优化** / 改进现有功能
- **修复** / 修复问题
- **移除** / 删除功能
- **调整** / 参数或行为调整
- **集成** / 新增模块集成
- **本地化** / 本地化工作
### 内容要求
1. **简洁**：每行记录一个独立变更，描述控制在20字内
2. **清晰**：使用"动词 + 对象 +（效果）"格式
3. **关键信息**：包含重要的参数变化、性能数据、修复的问题
4. **不要**：避免详细技术细节、文件路径、长篇问题分析

### 发布流程
1. 开发过程中持续在此文件顶部添加新记录
2. 版本发布前整理和分类内容
3. 复制内容到 CHANGELOG.md，更新版本号和日期
4. 清空此文件，准备下一轮开发

---
## PointSampling 模块
- 修复 3D 泊松采样球面分布不均匀问题（极点附近过密）
- 修复 Grid 使用 ZeroVector 作为无效标记的潜在冲突问题
- 修复 Depth 较小时 3D 采样仅生成几个点的问题（自动降级为 2D）
- 修复 FromStream 版本 TargetPointCount 与 Radius 同时指定时行为不一致
- 修复 2D 采样时 ApplyJitter 错误扰动 Z 坐标的问题
- 重构 2D/3D 采样核心逻辑为内部函数，统一随机源处理
- 合并 Local/Raw 坐标空间重复代码

## XToolsCore

- **增加** 新增 6 个防御性编程宏（指针/UObject/数组检查）
- **优化** 提升硬件不稳定环境下的代码鲁棒性

## BlueprintExtensions

- **增加** 带延迟的倒序ForLoop节点（K2Node_ForLoopWithDelayReverse）
- **优化** 所有Delay循环节点增加图兼容性检查（仅EventGraph可用）
- **优化** ForLoop/ForEach延迟节点增加编译时引脚有效性检查
- **修复** K2Node 蓝图编译时增加空指针防护，避免硬件异常崩溃
- **优化** ForEachArray/ForEachLoopWithDelay 节点增加引脚有效性检查

## XTools_EnhancedCodeFlow

- **修复** 所有异步 Action 增加 Owner 有效性检查，防止悬空指针
- **修复** 时间轴首次 tick 时初始值触发问题，对齐UE原生行为
- **优化** Owner 销毁后静默跳过回调，避免崩溃（10个 Action）
- **修复** BP 时间轴 OnFinished 回调 bStopped 参数始终为 false
- **修复** ECFTimeline 完成条件使用值比较导致的浮点精度问题
- **增加** 时间轴循环功能（bLoop），对齐UE原生FTimeline实现
- **修复** 时间轴结束时最终值精度问题，确保精确到达终点值
- **优化** 移除Custom时间轴多余的bSuppressCallback机制
- **修复** Loop模式下触发精确终点值并处理溢出时间

## X_AssetEditor

- **修复** 移除 UToolMenus 显式清理调用，符合 UE 标准实践

## CI/CD 工作流

- **修复** update-release-assets 工作流重复删除资产导致 404 错误
- **优化** 并发控制策略，取消旧任务只保留最新任务
- **优化** 下载 artifacts 时去重，避免处理重复文件
- **优化** 改进日志输出格式，添加序号和文件大小信息

## XTools_AutoSizeComments

- **修复** GetNodePos 函数空指针访问导致材质编辑器崩溃

## X_AssetEditor

- **修复** 启动时资产检查阶段误触发自动重命名
- **修复** 模块关闭时的 Lambda 生命周期竞态条件
- **修复** 初始化时跳过延迟保护机制的问题
- **修复** 导入资产缺少重入保护导致的潜在递归
- **修复** Lambda 在模块 Shutdown 后仍执行导致崩溃（3处）
- **修复** OnAssetRenamed 缺少 GEditor 空指针检查
- **优化** 正则表达式性能，使用静态常量避免重复创建
- **优化** 移除 OnAssetRenamed 的冗余回调调用，提升性能

## MaterialTools

- **修复** 添加材质函数后撤销崩溃（移除Transaction避免撤销系统冲突）
- **优化** 移除冗余的ExecuteWithTransaction和PrepareForModification
- **增加** 引入材质常量，减少硬编码
- **优化** 支持并行材质收集，提升批量处理性能
- **优化** 改进智能连接逻辑，引入评分系统解决误判
- **优化** 消除代码重复，统一核心处理逻辑
- **本地化** 全面本地化日志输出为中文
- **修复** EmissiveColor 输出引脚连接失败（添加 Emissive 别名）
- **修复** 材质函数节点位置计算错误，忽略简单常量节点
- **优化** 使用材质主节点实际位置计算新节点坐标

## MapExtensionsLibrary

- **优化** `RemoveEntriesWithValue` 移除 O(N^2) 复杂度，提升至 O(N)
- **修复** 修复 `GenericMap_RemoveEntries` 函数定义不完整导致的编译错误
- **修复** 恢复丢失的 `RemoveEntriesWithValue`, `SetValueAt`, `RandomItem` 函数

## TraceExtensionsLibrary

- **优化** 增加 `TraceChannel` 和 `ObjectType` 的静态缓存，优化字符串查找性能
- **优化** 移除调试日志输出，减少运行时开销

## VariableReflectionLibrary

- **增加** `GetVariableNames` 增加 `bIncludeSuper` 参数，支持获取父类变量
- **修复** 修复文件内容损坏导致的编译错误

## PointSampling

- **增加** 基于泊松圆盘采样的纹理点阵生成功能
- **增加** 多种点阵生成算法（圆形/矩形/三角形/样条线/网格）
- **增加** 3D矩形点阵生成支持
- **重构** 拆分为 Runtime 和 Editor 模块，优化构建配置
- **修复** 非编辑器构建时的多处编译错误
- **修复** K2Node 条件编译和 UHT 解析问题

## XToolsLibrary

- **增加** 递归获取所有子 Actor（BFS）

## PivotTool

- **增加** 静态网格体枢轴点管理功能