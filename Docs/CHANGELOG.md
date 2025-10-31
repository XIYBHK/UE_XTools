# 2025-10-31

## 构建系统更新
- CI 工作流支持 UE 5.0-5.6 版本构建（5.0, 5.2, 5.3, 5.4, 5.5, 5.6）
- 修复 PowerShell 中文字符编码问题，将所有日志消息改为英文，避免乱码和解析错误
- 修复 PowerShell 脚本换行符导致的解析错误，使用 `Write-Host ""` 替代 `` `n``

## 构建系统优化
- 修复 Shipping 构建中 `FXToolsErrorReporter` 的编译错误，使用模板方法支持所有日志分类类型（包括 `FNoLoggingCategory`），该方案已被 UE 社区广泛采用，适用于纯插件项目且零性能开销。
- 修复 CI 工作流产物双重压缩问题（`.zip.zip`），直接上传构建目录，由 GitHub Actions 自动打包。

## CI/CD 最佳实践改进（已通过 Context7 验证符合 GitHub Actions 官方标准）
- 添加并发控制（`concurrency`），防止同一 UE 版本同时运行多个构建，使用 `cancel-in-progress` 自动取消过期构建
- 添加 60 分钟超时保护（`timeout-minutes`），防止构建卡住消耗资源
- 添加详细日志输出和彩色终端提示，提升可读性
- 添加构建时间统计和包大小监控，可观测性提升
- 添加 Job Summary（`GITHUB_STEP_SUMMARY`），直观展示构建结果（版本、耗时、大小等）
- 添加自托管 runner 的清理步骤（`if: always()`），使用 `RUNNER_TEMP` 自动释放临时文件
- 支持 tag 推送自动触发构建（`on: push: tags`）
- 清理冗余空行和 emoji，提升代码可读性和专业性

## 日志系统优化
- 统一各模块日志输出，移除 `LogTemp`，使用模块日志类别或 `FXToolsErrorReporter`，满足 UE 日志最佳实践。
- 补充 `ComponentTimelineRuntime`、`EnhancedCodeFlow` 等模块的日志类别定义。
- 调整材质函数选取 UI 的日志类别，保持编辑器调试输出一致。

## 功能新增
- EnhancedCodeFlow 时间轴（标量/向量/颜色及自定义版本）新增 `PlayRate` 播放速率参数，默认 1.0，可快速整体加速或减缓动画；蓝图节点同步支持。