# 2025-10-31
- 统一各模块日志输出，移除 `LogTemp`，使用模块日志类别或 `FXToolsErrorReporter`，满足 UE 日志最佳实践。
- 补充 `ComponentTimelineRuntime`、`EnhancedCodeFlow` 等模块的日志类别定义。
- 调整材质函数选取 UI 的日志类别，保持编辑器调试输出一致。
- EnhancedCodeFlow 时间轴（标量/向量/颜色及自定义版本）新增 `PlayRate` 播放速率参数，默认 1.0，可快速整体加速或减缓动画；蓝图节点同步支持。