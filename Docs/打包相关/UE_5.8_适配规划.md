# UE 5.8 适配规划

> 状态：规划已复审，适配与双版本验收已完成
> 基线：XTools 1.9.6，验证日期 2026-07-17

## 目标与边界

- 维持一份源码同时支持 UE 5.3-5.8，不建立 5.8 专用分支。
- 5.8 差异集中到 `XToolsVersionCompat.h` 或窄范围条件编译，业务算法不复制。
- 自有模块只使用公开头文件和公开模块依赖。
- Electronic Nodes 的材质/动画图连线策略依赖引擎私有 DrawingPolicy，作为第三方核心机制的受控例外逐版本验证，不静默降级。
- 第三方更新已先行迁移；5.8 适配不得覆盖汉化、配置键和本地生命周期修补。

## 规划复审结论

原规划的“一份源码支持 5.3-5.8、优先公开 API、完整 BuildPlugin 验收”方向合理，但以下内容需要修正：

1. `SFilterList.h` 并非需要寻找新公共替代接口。BlueprintAssist 4.9.1 的 5.8 源码不再使用该类型，删除残留的私有头包含即可。
2. `IMainFrameModule::GetMainFrameCommandBindings()` 在本机 UE 5.3 与 5.8 都存在，SwitchLanguage 应直接使用它，不需要版本分支。
3. UE 5.8 仍提供 `PLATFORM_64BITS` 废弃别名，但该宏不能用于当前预处理表达式；UE 5 已不支持 32 位目标，Windows 分支直接判断 `PLATFORM_WINDOWS`。
4. Electronic Nodes 上游 5.8 仍通过 `MaterialEditor/Private` 实现材质连线策略。完全禁止私有依赖会丢功能，应改为受控例外并在每个引擎版本构建验证。
5. 适配不能只处理编译错误。迁移反审发现 BlueprintAssist 启用配置、防误触阈值和 ECF 协程生命周期发生回退，必须先补回再建立 5.8 基线。

参考资料：

- [UE 5.8 Release Notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes)
- [TArray::RemoveAt API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TArray/RemoveAt)
- [EAllowShrinking API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/EAllowShrinking)
- [IMainFrameModule API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/MainFrame/IMainFrameModule)

最终 API 结论以本机 UE 5.3/5.8 引擎源码和实际编译为准。

## 已完成的第三方同步

| 模块 | 来源版本 | 已迁移内容 |
|---|---:|---|
| AutoSizeComments | 3.4.9 | 预设按钮样式、标题前缀、节点边界模式、Subsystem 与 BlueprintAssist 检测 |
| Electronic Nodes | 3.21 | Gap/Arc/Circle 交叉样式与交叉线段拆分 |
| EnhancedCodeFlow | 3.9.0 | Action 标签/查询/时间控制、异步加载、协程等待和生命周期改进 |
| BlueprintAssist | 4.9.1 | 图任务/操作架构、Schema 适配、配置查看器、ControlRig 与搜索功能 |
| SwitchLanguage | 2024.10.25 | Schema 弱引用缓存及失效重建 |

迁移反审结果：

| 检查项 | 结果 | 处理 |
|---|---|---|
| 中文命令、设置元数据 | 保留 | 新增命令继续使用中文显示文本 |
| 重复 Marketplace 插件检测 | 保留 | 集成模块检测到外部插件时保持空载 |
| 晃动断开全部连线 | 功能保留，防误触阈值回退 | 恢复 30 像素最小位移和 `Dot < -0.5` |
| BlueprintAssist 启用配置 | `bEnablePlugin` 丢失 | 恢复原配置键，并兼容上游禁用键 |
| BugSplat 遥测禁用 | 仅禁用启动初始化，设置仍可触发上传 | 默认设为 Never，并在发送入口硬性阻止上传 |
| AutoSizeComments 旧禁用配置 | 字段保留但未生效 | 工厂同时尊重 `bEnablePlugin` 与旧 `bDisableASCGraphNode` |
| ECF 协程生命周期 | 上游新功能覆盖了本地 frame 所有权修补 | 最终挂起后由当前 Action 销毁，Owner 失效时不恢复协程 |
| ECF 后台完成标志 | 后台线程捕获 UObject 成员地址 | 改为线程安全共享标志，避免超时销毁后写悬空内存 |
| ECF 普通异步动作 | 二次反审发现同类 UObject 成员地址捕获 | 与协程版本统一使用线程安全共享标志 |

## 最终实测结果

执行：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 `
  -EngineRoots @(
    'D:\Program Files\Epic Games\UE_5.3',
    'D:\Program Files\Epic Games\UE_5.8'
  ) `
  -OutputBase 'D:\UEBuild58Verification\XToolsPackages-FinalAfterAudit'
