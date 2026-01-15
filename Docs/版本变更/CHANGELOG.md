# XTools 更新日志 (CHANGELOG)

## 版本 v1.9.4 (2026-01-15)

<details>
<summary><strong>主要更新</strong></summary>

### 新增功能
- **ObjectPool**: 控制台命令 `objectpool.stats` 显示对象池统计信息
- **ObjectPool**: `GetAllPoolStats()` 和 `GetPoolCount()` 公开接口
- **GeometryTool**: 基于形状组件的点阵生成（球体/立方体表面）
- **GeometryTool**: 随机旋转、缩放、噪声参数和朝向原点控制
- **GeometryTool**: 自定义矩形区域和圆形多层次点阵生成
- **PointSampling**: 军事战术阵列型和几何艺术阵列型生成
- **FieldSystemExtensions**: TMap/TSet 版本缓存支持智能去重
- **SortEditor**: 排序模式引脚连接状态检测和变化监听

### 重要修复
- **PointSampling**: 泊松缓存键哈希/相等性契约、随机种子失效、坐标归一化等多处问题
- **SortEditor**: K2Node_SmartSort 提升枚举引脚消失、排序模式无效、标题显示错误
- **BlueprintExtensions**: SGraphNodeCasePairedPinsNode 本地化支持
- **BlueprintExtensionsRuntime**: 除零/空指针/WorldContext 崩溃修复
- **X_AssetEditor**: 自动化流程误触发资产重命名

### 性能优化
- **BlueprintExtensions**: GetCasePinCount() 从 O(n²) 优化为 O(n)
- **FieldSystemExtensions**: Map+Set 组合提升查找性能
- **PointSampling**: 矩形和圆形阵列型采样算法优化

</details>

<details>
<summary><strong>ObjectPool 模块</strong></summary>

- **新增** `objectpool.stats` 控制台命令，在屏幕左上角显示对象池统计信息
- **新增** `GetAllPoolStats()` 和 `GetPoolCount()` 公开接口
- **优化** 代码结构与清理冗余注释

</details>

<details>
<summary><strong>BlueprintExtensions 模块</strong></summary>

- **修复** `SGraphNodeCasePairedPinsNode` 使用 NSLOCTEXT 宏支持本地化
- **优化** `GetCasePinCount()` 从 O(n²) 优化为 O(n) 复杂度
- **重构** `K2Node_ForEachMap` 复用 `FK2NodePinTypeHelpers` 辅助类，减少重复代码

</details>

<details>
<summary><strong>GeometryTool 模块</strong></summary>

- **新增** 基于形状组件的点阵生成功能
- **新增** 支持球体和立方体表面点阵生成
- **新增** 支持随机旋转、缩放和噪声参数
- **新增** 支持朝向原点的变换控制
- **新增** 自定义矩形区域点阵生成
- **新增** 圆形多层次点阵生成
- **本地化** 完整的蓝图节点中文参数名

</details>

<details>
<summary><strong>PointSampling 模块</strong></summary>

- **修复** 泊松缓存键哈希/相等性契约不一致（改用强比较）
- **修复** 泊松缓存键缺少 Scale 和 MaxAttempts 字段
- **修复** ApplyTransform 中除零风险（SafeScale 检查）
- **修复** 随机种子参数失效（新增 GeneratePoisson2D/3DFromStream）
- **修复** 理想采样坐标归一化错误（[0..W] 误当 [-W/2..W/2]）
- **修复** FloatRGBA 格式 FP16/FP32 平台差异（动态判断字节数）
- **修复** 理想采样数据越界风险（添加大小验证和范围 Clamp）
- **优化** 泊松采样日志级别从 Log 降为 Verbose（减少刷屏）
- **优化** EPoissonCoordinateSpace 枚举文档（详细说明 World/Local/Raw 差异）
- **优化** 矩形和圆形阵列型采样算法
- **优化** 采样辅助模块核心逻辑为内部函数，统一随机源处理
- **优化** 重构为 Runtime 和 Editor 模块，优化构建配置
- **调整** RenderCore 从 Public 依赖移至 Private（符合 IWYU 原则）
- **调整** 新增 RHI 模块依赖（用于 EPixelFormat 定义）
- **移除** MeshSamplingHelper 中未使用的 SocketRotation 和 SocketTransform 变量
- **新增** 基于泊松圆盘采样的纹理点阵生成功能
- **新增** 多种点阵生成算法（圆形/矩形/三角形/样条线/网格）
- **新增** 3D 矩形点阵生成支持
- **新增** 军事战术阵列型和几何艺术阵列型生成功能

</details>

<details>
<summary><strong>SortEditor 模块</strong></summary>

