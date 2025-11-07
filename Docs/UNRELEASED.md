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

## 🐛 问题修复 (Fixed)

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
