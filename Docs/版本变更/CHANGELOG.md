# XTools 更新日志 (CHANGELOG)

## 版本 v1.9.2 (2025-11-17)

<details>
<summary><strong>📋 主要更新</strong></summary>

### 🆕 新增功能
- **X_AssetEditor**: 命名冲突检测系统、变体命名支持、数字后缀规范化、纹理打包后缀支持
- **BlueprintAssist**: 插件启用开关、高级搜索功能、节点展开限制、调试设置
- **BlueprintScreenshotTool**: 完整集成蓝图截图工具，支持多显示器环境

### 🔧 重要修复
- **BlueprintAssist**: 修复晃动节点断开连接后节点不跟随鼠标问题
- **X_AssetEditor**: 修复手动重命名保护机制，解决自动规范化覆盖问题
- **AutoSizeComments**: 修复取消标题样式时无条件应用默认字体大小的问题
- **兼容性**: 修复 UE 5.4 版本 FCompression API 兼容性问题

### 🚀 性能优化
- **BlueprintScreenshotTool**: CPU占用降低约60%，使用BFS避免栈溢出
- **MaterialTools**: 材质函数智能连接优化，支持自动回溯接入
- **第三方插件**: 检测外部插件避免重复加载，提升启动性能

</details>

<details>
<summary><strong>📦 X_AssetEditor 模块</strong></summary>

- **修复** 手动重命名保护机制，基于调用堆栈检测彻底解决自动规范化覆盖问题
- **修复** 数字后缀规范化逻辑，移至重命名流程最终步骤，确保_1正确转换为_01格式
- **增强** 批量重命名功能，即使前缀正确的资产也会检查数字后缀规范化需求
- **增加** 包含蒙太奇通知在内的部分资产前缀映射规则
- **增加** 命名冲突检测系统，自动避免重命名失败
- **增加** 变体命名支持，兼容Allar Style Guide规范
- **增加** 数字后缀规范化，自动转换为两位数格式
- **增加** 纹理打包后缀支持，包含_ERO、_ARM等组合
- **优化** 资产规范化失败后抛出资产详细信息
- **优化** 用户操作上下文检测，支持UE 5.3-5.7版本
- **优化** 重命名逻辑，集成智能冲突解决方案
- **优化** MaterialTools 材质函数智能连接，失败时回溯 MaterialAttributes 链路并自动接入 BaseColor/自发光 节点

</details>

<details>
<summary><strong>🎨 BlueprintAssist 模块</strong></summary>

- **修复** 晃动节点断开连接后节点不跟随鼠标，保持逻辑链连接
- **修复** BlueprintAssistTypes.h与BlueprintAssistUtils.h循环依赖
- **修复** API宏不一致问题，统一使用XTOOLS_BLUEPRINTASSIST_API
- **修复** 宏重定义警告，移除BlueprintAssistSettings.h中的重复宏定义
- **修复** TryCreateConnection调用，使用TryCreateConnectionUnsafe
- **修复** UE 5.4版本 FCompression::GetMaximumCompressedSize API兼容性问题
- **增加** 插件启用开关
- **增加** bSkipAutoFormattingAfterBreakingPins设置，断开引脚时跳过自动格式化
- **增加** ExpandNodesMaxDist设置，限制节点展开的最大水平距离
- **增加** BlueprintAssistDebug调试设置支持
- **调整** 晃动断开连接灵敏度（MinShakeDistance 5→30，DotProduct <0→<-0.5）
- **优化** 启动流程，检测到外部BlueprintAssist插件时集成版保持空载
- **本地化** 所有用户可见文本（100+设置项 + 30+菜单项）

</details>

<details>
<summary><strong>📸 BlueprintScreenshotTool 模块</strong></summary>

