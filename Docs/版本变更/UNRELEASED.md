# 待发布更新 (UNRELEASED)

> 此文档记录尚未发布的功能更新、修复和移除，用于下次版本发布时的描述参考。
> 发布新版本时，将此文件内容合并到 CHANGELOG.md，然后清空此文件（保留说明部分）。

---

### BlueprintExtensionsRuntime
- 修复 Map 函数边界检查和循环逻辑
- 优化 Map_Identical 算法性能（嵌套循环优化）
- 修复变量反射 API 逻辑错误（ensureAlwaysMsgf）

### EnhancedCodeFlow
- 修复协程生命周期管理（异步任务崩溃）
- 修复时间处理逻辑（累加方式跟踪超时）
- 修复循环模式双重 Tick 问题
- 添加 Owner 有效性检查

### ObjectPool
- 增强统计信息管理（新增 TotalReturned 字段）
- 修复生命周期管理（OnReturnToPool 事件调用）
- 优化 ContainsActor 性能（先检查小容器）
- 移除多余头文件包含（Engine.h）

### SortEditor
- 添加 RebuildDynamicPins 递归调用保护

### FormationSystem
- 优化阵型尺寸比较容差计算（相对容差替代硬编码）
- 添加 Z 维度尺寸检查
- 修复浮点精度问题（Acos/DotProduct Clamp 钳位）
- 移除冗余 ensure() 调用

### PointSampling
- 修复纹理采样缓冲区越界风险（RGBA16/RGBA16F 边界检查）

### RandomShuffles
- 修复通用数组随机采样输出状态不一致
- 使用 thread_local 避免 PRD 状态共享污染

### XTools
- 添加 PRD 测试最大循环次数限制

### FieldSystemExtensions
- 修复空指针二次调用竞态条件（lambda 存储 GetOwner）

### GeometryTool
- 添加不支持的形状类型警告

### X_AssetEditor
- **诊断** 自动规范命名功能在部分导入场景下不触发
  - 问题：拖拽导入、批量导入等场景不触发 OnAssetPostImport
  - 原因：Factory 时间窗机制依赖 OnNewAssetCreated，部分场景不触发该委托
  - 方案：降低 Factory 时间窗权重，添加文件时间戳备用检测
  - 参考资料：`Docs/开发文档/自动规范命名_导入检测问题分析.md`
- 优化 GenerateUniqueAssetName 性能（O(N²) → O(N)）

### BlueprintAssist
- 修复数组访问边界检查（GetLeftSibling 索引越界）

### BlueprintScreenshotTool
- 修复 RenderTarget 空指针检查

---## 📋 日志格式说明

### 模块分类
按模块名称分为独立章节，每个章节内使用简洁的列表格式记录变更

### 记录格式
```markdown
## 模块名
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