```

结果：

- UE 5.3：UHT、516 个 Editor 动作、193 个 Runtime/Shipping 动作、链接与打包全部通过。
- UE 5.8：严格包含模式下 UHT、391 个 Editor 动作、108 个 Runtime/Shipping 动作、链接与打包全部通过。
- UE 5.8 插件源码弃用警告已清理，BlueprintAssist 的 Niagara、RigVM、可选 AssetSearch 依赖已显式声明。
- UE 5.3 的 `BuildPlugin -StrictIncludes` 会遗漏引擎 UHT 生成头目录；多版本脚本仅对 5.3 自动关闭该参数，5.4-5.8 保持严格包含检查。

完整日志位于：

- UE 5.3：`%APPDATA%\Unreal Engine\AutomationTool\Logs\D+Program+Files+Epic+Games+UE_5.3\UBT-UnrealEditor-Win64-Development.txt`
- UE 5.8：`%APPDATA%\Unreal Engine\AutomationTool\Logs\D+Program+Files+Epic+Games+UE_5.8\UBA-UnrealEditor-Win64-Development.txt`

## 已实施：基础 API 与 IWYU

1. **强类型数组收缩参数**
   - `BulletHomingLibrary.cpp` 的 `RemoveAt(..., false)`。
   - `AxisLockerComponent.cpp` 的 `Pop(false)`。
   - 通过 `XTOOLS_ENGINE_5_8_OR_LATER` 在 5.8 使用 `EAllowShrinking::No`，旧版本保留 `bool`。

2. **BlueprintExtensions 显式包含**
   - 为 `UEdGraphSchema_K2` 补前置声明或公开头。
   - `K2Node_MapFindRef.cpp` 显式包含 ToolMenus、UIAction、ScopedTransaction。
   - 两个自定义 `SGraphNode` 显式包含 `SImage`。

3. **PointSampling 纹理完整类型**
   - 为 `UTexture2D`、`FTexturePlatformData` 补公开头。
   - 复核 5.8 纹理平台数据访问是否仍允许编辑器侧同步读取。

4. **SortEditor 属性与结构体头**
   - 显式包含完整 `FTextProperty` 定义。
   - 核验 `UUserDefinedStruct` 在 5.8 的新公开头路径。

5. **平台宏清理**
   - `XToolsLibrary.cpp` 不再使用 5.8 废弃的 `PLATFORM_64BITS`。
   - 该逻辑仅为 Windows 内存统计，直接使用 `PLATFORM_WINDOWS`。

## 已实施：第三方编辑器模块

1. **AutoSizeComments**
   - `AutoSizeCommentsCommands.h` 显式包含 `FAppStyle`。
   - 调整 `MaterialLayersFunctions` 包含顺序，消除 `LOCTEXT_NAMESPACE` 污染。
   - 将 `OnPostEngineInit` 切换到 5.8 推荐访问器。

2. **BlueprintAssist**
   - 删除未使用的 `Editor/ContentBrowser/Private/SFilterList.h` 残留包含，与上游 5.8 源码一致。
   - 用 `UE_REMOVE_OPTIONAL_PARENS`、`IsSavingPackage()` 替换废弃接口。
   - 保持 5.3 的 AssetSearch 条件编译和 5.8 新搜索功能。

3. **Electronic Nodes**
   - 恢复上游 5.8 的 MaterialEditor 私有路径受控依赖，并按 5.6+ `FVector2f` 签名适配。
   - 补齐 DrawingPolicy 所需 Slate/GraphEditor 显式头。
   - 复核新交叉样式在缩放、曲线和重路由节点下的绘制结果。

4. **BlueprintScreenshotTool**
   - 补齐 `TCommands`、`FInputChord`、`SGraphEditor`、`UEdGraph`、RenderTarget 完整类型。
   - 检查 5.8 共享指针模式和截图渲染 API 签名。

5. **SwitchLanguage**
   - 移除不必要的引擎版本分支。
   - 5.3-5.8 统一调用公开的 `GetMainFrameCommandBindings()`。

## 已实施：X_AssetEditor

- 为 `FRegexPattern`、`Algo::FindByPredicate`、`AActor`、`UStaticMeshComponent` 补公开头。
- 为 `GEditor`、`UAssetEditorSubsystem`、`IAssetEditorInstance` 补编辑器公开头。
- 逐项迁移材质表达式废弃 API：`GetInputValueType`、`GetOutputValueType`、`GetInput`。
- 保留当前材质连接保底策略，不借适配重构行为。

## 实施记录

1. 先修迁移反审确认的行为回退，并以 UE 5.3 编译确认旧基线。
2. 修阶段一的机械签名/IWYU 问题，每轮运行 UE 5.8 BuildPlugin 获取下一组首错。
3. 再修第三方编辑器模块，优先公开 API；Electronic Nodes 私有 DrawingPolicy 例外单独记录。
4. 最后处理 X_AssetEditor 和废弃警告。
5. 每一阶段均回归 UE 5.3，再运行 UE 5.8 `BuildPlugin`。
6. 已扩展 CI 与本地脚本默认矩阵到 5.3-5.8；CI 保持原 F 盘路径优先，并支持本机 D 盘 Epic 安装路径回退。

## 验收标准

- [x] UE 5.3 与 UE 5.8 完整 `BuildPlugin` 均通过 UHT、编译、链接和打包。
- [ ] UE 5.4-5.7 由 CI 矩阵在提交后继续验证，本次未在本机重复全量构建。
- [x] 运行时模块未新增 Editor 依赖。
- [x] 自有模块未新增引擎 `Private` 头依赖；Electronic Nodes 保留既有受控例外并通过 5.3/5.8 构建。
- [x] 中文元数据、本地功能和配置键保持向后兼容。
- [x] 本次日志中的 5.8 插件源码弃用警告已清理。