- **集成** 蓝图截图工具模块
- **增加** 插件启用开关
- **修复** 插件禁用时的崩溃问题
- **修复** 模块重命名后的API导出宏问题
- **修复** 首次截图节点图标丢失问题
- **优化** 性能：间隔检查替代每帧Tick，CPU占用降低约60%
- **优化** 递归遍历，使用队列(BFS)避免栈溢出
- **优化** 内存管理，使用TUniquePtr替代裸指针
- **优化** DPI获取，支持多显示器环境
- **优化** 保存失败提示的用户体验
- **优化** 错误处理路径，接入 FXToolsErrorReporter 统一上报
- **优化** 工具栏文本，简化"截取截图"为"截图"
- **本地化** 15项设置 + 2个命令 + 3个错误提示

</details>

<details>
<summary><strong>🔧 其他模块更新</strong></summary>

### AutoSizeComments
- **修复** 取消标题样式时无条件应用默认字体大小的问题

### FieldSystemExtensions
- **优化** 默认开启tick

### Sort
- **优化** 冗余结构处理，移除无用元数据参数

### EnhancedCodeFlow
- **修复** 模块重命名后的API导出宏问题
- **修复** FECFHandleBP移动构造函数语法错误

### ComponentTimelineRuntime/Uncooked
- **修复** 模块重命名后的API导出宏问题

### 错误处理系统
- **优化** 统一 XTools 核心工具及部分编辑器模块的错误/关键告警日志到 FXToolsErrorReporter
- **优化** BlueprintExtensionsRuntime、Sort/SortEditor、X_AssetEditor 等模块的错误处理路径，保留调试用 UE_LOG 日志

</details>

<details>
<summary><strong>🔌 第三方插件集成优化</strong></summary>

- **优化** XTools_BlueprintAssist 启动流程，检测到外部 BlueprintAssist 插件启用时集成版保持空载
- **优化** XTools_AutoSizeComments、XTools_ElectronicNodes、XTools_BlueprintScreenshotTool、XTools_SwitchLanguage 在检测到外部插件启用时保持空载
- **优化** XTools_EnhancedCodeFlow 子系统创建逻辑，检测到外部 EnhancedCodeFlow 插件启用时不创建集成版子系统

</details>

---

## 版本 v1.9.1 (2025-11-13)

### 新增
- 集成 BlueprintScreenshotTool 蓝图截图模块，支持快捷键截图与结果通知
- 资产命名系统新增命名冲突检测、变体命名、数字后缀规范化、纹理打包后缀与蒙太奇通知前缀等能力

### 优化
- 完善 UE 5.3–5.7 版本兼容性处理，统一 BlueprintAssist、BlueprintScreenshotTool 等模块的条件编译策略
- 优化资产重命名流程与用户操作上下文检测，提升稳定性和启动性能
- 优化 FieldSystemExtensions 默认行为、Sort 模块冗余结构与 BlueprintScreenshotTool 工具栏显示
- 统一 XTools 核心工具及部分编辑器模块的错误/关键告警日志到 FXToolsErrorReporter
- 调整 BlueprintExtensionsRuntime、Sort/SortEditor、X_AssetEditor 等模块的错误处理路径
- 优化 MaterialTools 材质函数智能连接，失败时回溯 MaterialAttributes 链路并自动接入 BaseColor/自发光 节点

### 修复
- 修复 UE 5.6 GetPasteLocation API 变化导致的 CI 编译错误
- 修复 BlueprintAssist 模块中 FVector2D/FVector2f 类型转换问题
- 修复资产自动重命名功能导致的编辑器崩溃
- 修复模块重命名后各模块 API 导出宏（XTools_ 前缀）不一致问题
- 修复 EnhancedCodeFlow 模块移动构造函数实现错误
- 修复 BlueprintAssist 晃动节点断开连接后节点不跟随鼠标的问题
- 修复 BlueprintScreenshotTool 在插件禁用、首次截图、内存管理、DPI、多显示器和失败提示等场景下的异常
- 修正部分运行时与编辑器工具在参数校验失败时的错误信息不一致问题，避免关键错误被普通日志淹没

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