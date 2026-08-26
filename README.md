# XTools

XTools 是面向 Unreal Engine 5.3-5.8 的模块化蓝图与编辑器工具插件。插件包含运行时功能库、K2Node 编辑器扩展、资产处理工具，以及若干经过本地化维护的第三方编辑器模块。

## 环境要求

- Unreal Engine 5.3-5.8
- Windows（当前发布与自动化构建目标）
- C++ 项目或可编译插件的宿主项目
- 插件依赖以 [`XTools.uplugin`](XTools.uplugin) 为准

插件声明的引擎插件依赖包括 `EditorScriptingUtilities`、`PythonScriptPlugin`、`Niagara`、`RigVM`，以及可选的 `AssetSearch`。

## 安装

将仓库放入项目的 `Plugins/UE_XTools` 目录，重新生成项目文件并编译 Editor Target。启用插件后重启编辑器。

```text
<Project>/
└─ Plugins/
   └─ UE_XTools/
      ├─ Source/
      ├─ Scripts/
      └─ XTools.uplugin
```

## 模块概览

插件由 26 个模块组成。模块类型和加载阶段的权威来源是 [`XTools.uplugin`](XTools.uplugin)。

### 运行时模块

| 模块 | 职责 |
| --- | --- |
| `XToolsCore` | 版本兼容、统一日志与防御性宏 |
| `XTools` | 插件入口与通用运行时工具 |
| `AxisLocker` | 物理轴向锁定与状态恢复 |
| `PointSampling` | 泊松、网格、样条和阵型采样 |
| `GeometryTool` | 几何辅助算法 |
| `RandomShuffles` | 随机洗牌、权重采样和 PRD |
| `FormationSystem` | 编队生成与移动控制 |
| `SplineMovement` | 样条移动和 AI 导航模式 |
| `QueueSpline` | 队列样条状态与重入隔离派发 |
| `XTools_ComponentTimelineRuntime` | 组件时间轴运行时 |
| `XTools_EnhancedCodeFlow` | 延迟、时间轴、异步流程与协程 |
| `Sort` | 基础类型、Actor 和反射属性排序 |
| `ObjectPool` | Actor 对象池、预热、批量操作与生命周期事件 |
| `BlueprintExtensionsRuntime` | 蓝图扩展节点的运行时函数库 |
| `FieldSystemExtensions` | Chaos 场系统筛选与辅助功能 |

### 编辑器与 UncookedOnly 模块

- `BlueprintExtensions`、`ObjectPoolEditor`、`SortEditor`
- `SplineMovementEditor`、`XTools_ComponentTimelineUncooked`
- `X_AssetEditor`、`XTools_SwitchLanguage`
- `XTools_AutoSizeComments`、`XTools_BlueprintAssist`
- `XTools_ElectronicNodes`、`XTools_BlueprintScreenshotTool`

Runtime 模块不得依赖 Editor 或 UncookedOnly 模块。K2Node 应将运行时逻辑下沉到对应 Runtime 模块。

## 主要能力

- 蓝图循环、容器、反射和流程控制节点
- 基础类型、自然字符串、Actor 和通用属性排序
- 固定种子采样、权重随机和 PRD 状态管理
- 泊松、矩形网格、样条、静态网格和阵型采样
- Actor 对象池、批量生成/归还、延迟生成和统计
- 样条移动、AI 寻路、编队与队列移动
- Chaos Field、几何、轴向锁定和组件时间轴工具
- 碰撞、材质、命名、截图和蓝图编辑器辅助工具

具体节点和当前变更请查阅 [`Docs/版本变更/BLUEPRINT_NODES.md`](Docs/版本变更/BLUEPRINT_NODES.md) 与 [`Docs/版本变更/UNRELEASED.md`](Docs/版本变更/UNRELEASED.md)。

## 构建与测试

优先使用项目 `.vscode/tasks.json` 中的构建任务。多版本插件打包使用：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 -Follow
```

指定引擎目录时，每个路径作为独立参数传入：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots @(
    '<UE_5.3>'
    '<UE_5.8>'
) -Follow
```

自动化测试应先编译对应 Editor Target，再用 `UnrealEditor-Cmd.exe` 运行受影响的 `XTools.<Module>` 前缀，并检查导出的 `index.json`，不能只看进程退出码。

- [命令行编译](Docs/打包相关/命令行编译指令.md)
- [多版本插件打包](Docs/打包相关/UE_多版本插件打包_问题与解决方案.md)
- [UE 全自动测试最佳实践](Docs/测试相关/UE全自动测试最佳实践.md)

## 开发约定

- 中文 Blueprint 元数据：`DisplayName`、`ToolTip`、参数显示名和分类保持中文。
- UE 5.4+ 按 IWYU 显式包含使用的头文件。
- 跨版本差异集中在 `Source/XToolsCore/Public/XToolsVersionCompat.h`。
- 用户可见错误使用 `XTOOLS_LOG_*` 或 `FXToolsErrorReporter`，不要新增散落的 `LogTemp`。
- K2Node 连接使用 Schema 校验，不直接调用 `MakeLinkTo()` 绕过规则。
- 异步回调捕获 UObject/Slate 对象时使用弱引用，并在执行时重新校验。

完整项目约定见 [`AGENTS.md`](AGENTS.md)，文档索引见 [`Docs/README.md`](Docs/README.md)。

## 文档与版本

- 当前开发变更：[`Docs/版本变更/UNRELEASED.md`](Docs/版本变更/UNRELEASED.md)
- 已发布历史：[`Docs/版本变更/CHANGELOG.md`](Docs/版本变更/CHANGELOG.md)
- 文档索引：[`Docs/README.md`](Docs/README.md)
- 维护脚本：[`Scripts/README.md`](Scripts/README.md)

版本号、模块清单和插件依赖以 `XTools.uplugin` 为准；文档不复制一次性构建统计或未经基准验证的性能数字。
