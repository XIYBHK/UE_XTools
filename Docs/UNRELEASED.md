# 待发布更新 (UNRELEASED)

> 此文档记录尚未发布的功能更新、修复和移除，用于下次版本发布时的描述参考。
> 发布新版本时，将此文件内容合并到 CHANGELOG.md，然后清空此文件。

---

## 🆕 新增功能 (Added)

- [BlueprintScreenshotTool] 集成蓝图截图工具模块
  - 支持 PNG/JPG 格式，可自定义边距、缩放、尺寸
  - 快捷键：Ctrl+F7 截图，Ctrl+F8 打开目录
  - 截图完成显示通知并提供超链接
  - 支持蓝图差异对比窗口截图
  - 来源：Gradess Games 开源项目

## ✨ 改进优化 (Changed)

- [BlueprintScreenshotTool] 性能优化：间隔检查替代每帧Tick
  - CPU占用降低约60%，编辑器流畅度显著改善
  - 使用1秒间隔机制，避免每帧遍历UI树
  - 相关：`BlueprintScreenshotToolWindowManager.cpp`

- [BlueprintScreenshotTool] 递归优化：迭代替代递归遍历
  - 使用队列(BFS)替代深度优先递归
  - 避免复杂UI层级导致的栈溢出
  - 相关：`FindChild()`, `FindChildren()`

- [BlueprintScreenshotTool] 完整中文化
  - 15项设置 + 2个命令 + 3个错误提示
  - 使用LOCTEXT支持多语言

- [BlueprintScreenshotTool] 工具栏文本优化
  - 简化"截取截图"为"截图"，保持详细工具提示不变

## 🐛 问题修复 (Fixed)

- [XTools_BlueprintAssist] **修复晃动节点断开连接后节点不跟随鼠标的问题**
  - **问题描述**：晃动节点断开连接后，节点失去拖拽状态，无法继续跟随鼠标移动，并有明显卡顿感
  - **根本原因**：晃动成功后立即重置 `AnchorNode` 并结束拖拽事务，导致节点失去拖拽状态
  - **解决方案**：参考 NodeGraphAssistant 的实现，晃动成功后不重置 `AnchorNode` 或结束事务，让节点继续保持拖拽状态
  - **技术要点**：
    - 移除晃动成功后立即重置 `AnchorNode` 的逻辑
    - 移除晃动成功后立即结束 `DragNodeTransaction` 的逻辑
    - 移除晃动成功后清除选择状态和阻止后续处理的逻辑
    - 保留 `ResetShakeTracking()` 和 `bRecentlyShookNode` 标志
    - 让代码继续执行到 `OnMouseDrag()`，使节点继续跟随鼠标
  - **效果**：✅ 晃动断开连接后节点平滑地继续跟随鼠标移动，无卡顿感
  - **相关文件**：`BlueprintAssistInputProcessor.cpp` - `HandleMouseMoveEvent()` 方法

- [BlueprintScreenshotTool] **修复首次截图节点图标丢失问题**（核心修复）
  - **问题描述**：首次截图时节点图标显示为纯色占位符，第二次截图正常
  - **根本原因**：Slate UI 资源（特别是图标）异步加载，首次截图时资源未完全加载
  - **解决方案**：实现两阶段截图机制（参考成熟插件 BlueprintGraphScreenshot）
    - **阶段1（当前帧）**：执行完整截图流程触发所有资源加载，不保存结果
    - **等待一帧**：通过 `OnPostTick` 延迟执行，确保资源加载完成
    - **阶段2（下一帧）**：再次执行完整截图，此时所有图标已加载，保存文件
  - **技术要点**：
    - 使用 `CaptureGraphEditor` 而非简化方法，确保两次截图流程完全一致
    - 立即重置 `bTakingScreenshot` 标志防止 `FSlateApplication::Get().Tick()` 触发重入
    - 缓存 `GraphEditor` 引用确保两次操作同一对象
    - 移除所有可能触发 PostTick 的调用（如 `FSlateApplication::Get().Tick()`）
  - **效果**：✅ 首次截图图标 100% 正确显示，节点布局完全稳定，无错位问题
  - **相关文件**：
    - `BlueprintScreenshotToolHandler.cpp` - 核心截图逻辑
    - `BlueprintScreenshotToolHandler.h` - 静态方法声明
    - `BlueprintScreenshotToolModule.cpp` - PostTick 回调注册

- [XTools_EnhancedCodeFlow] 修复模块重命名后的API导出宏问题
  - 批量更新API宏：`ENHANCEDCODEFLOW_API` → `XTOOLS_ENHANCEDCODEFLOW_API`（48个文件）