- **修复** K2Node_SmartSort 提升枚举引脚为变量时第二个引脚消失
- **修复** 提升为变量后排序模式无效（始终使用默认值）
- **修复** Vector 数组连接后标题错误显示为"结构体属性排序"
- **重构** 使用统一入口函数替代 Switch 分支（SortVectorsUnified/SortActorsUnified）
- **优化** 提升为变量时使用 AdvancedView 叠加不常用引脚
- **优化** 动态引脚 Tooltip 更新（描述各引脚适用场景）
- **新增** 排序模式引脚连接状态检测（连接时显示所有可能引脚）
- **新增** 排序模式引脚连接变化监听（PinConnectionListChanged）

</details>

<details>
<summary><strong>其他模块更新</strong></summary>

### RandomShuffles
- **修复** 性能统计线程安全漏洞和代码规范问题

### X_AssetEditor
- **重构** 资产重命名触发逻辑
- **修复** 自动化流程误触发资产重命名（增强上下文检测）
- **优化** 资产重命名触发逻辑，采用 Factory 时间窗 + 资产类型双重匹配机制
- **重构** 移除冗余的手动重命名检测逻辑，简化委托绑定

### FieldSystemExtensions
- **修复** GC 缓存去重、IsValid 替代、日志降噪、UTF-8 编译参数
- **新增** TMap 版本的 FFieldSystemCacheMap 支持智能缓存键去重
- **新增** TSet 版本的 FFieldSystemCacheSet 用于快速查询
- **重构** 统一使用 Map+Set 组合替代单一 Map，提升查找性能
- **重构** 替代所有 IsValid 检查，统一使用 SafePointer 模板
- **优化** 日志级别优化（非关键日志降级为 Verbose）
- **新增** UTF-8 编译参数支持（兼容跨平台源文件）

### BlueprintExtensionsRuntime
- **修复** 除零/空指针/WorldContext 崩溃，Transform 按值返回

### XTools_EnhancedCodeFlow
- **修复** 时间轴回调和精度问题，增加循环功能

### XTools
- **修复** 移除 try/catch、缓存键 Hash/Equals 简约、IWYU 安全、日志降噪、UTF-8 编译参数

### CI/CD
- **优化** 移除 Artifact 上传前的手动压缩，直接使用 upload-artifact@v4 自动压缩
- **优化** Release zip 仅在 tag push 时创建（非 tag 构建不再双重压缩）
- **清理** 从仓库移除误提交的 ci 日志目录
- **新增** build-plugin-optimized.yml 支持 workflow_dispatch 事件触发发布包准备
- **新增** update-release-assets.yml 优化输出变量处理逻辑

</details>

---

## 版本 v1.9.3 (2025-12-15)

<details>
<summary><strong>主要更新</strong></summary>

### 新增功能
- **PointSampling**: 纹理点阵生成、多种算法支持、3D矩形点阵
- **XToolsCore**: 6个防御性编程宏（指针/UObject/数组检查）
- **BlueprintExtensions**: 带延迟的倒序ForLoop节点
- **XTools_EnhancedCodeFlow**: 时间轴循环功能（bLoop）
- **X_AssetEditor**: 特殊编辑模式检测（破碎/建模/地形等模式下禁用重命名）
- **PivotTool**: 静态网格体枢轴点管理功能
- **XToolsLibrary**: 递归获取所有子Actor（BFS）
- **VariableReflectionLibrary**: GetVariableNames增加bIncludeSuper参数

### 重要修复
- **X_AssetEditor**: 启动时误触发自动重命名、Lambda生命周期竞态等多处崩溃问题
- **MaterialTools**: 添加材质函数后撤销崩溃、EmissiveColor连接失败
- **XTools_AutoSizeComments**: GetNodePos空指针访问导致材质编辑器崩溃
- **PointSampling**: 3D泊松采样球面分布不均匀、Grid无效标记冲突等问题

### 性能优化
- **X_AssetEditor**: 使用OnEditorModeIDChanged回调跟踪模式切换，替代轮询检测
- **MaterialTools**: 并行材质收集、智能连接评分系统
- **MapExtensionsLibrary**: RemoveEntriesWithValue复杂度O(N^2)降至O(N)

</details>

<details>
<summary><strong>PointSampling 模块</strong></summary>

- **修复** 3D泊松采样球面分布不均匀问题（极点附近过密）
- **修复** Grid使用ZeroVector作为无效标记的潜在冲突问题
- **修复** Depth较小时3D采样仅生成几个点的问题（自动降级为2D）
- **修复** FromStream版本TargetPointCount与Radius同时指定时行为不一致
- **修复** 2D采样时ApplyJitter错误扰动Z坐标的问题
- **修复** 非编辑器构建时的多处编译错误
- **修复** K2Node条件编译和UHT解析问题
- **重构** 2D/3D采样核心逻辑为内部函数，统一随机源处理
- **重构** 拆分为Runtime和Editor模块，优化构建配置
- **增加** 基于泊松圆盘采样的纹理点阵生成功能
- **增加** 多种点阵生成算法（圆形/矩形/三角形/样条线/网格）
- **增加** 3D矩形点阵生成支持

