# 2025-11-01

## 多版本兼容性方案（一套代码支持 UE 5.3-5.6）

### 版本支持策略
- **支持版本**: UE 5.3, 5.4, 5.5, 5.6
- **不支持**: UE 5.0-5.2（详见下方原因）
- **方法**: 使用条件编译处理 API 差异，遵循 UE 官方最佳实践

### UE 5.0/5.2 移除原因
- **UE 5.0**: 需要额外的 .NET Desktop Runtime 3.1.x 依赖，增加部署复杂度
- **UE 5.2**: 引擎源码 `ConcurrentLinearAllocator.h` 与新版 VS 2022 (14.44+) 存在兼容性 Bug（C4668/C4067），无法在插件层面修复
- **业界做法**: 大多数商业插件也跳过有兼容性问题的旧版本，专注于稳定版本

### 跨版本兼容性实现
- **UE 5.5 API 变更**: 使用条件编译 `#if ENGINE_MINOR_VERSION >= 5` 处理 `TArray::Pop()` 参数变更
- **版本宏定义**: 在 Build.cs 中自动设置 `ENGINE_MAJOR_VERSION` 和 `ENGINE_MINOR_VERSION`
- **编译时选择**: 预处理器自动选择正确 API，零运行时开销

### CI/CD 优化
- 修复 PowerShell 中文字符编码问题，所有日志改为英文
- 修复 PowerShell 换行符解析错误
- 修复产物双重压缩问题（`.zip.zip`）

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