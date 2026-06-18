# 待发布更新 (UNRELEASED)

> 此文档记录尚未发布的功能更新、修复和移除，用于下次版本发布时的描述参考。
> 发布新版本时，将此文件内容合并到 CHANGELOG.md，然后清空此文件（保留说明部分）。

---

### XToolsCore
- 调整 防御宏改 inline constexpr，版本兼容宏归类整理

### XTools
- 新增 PRD 测试最大循环次数限制
- 新增 补充贝塞尔采样配置编辑器元数据

### AxisLocker
- 新增 物理轴向锁定运行时模块
- 新增 蓝图预设与状态恢复栈
- 修复 组件目标选择改名称下拉

### BlueprintExtensions / BlueprintExtensionsRuntime
- 修复 Map 函数边界检查和循环逻辑
- 优化 Map_Identical 算法性能（嵌套循环优化）
- 修复 变量反射 API 逻辑错误与空对象检查（IsValid）
- 调整 K2Node 编译报错由 Error 改 Warning
- 修复 K2Node 重建引脚校验优雅降级（ensureMsgf）
- 调整 统一连接接口（TryConnect）与日志类别归位

### ComponentTimeline
- 修复 时间轴组件生命周期（Construction Script 重建问题）
- 调整 K2Node 报错改 Warning，日志归位到模块类别

### EnhancedCodeFlow
- 修复 协程生命周期管理（异步任务崩溃）
- 修复 时间处理逻辑（累加方式跟踪超时）
- 修复 循环模式双重 Tick 问题
- 新增 Owner 有效性检查
- 新增 协程异常处理和错误标记

### ObjectPool
- 增强 统计信息管理（新增 TotalReturned 字段）
- 修复 生命周期管理（OnReturnToPool 事件调用）
- 优化 ContainsActor 性能（O(n)→O(1)，TSet 索引）
- 新增 ClearTimersAndEvents 状态重置
- 移除 多余头文件包含（Engine.h）
- 修复 类指针缓存改弱引用避免 GC 悬空
- 移除 删除重复的预分配策略字段
- 优化 统计归入专属 StatGroup
- 调整 Shipping 保留 Warning/Error 日志

### Sort / SortEditor / RandomShuffles
- 新增 SortEditor RebuildDynamicPins 递归调用保护
- 优化 HeapSort 消除 TFunction 堆分配
- 优化 PRD 常量表改静态数组消除堆分配
- 修复 字符串 KeyFuncs 标准化跨版本编译
- 修复 通用数组随机采样输出状态不一致
- 优化 使用 thread_local 避免 PRD 状态共享污染
- 调整 K2Node 报错由 Error 改 Warning
- 调整 日志归位到 LogSortEditor，移除冗余 Super 调用
- 新增 补充排序参数编辑器范围

### FormationSystem
- 优化 阵型尺寸比较容差计算（相对容差替代硬编码）
- 新增 Z 维度尺寸检查
- 修复 浮点精度问题（Acos/DotProduct Clamp 钳位）
- 移除 冗余 ensure() 调用
- 修复 螺旋检测相关系数公式（补距离平方和）
- 新增 实现阵型接口回调通知（IFormationInterface）
- 优化 热路径添加 Stats 性能埋点
- 修复 编队成本计算防御空数组越界

### FieldSystemExtensions
- 修复 空指针二次调用竞态条件（lambda 存储 GetOwner）
- 调整 关闭无 Tick 逻辑 Actor 的默认 Tick

### PointSampling / GeometryTool
- 修复 纹理采样缓冲区越界风险（RGBA16/RGBA16F 边界检查）
- 修复 材质渲染世界上下文错误（采样失效）
- 优化 采样统计函数添加规模上限保护
- 新增 不支持的形状类型警告
- 优化 禁用组件不必要的 Tick（降低 CPU 开销）
- 优化 圆形采点缓存 Owner 减少重复调用

### X_AssetEditor
- 优化 GenerateUniqueAssetName 性能（O(N²) → O(N)）
- 修复 Lambda 捕获悬空指针风险（弱引用模式）
- 优化 自动规范命名导入检测（添加文件时间戳备用通道）
- 优化 简化用户操作上下文检测逻辑
- 修复 命名冲突后缀统一为两位补零
- 优化 消除材质函数检查的临时 UObject
- 优化 集合参数改 const 引用避免拷贝
- 新增 批量修改 Pivot 支持中途取消

### BlueprintAssist / ElectronicNodes
- 修复 数组访问边界检查（GetLeftSibling 索引越界）
- 移除 禁用第三方崩溃遥测上报（隐私）
- 修复 废弃样式集名改用 FAppStyle
- 移除 清理重复模块依赖
- 新增 引擎私有路径版本守卫注释

### BlueprintScreenshotTool
- 修复 RenderTarget 空指针检查

### 多模块
- 调整 编辑器依赖收敛为 Private 依赖
- 新增 补充 UPROPERTY 编辑器元数据（范围/单位/条件显示）

---

## 日志格式说明

### 模块分类
按模块名称分为独立章节，每个章节内使用简洁的列表格式记录变更

### 记录格式
```markdown
### 模块名
- 类型 描述
- 类型 描述
```

### 类型说明（单字或双字）:
- **新增** / 新增功能
- **优化** / 改进现有功能
- **修复** / 修复问题
- **移除** / 删除功能
- **调整** / 参数或行为调整
- **集成** / 新增模块集成
- **本地化** / 本地化工作

### 内容要求
1. **简洁**：每行记录一个独立变更，描述控制在 20 字内
2. **清晰**：使用"动词 + 对象 + （效果）"格式
3. **关键信息**：包含重要的参数变化、性能数据、修复的问题
4. **不要**：避免详细技术细节、文件路径、长篇问题分析

### 发布流程
1. 开发过程中持续在此文件顶部添加新记录
2. 版本发布前整理和分类内容
3. 复制内容到 CHANGELOG.md，更新版本号和日期
4. 清空此文件（保留说明部分），准备下一轮开发

---