</details>

<details>
<summary><strong>X_AssetEditor 模块</strong></summary>

- **修复** 启动时资产检查阶段误触发自动重命名
- **修复** 模块关闭时的Lambda生命周期竞态条件
- **修复** 初始化时跳过延迟保护机制的问题
- **修复** 导入资产缺少重入保护导致的潜在递归
- **修复** Lambda在模块Shutdown后仍执行导致崩溃（3处）
- **修复** OnAssetRenamed缺少GEditor空指针检查
- **修复** 移除UToolMenus显式清理调用，符合UE标准实践
- **增加** 特殊编辑模式检测，破碎/建模/地形等模式下自动禁用重命名
- **优化** 使用OnEditorModeIDChanged回调跟踪模式切换，替代轮询检测
- **优化** 正则表达式性能，使用静态常量避免重复创建
- **优化** 移除OnAssetRenamed的冗余回调调用，提升性能

</details>

<details>
<summary><strong>MaterialTools 模块</strong></summary>

- **修复** 添加材质函数后撤销崩溃（移除Transaction避免撤销系统冲突）
- **修复** EmissiveColor输出引脚连接失败（添加Emissive别名）
- **修复** 材质函数节点位置计算错误，忽略简单常量节点
- **增加** 引入材质常量，减少硬编码
- **优化** 移除冗余的ExecuteWithTransaction和PrepareForModification
- **优化** 支持并行材质收集，提升批量处理性能
- **优化** 改进智能连接逻辑，引入评分系统解决误判
- **优化** 消除代码重复，统一核心处理逻辑
- **优化** 使用材质主节点实际位置计算新节点坐标
- **本地化** 全面本地化日志输出为中文

</details>

<details>
<summary><strong>XTools_EnhancedCodeFlow 模块</strong></summary>

- **修复** 所有异步Action增加Owner有效性检查，防止悬空指针
- **修复** 时间轴首次tick时初始值触发问题，对齐UE原生行为
- **修复** BP时间轴OnFinished回调bStopped参数始终为false
- **修复** ECFTimeline完成条件使用值比较导致的浮点精度问题
- **修复** 时间轴结束时最终值精度问题，确保精确到达终点值
- **修复** Loop模式下触发精确终点值并处理溢出时间
- **增加** 时间轴循环功能（bLoop），对齐UE原生FTimeline实现
- **优化** Owner销毁后静默跳过回调，避免崩溃（10个Action）
- **优化** 移除Custom时间轴多余的bSuppressCallback机制

</details>

<details>
<summary><strong>其他模块更新</strong></summary>

### XToolsCore
- **增加** 新增6个防御性编程宏（指针/UObject/数组检查）
- **优化** 提升硬件不稳定环境下的代码鲁棒性

### BlueprintExtensions
- **增加** 带延迟的倒序ForLoop节点（K2Node_ForLoopWithDelayReverse）
- **优化** 所有Delay循环节点增加图兼容性检查（仅EventGraph可用）
- **优化** ForLoop/ForEach延迟节点增加编译时引脚有效性检查
- **修复** K2Node蓝图编译时增加空指针防护，避免硬件异常崩溃

### XTools_AutoSizeComments
- **修复** GetNodePos函数空指针访问导致材质编辑器崩溃

### MapExtensionsLibrary
- **优化** RemoveEntriesWithValue移除O(N^2)复杂度，提升至O(N)
- **修复** GenericMap_RemoveEntries函数定义不完整导致的编译错误
- **修复** 恢复丢失的RemoveEntriesWithValue、SetValueAt、RandomItem函数

### TraceExtensionsLibrary
- **优化** 增加TraceChannel和ObjectType的静态缓存，优化字符串查找性能
- **优化** 移除调试日志输出，减少运行时开销

### VariableReflectionLibrary
- **增加** GetVariableNames增加bIncludeSuper参数，支持获取父类变量
- **修复** 文件内容损坏导致的编译错误

### XToolsLibrary
- **增加** 递归获取所有子Actor（BFS）

### PivotTool
- **增加** 静态网格体枢轴点管理功能

### CI/CD工作流
- **修复** update-release-assets工作流重复删除资产导致404错误
- **优化** 并发控制策略，取消旧任务只保留最新任务
- **优化** 下载artifacts时去重，避免处理重复文件
- **优化** 改进日志输出格式，添加序号和文件大小信息

</details>

---

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