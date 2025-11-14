# XTools 更新日志 (CHANGELOG)

## 版本 v1.9.1 (2025-11-13)

### 新增
- 集成 BlueprintScreenshotTool 蓝图截图模块，支持快捷键截图与结果通知
- 资产命名系统新增命名冲突检测、变体命名、数字后缀规范化、纹理打包后缀与蒙太奇通知前缀等能力

### 优化
- 完善 UE 5.3–5.7 版本兼容性处理，统一 BlueprintAssist、BlueprintScreenshotTool 等模块的条件编译策略
- 优化资产重命名流程与用户操作上下文检测，提升稳定性和启动性能
- 优化 FieldSystemExtensions 默认行为、Sort 模块冗余结构与 BlueprintScreenshotTool 工具栏显示

### 修复
- 修复 UE 5.6 GetPasteLocation API 变化导致的 CI 编译错误
- 修复 BlueprintAssist 模块中 FVector2D/FVector2f 类型转换问题
- 修复资产自动重命名功能导致的编辑器崩溃
- 修复模块重命名后各模块 API 导出宏（XTools_ 前缀）不一致问题
- 修复 EnhancedCodeFlow 模块移动构造函数实现错误
- 修复 BlueprintAssist 晃动节点断开连接后节点不跟随鼠标的问题
- 修复 BlueprintScreenshotTool 在插件禁用、首次截图、内存管理、DPI、多显示器和失败提示等场景下的异常

---

##  版本 v1.9.0 (2025-11-06)

### 新增
- 集成 AutoSizeComments、BlueprintAssist、ElectronicNodes 三个编辑器增强插件
- 为集成插件提供完整中文化配置和默认设置优化，提升开箱即用体验

### 优化
- 完善 XTools 版本宏系统与跨 UE 5.3–5.6 的兼容性处理
- 优化材质工具、采样等相关模块的 API 使用与实现细节，提升可维护性与性能

### 修复
- 修复 K2Node 通配符引脚类型丢失问题（ForEachArray/Map/Set 等节点），确保编辑器重启后类型保持正确
- 修复 BlueprintAssist、ElectronicNodes 等第三方插件在 UE 5.0+ 与 5.6 下的编译错误
- 修复 FieldSystemExtensions、BlueprintExtensionsRuntime 等模块在新版本引擎下的警告与编译问题

---

# 2025-11-05

## 兼容性与采样工具

- 修复 XTools 采样功能在 UE 5.4–5.6 下的头文件依赖与 API 兼容问题
- 修复 FieldSystemExtensions 在 UE 5.6 下 BufferCommand 弃用导致的兼容性问题
- 新增基于 GeometryCore 的原生表面采样模式，显著提升采样性能
- 修复采样目标误判、Noise 应用错误、除零和整型溢出等崩溃风险
- 改进错误信息、参数校验和调试日志，便于定位采样问题

## BlueprintExtensions 与 MaterialTools

- 优化 MaterialTools 核心 API 使用和智能连接系统，实现更高性能与更好可维护性
- 优化 PointSampling 模块算法和内存使用，提升大规模点云处理性能
- 完善 XTools 采样工具的可视化调试与运行时兼容性

---

# 2025-11-04

## FieldSystemExtensions 模块

- 新增 FieldSystemExtensions 模块与 AXFieldSystemActor，提供高性能 Chaos / GeometryCollection 筛选能力
- 支持按对象类型、Actor 类 / Tag、运行时过滤等多种筛选方案
- 提供 UXFieldSystemLibrary 辅助函数，简化筛选器创建与复用

## 蓝图循环与本地化

- 新增带延迟的 ForLoop / ForEach 蓝图节点，支持逐帧 / 渐进式逻辑
- 为循环节点补充中英文搜索关键词，提升节点检索体验
- 重构 BlueprintExtensions 模块架构（Runtime + UncookedOnly），统一节点分类与本地化

---

# 2025-11-01

## 多版本支持与构建系统

- 明确支持 UE 5.3–5.6 的版本策略，完善关键 API 的条件编译处理
- 修复 Shipping 构建错误，统一日志类别并改进错误上报
- 优化 CI/CD 工作流：修复编码和压缩问题，增加并发控制和构建统计
- 为 EnhancedCodeFlow 时间轴新增 PlayRate 播放速率参数（默认 1.0）