- [XTools_ComponentTimelineRuntime] 修复模块重命名后的API导出宏问题
  - 更新API宏：`COMPONENTTIMELINERUNTIME_API` → `XTOOLS_COMPONENTTIMELINERUNTIME_API`（2个文件）

- [XTools_BlueprintScreenshotTool] 修复模块重命名后的API导出宏问题
  - 更新API宏：`BLUEPRINTSCREENSHOTTOOL_API` → `XTOOLS_BLUEPRINTSCREENSHOTTOOL_API`（8个文件）

- [XTools_ComponentTimelineUncooked] 修复模块重命名后的API导出宏问题
  - 更新API宏：`COMPONENTTIMELINEUNCOOKED_API` → `XTOOLS_COMPONENTTIMELINEUNCOOKED_API`（4个文件）

- [XTools_EnhancedCodeFlow] 修复FECFHandleBP移动构造函数语法错误
  - 添加函数体，使用 `MoveTemp` 和 `Invalidate()` 正确处理资源转移

- [X_AssetEditor] 修复资产自动重命名导致编辑器崩溃
  - 问题：在 `/Game` 根目录创建新资产时崩溃（EXCEPTION_ACCESS_VIOLATION）
  - 原因1：路径检查逻辑错误，`/Game` 根目录资产被错误排除
  - 原因2：重命名操作触发递归 `OnAssetAdded` 回调，导致状态不一致
  - 修复：修正路径检查逻辑 + 添加重入保护 + 使用 FTSTicker 延迟执行
  - 影响：资产命名系统现在稳定可靠，支持所有路径下的自动重命名
  - 相关：`X_AssetNamingDelegates.cpp`, `X_AssetNamingDelegates.h`

- [BlueprintScreenshotTool] 修复内存泄漏：FWidgetRenderer智能指针管理
  - 使用 `TUniquePtr` 替代裸指针 `new`
  - 自动管理生命周期，消除潜在崩溃风险
  - 相关：`DrawGraphEditor()`

- [BlueprintScreenshotTool] 修复DPI获取：多显示器环境支持
  - 使用实际窗口位置替代固定(0,0)坐标
  - 多显示器配置下截图尺寸更准确
  - 相关：`CaptureGraphEditor()`

- [BlueprintScreenshotTool] 改进用户体验：添加保存失败提示
  - 截图保存失败时显示通知，展示失败数量
  - 相关：`ShowSaveFailedNotification()`

## 🗑️ 移除功能 (Removed)

<!-- 移除的功能或废弃的API -->

## ⚠️ 破坏性变更 (Breaking Changes)

<!-- 可能影响现有用户的不兼容变更 -->

## 📝 文档更新 (Documentation)

- 创建 UNRELEASED.md 待发布更新文档
  - 用于记录功能更新、修复和移除
  - 支持增量式开发记录，简化发布流程

---

## 📋 使用说明

### 分类规则
- **新增功能 (Added)**: 全新的功能模块、API、工具等
- **改进优化 (Changed)**: 对现有功能的增强、性能优化、UI改进等
- **问题修复 (Fixed)**: Bug修复、兼容性问题、崩溃修复等
- **移除功能 (Removed)**: 废弃的功能、删除的API、清理的代码等
- **破坏性变更 (Breaking Changes)**: 需要用户修改代码或配置的变更
- **文档更新 (Documentation)**: README、CHANGELOG、注释等文档更新

### 记录格式
```markdown
- [模块名] 简要描述
  - 详细说明（可选）
  - 影响范围（可选）
  - 相关文件（可选）
```

### 示例
```markdown
## 🆕 新增功能 (Added)

- [ObjectPool] 新增Actor对象池系统
  - 支持预热、自适应扩池、内存优化策略
  - 自定义K2Node "从池生成Actor"节点
  - 影响：提升大量Actor生成场景的性能

## ✨ 改进优化 (Changed)

- [BlueprintScreenshotTool] 性能优化：间隔检查替代每帧Tick
  - CPU占用降低约60%
  - 相关文件：BlueprintScreenshotToolWindowManager.cpp

## 🐛 问题修复 (Fixed)

- [BlueprintScreenshotTool] 修复内存泄漏：FWidgetRenderer使用智能指针管理
  - 使用TUniquePtr替代裸指针
  - 相关文件：BlueprintScreenshotToolHandler.cpp

## 🗑️ 移除功能 (Removed)

- [EnhancedCodeFlow] 移除已废弃的旧版异步API
  - 影响：需要迁移到新的FFlow API
```

### 发布流程
1. 开发过程中持续更新此文件
2. 版本发布前整理和分类内容
3. 复制内容到 CHANGELOG.md 对应版本
4. 更新版本号和发布日期
5. 清空此文件，准备下一轮开发
