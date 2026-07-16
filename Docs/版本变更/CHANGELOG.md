# XTools 更新日志 (CHANGELOG)

## 版本 v1.9.6 (2026-07-16)

<details>
<summary><strong>主要更新</strong></summary>

### 新增功能
- **AxisLocker**: 新增物理轴向锁定模块、蓝图预设和状态恢复栈
- **BlueprintExtensionsRuntime**: 新增追踪弹运动、样条跟随移动和 Mesh 反馈能力
- **PointSampling**: 新增静态网格体体素点位及材质颜色采样
- **X_AssetEditor**: 新增材质颜色烘焙和蓝图逻辑流导出工具
- **BlueprintExtensions**: 新增带延迟 WhileLoop 和可中断循环节点

### 重要修复
- **BlueprintExtensionsRuntime**: 完善追踪弹制导、末端捕获及组件状态恢复
- **多版本兼容**: 修复 UE 5.3-5.7 BuildPlugin 编译问题
- **多模块**: 修复蓝图节点边界、对象生命周期和跨版本编译问题
- **ObjectPool / EnhancedCodeFlow**: 完善对象池与异步流程生命周期管理

### 性能优化
- **PointSampling**: 优化体素化、纹理采样和统计规模保护
- **Sort / RandomShuffles**: 减少热路径分配并提升排序稳定性
- **FormationSystem**: 优化阵型计算容差、成本计算和性能埋点

</details>

<details>
<summary><strong>代码审查修复 (2026-06-11)</strong></summary>

- 修复 Sort.Runtime 删除 8 个无用编辑器依赖
- 修复 ObjectPool.Runtime 删除 3 个无用编辑器依赖
- 修复 FormationSystem.Runtime 删除 4 个无用编辑器依赖
- 修复 SortEditor 删除 EditorStyle 残留依赖
- 调整 FormationLibrary 15 个 BlueprintPure 改为 BlueprintCallable
- 新增 FormationMovementComponent AcceptanceRadius ForceUnits="cm"
- 新增 XToolsLibrary 传统 API 4 个参数添加 UIMin/UIMax/ForceUnits

</details>
<details>
<summary><strong>XToolsCore</strong></summary>

- 调整 防御宏改 inline constexpr，版本兼容宏归类整理

</details>
<details>
<summary><strong>XTools</strong></summary>

- 新增 PRD 测试最大循环次数限制
- 新增 补充贝塞尔采样配置编辑器元数据
- 调整 获取最高附加父Actor包含起始组件所属Actor，并增加循环检测、无效对象检查和最大深度保护
- 修复 获取所有附加子Actor递归遍历改为队列去重，避免重复路径或异常附加关系导致重复输出

</details>
<details>
<summary><strong>Scripts</strong></summary>

- 新增 蓝图资产复制文本导出脚本，支持从真实 Blueprint 资产导出 Graph 节点复制文本与 manifest

</details>
<details>
<summary><strong>AxisLocker</strong></summary>

- 新增 物理轴向锁定运行时模块
- 新增 蓝图预设与状态恢复栈
- 修复 组件目标选择改名称下拉

</details>
<details>
<summary><strong>BlueprintExtensions / BlueprintExtensionsRuntime</strong></summary>

- 修复 追踪弹调试宏在关闭调试绘制的构建中未定义
- 修复 延迟循环节点使用已移除的浮点比较函数导致 UE 5.5+ 编译失败
- 新增 带延迟的 WhileLoop 蓝图节点，支持事件图 Latent 循环与 Break 中断
- 新增 样条轨迹-导弹随机流节点，支持可复现的导弹预览样条随机偏转
- 新增 Mesh反馈节点，支持按网格体类型和组件Tag收集Actor网格体，并可从Actor或网格体数组批量创建/缓存动态材质实例
- 新增 追踪弹运动节点支持纯追踪、预测拦截和比例导引
- 新增 追踪弹运动节点轨迹调试绘制
- 优化 追踪弹比例导引算法
- 优化 追踪弹PN末端收束
- 优化 追踪弹末端捕获，减少近目标绕圈
- 优化 追踪弹轨迹调试保留时间
- 修复 追踪弹大仰角发射不追踪
- 修复 追踪弹初始速度被组件速度覆盖
- 优化 追踪弹写入组件速度同步
- 优化 追踪弹最大速度同步组件
- 优化 追踪弹速度与转向插值率渐进增长，并支持按初始距离比例进入完全制导
- 优化 追踪弹默认速度、制导、末端捕获与调试参数，默认配置可直接用于常规第三人称追踪弹
- 修复 追踪弹在获得有效目标前提前耗尽发射段与渐进制导阶段
- 修复 追踪弹终端状态仅写入零速度但组件模拟仍会恢复运动
- 优化 追踪弹PN渐进制导的零速率语义，并在组件启用速度旋转时同帧同步Actor旋转
- 修复 追踪弹覆盖外部组件暂停状态、终端结果未锁存及换目标后零速重启
- 优化 追踪弹方向响应在纯追踪与比例导引模式下的参数说明和旋转输出一致性
- 修复 追踪弹继承组件速度时终端后换目标永久零速，并同步垂直旋转模式的输出结果
- 修复 追踪弹仅凭RotationFollowsVelocity误判组件已接管Actor旋转，并明确重置节点的运动恢复语义与追踪会话计时
- 修复 追踪弹渐进插值首帧提前增长、目标失效回退不稳定与组件旋转延后一帧
- 修复 追踪弹PN末端切向阻尼帧率依赖，并修正零初始转向响应配合正增长率时的首帧突转
- 修复 追踪弹零速度插值率配合正增长率时直接跳速、零最大转向响应下制导模式不一致及零DeltaTime仍执行末端收束
- 修复 追踪弹末端收束速度大小未写回、零DeltaTime仍触发完全制导及外观根组件覆盖Actor旋转
- 修复 追踪弹移动目标帧间捕获、越过及历史最近距离判定误差
- 修复 追踪弹正后方目标方向插值死锁及已停止ProjectileMovement仍报告成功
- 优化 追踪弹终端恢复组件归属，并明确目标瞬移与对象池复用的状态重置约束
- 修复 追踪弹发射段制导倍率在反向目标时跳变及非有限弹体位置仍继续追踪
- 修复 追踪弹非法位置提前消费目标切换状态及无效外观旋转偏移传播
- 修复 追踪弹首次无目标时提前固化运动状态，并使用UpdatedComponent朝向初始化无Actor静止弹体
- 修复 追踪弹新目标位置暂时非法时提前消费目标切换，并按相对速度执行移动目标PN末端收束
- 修复 追踪弹非PN路径忽略组件实际速度方向，并同步外观组件实际旋转输出
- 修复 追踪弹无法驱动物理或静态UpdatedComponent时仍报告成功，并校验子弹Actor与移动组件归属一致
- 修复 追踪弹外部暂停时仍推进制导状态，并在终态锁存期间冻结追踪会话时间
- 修复 追踪弹关闭组件写入后未归还终态暂停，并补齐空UpdatedComponent时的移动组件归属校验
- 修复 Map 函数边界检查和循环逻辑
- 优化 Map_Identical 算法性能（嵌套循环优化）
- 修复 变量反射 API 逻辑错误与空对象检查（IsValid）
- 修复 ProcessExtensions payload 调用参数帧构造，支持非结构体单参数并拒绝 out/ref 参数
- 修复 对象属性复制改用反射 CopyCompleteValue，避免文本导入导出造成类型丢失
- 调整 K2Node 编译报错由 Error 改 Warning
- 修复 K2Node 重建引脚校验优雅降级（ensureMsgf）
- 修复 带延迟循环节点限制为事件图使用，避免函数/宏图展开 Latent 节点导致编译崩溃
- 修复 带 Break 的循环节点 Completed 执行路径，避免 Break 后完成分支连接不完整
- 修复 ForEachMap/ForEachSet 遍历改为快照数组，避免循环体修改容器后索引漂移
- 调整 统一连接接口（TryConnect）与日志类别归位
- 优化 样条轨迹、支撑系统、炮塔旋转节点输入校验、中文分类和 Tooltip 说明
- 优化 样条轨迹曲率/随机因子钳制，避免 NaN 或极端参数污染样条
- 优化 支撑系统 Trace/PID 输出默认值和非法输入恢复逻辑
- 优化 炮塔旋转角度钳制，支持跨 -180/180 和接近 360 度全范围旋转
- 新增 样条线跟随移动函数库，支持 AddMovementInput 与 AI MoveTo 目标点、停止/往返/循环终点行为、调试绘制、初始右偏移捕获，并提供偏移路径速度补偿用于队形对齐
- 修复 样条跟随角色速度补偿受 MaxWalkSpeed/MaxAcceleration 限制导致外圈掉队
- 修复 样条跟随速度补偿恢复生命周期
- 修复 开放样条循环停在端点
- 优化 样条跟随节点关键参数直露，计算节点兼容 Actor
- 优化 样条跟随节点参数说明
- 优化 样条跟随参数 Tooltip 换行
- 优化 样条跟随 Tick 路径静默无效输入
- 调整 样条跟随闭合样条按终点行为处理
- 移除 样条参考Actor队形实验逻辑
- 新增 样条偏移按半宽限制到缩放范围

</details>
<details>
<summary><strong>ComponentTimeline</strong></summary>

- 修复 时间轴组件生命周期（Construction Script 重建问题）
- 调整 K2Node 报错改 Warning，日志归位到模块类别

</details>
<details>
<summary><strong>EnhancedCodeFlow</strong></summary>

- 修复 协程生命周期管理（异步任务崩溃）
- 修复 时间处理逻辑（累加方式跟踪超时）
- 修复 循环模式双重 Tick 问题
- 调整 WhileTrueExecute 增加是否在初始化时立即评估条件的参数，供蓝图延迟循环安全展开使用
- 新增 Owner 有效性检查
- 新增 协程异常处理和错误标记

</details>
<details>
<summary><strong>ObjectPool</strong></summary>

- 修复 Actor 状态重置缺少 TimerManager 完整类型
- 增强 统计信息管理（新增 TotalReturned 字段）
- 修复 生命周期管理（OnReturnToPool 事件调用）
- 优化 ContainsActor 性能（O(n)→O(1)，TSet 索引）
- 新增 ClearTimersAndEvents 状态重置
- 移除 多余头文件包含（Engine.h）
- 修复 类指针缓存改弱引用避免 GC 悬空
- 移除 删除重复的预分配策略字段
- 优化 统计归入专属 StatGroup
- 优化 SpawnActorFromPool 在子系统返回空 Actor 时回退普通生成
- 调整 Shipping 保留 Warning/Error 日志

</details>
<details>
<summary><strong>Sort / SortEditor / RandomShuffles</strong></summary>

- 新增 SortEditor RebuildDynamicPins 递归调用保护
- 优化 HeapSort 消除 TFunction 堆分配
- 优化 PRD 常量表改静态数组消除堆分配
- 修复 字符串 KeyFuncs 标准化跨版本编译
- 修复 通用数组随机采样输出状态不一致
- 修复 严格权重随机采样拒绝负数/NaN权重，避免输出数量越界
- 修复 属性排序遇到不支持属性时提前保持原序，避免比较器返回 false 后仍交换元素
- 优化 属性排序空对象比较顺序，避免对象数组存在空元素时排序不稳定
- 优化 使用 thread_local 避免 PRD 状态共享污染
- 调整 K2Node 报错由 Error 改 Warning
- 调整 日志归位到 LogSortEditor，移除冗余 Super 调用
- 新增 补充排序参数编辑器范围

</details>
<details>
<summary><strong>FormationSystem</strong></summary>

- 优化 阵型尺寸比较容差计算（相对容差替代硬编码）
- 新增 Z 维度尺寸检查
- 修复 浮点精度问题（Acos/DotProduct Clamp 钳位）
- 移除 冗余 ensure() 调用
- 修复 螺旋检测相关系数公式（补距离平方和）
- 新增 实现阵型接口回调通知（IFormationInterface）
- 优化 热路径添加 Stats 性能埋点
- 修复 编队成本计算防御空数组越界

</details>
<details>
<summary><strong>FieldSystemExtensions</strong></summary>

- 修复 空指针二次调用竞态条件（lambda 存储 GetOwner）
- 调整 关闭无 Tick 逻辑 Actor 的默认 Tick

</details>
<details>
<summary><strong>PointSampling / GeometryTool</strong></summary>

- 修复 RenderTarget 创建与编辑器材质链 API 的非编辑器编译兼容
- 新增 从静态网格体生成体素点位节点，支持表面体素化、内部填充和颜色/材质索引输出
- 优化 从静态网格体生成体素点位节点的体素化性能、内存/工作量保护和极端输入诊断
- 优化 从静态网格体生成体素点位节点的内部填充上限处理，避免因包围盒最大可能体素数过早返回空结果，改为输出达到 MaxVoxelCount 时截断
- 优化 体素点位节点支持CPU可读资产顶点色插值、UV采样材质贴图颜色和编辑器BaseColor链简单颜色常量回退
- 优化 体素点位节点输出资产顶点色数量、CPU访问和启用状态诊断
- 修复 纹理采样缓冲区越界风险（RGBA16/RGBA16F 边界检查）
- 修复 材质渲染世界上下文错误（采样失效）
- 优化 采样统计函数添加规模上限保护
- 新增 不支持的形状类型警告
- 优化 禁用组件不必要的 Tick（降低 CPU 开销）
- 优化 圆形采点缓存 Owner 减少重复调用

</details>
<details>
<summary><strong>X_AssetEditor</strong></summary>

- 新增 静态网格体右键材质烘焙工具，默认复制资产后将材质 BaseColor 按 UV 写入副本资产顶点色，便于体素点位节点优先采样真实颜色数据
- 新增 蓝图右键逻辑流导出工具，输出 JSON 与 Markdown 图表/节点/引脚/连接信息
- 优化 GenerateUniqueAssetName 性能（O(N²) → O(N)）
- 修复 Lambda 捕获悬空指针风险（弱引用模式）
- 优化 自动规范命名导入检测（添加文件时间戳备用通道）
- 优化 简化用户操作上下文检测逻辑
- 修复 命名冲突后缀统一为两位补零
- 优化 消除材质函数检查的临时 UObject
- 优化 集合参数改 const 引用避免拷贝
- 新增 批量修改 Pivot 支持中途取消

</details>
<details>
<summary><strong>BlueprintAssist / ElectronicNodes</strong></summary>

- 修复 数组访问边界检查（GetLeftSibling 索引越界）
- 移除 禁用第三方崩溃遥测上报（隐私）
- 修复 废弃样式集名改用 FAppStyle
- 移除 清理重复模块依赖
- 新增 引擎私有路径版本守卫注释

</details>
<details>
<summary><strong>BlueprintScreenshotTool</strong></summary>

- 修复 RenderTarget 空指针检查

</details>
<details>
<summary><strong>多模块</strong></summary>

- 调整 编辑器依赖收敛为 Private 依赖
- 新增 补充 UPROPERTY 编辑器元数据（范围/单位/条件显示）

</details>

---
## 版本 v1.9.5 (2026-01-31)

<details>
<summary><strong>主要更新</strong></summary>

### BlueprintExtensions 模块
- **修复** 修复带延迟循环节点初次延迟问题（首次迭代立即执行）
- **修复** 修复 ForEach 循环节点 Break 执行流中断问题（Break 后继续到 Completed）
- **修复** 修复 ForEachArrayReverse 引脚重复创建问题（移除 Super 调用）
- **完善** 完善 ForEachArrayReverse 类型验证逻辑
- **修复** 修复 Map 随机访问空 Map 边界问题（RandomItem/RandomItemFromStream）

### X_AssetEditor 模块
- **优化** 彻底修复 Ticker Lambda 悬空指针风险（使用 TSharedPtr/TWeakPtr）
- **新增** 实现变体命名支持功能（FAssetNamingPattern 结构体）
- **新增** 实现重命名后的重名冲突自动处理
- **重构** 提取公共辅助函数，消除代码重复
- **优化** 配置化硬编码常量（FactoryCreationTimeWindow、StartupActivationDelay）
- **优化** 静态缓存正则表达式模式，避免重复编译
- **优化** 批量重命名性能：预缓存文件夹资产名称（O(n²)→O(n)）
- **移除** 删除 90 行未实现的命名冲突检测代码
- **调整** FactoryCreationTimeWindow 默认值改为 15 秒

### PointSampling 模块
- **修复** 修复白底纹理采样异常问题
- **新增** 新增自动亮度反转功能（智能识别背景色）
- **新增** 新增反转采样模式（LuminanceInverted、AlphaInverted）
- **调整** 采样核心函数增加 bInvert 参数
- **优化** 增加采样过程中的详细日志输出
- **修复** 修复压缩纹理采样时尺寸缩放和纵横比错误的问题
- **优化** 增强纹理采样自动对齐逻辑，确保点位整齐且不重叠
- **修复** 修复压缩纹理在渲染模式下无法自动反转采样的问题
- **优化** 修复UHT解析错误（ToolTip多行字符串合并为单行）

</details>

---

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
- **BlueprintExtensions**: GetCasePinCount() 从 O(n^2) 优化为 O(n)
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
- **优化** `GetCasePinCount()` 从 O(n^2) 优化为 O(n) 复杂度
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
- **新增** 基于泊松圆盘采样的纹理点阵生成功能
- **新增** 多种点阵生成算法（圆形/矩形/三角形/样条线/网格）
- **新增** 3D矩形点阵生成支持

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
- **新增** 特殊编辑模式检测，破碎/建模/地形等模式下自动禁用重命名
- **优化** 使用OnEditorModeIDChanged回调跟踪模式切换，替代轮询检测
- **优化** 正则表达式性能，使用静态常量避免重复创建
- **优化** 移除OnAssetRenamed的冗余回调调用，提升性能

</details>

<details>
<summary><strong>MaterialTools 模块</strong></summary>

- **修复** 添加材质函数后撤销崩溃（移除Transaction避免撤销系统冲突）
- **修复** EmissiveColor输出引脚连接失败（添加Emissive别名）
- **修复** 材质函数节点位置计算错误，忽略简单常量节点
- **新增** 引入材质常量，减少硬编码
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
- **新增** 时间轴循环功能（bLoop），对齐UE原生FTimeline实现
- **优化** Owner销毁后静默跳过回调，避免崩溃（10个Action）
- **优化** 移除Custom时间轴多余的bSuppressCallback机制

</details>

<details>
<summary><strong>其他模块更新</strong></summary>

### XToolsCore
- **新增** 新增6个防御性编程宏（指针/UObject/数组检查）
- **优化** 提升硬件不稳定环境下的代码鲁棒性

### BlueprintExtensions
- **新增** 带延迟的倒序ForLoop节点（K2Node_ForLoopWithDelayReverse）
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
- **新增** GetVariableNames增加bIncludeSuper参数，支持获取父类变量
- **修复** 文件内容损坏导致的编译错误

### XToolsLibrary
- **新增** 递归获取所有子Actor（BFS）

### PivotTool
- **新增** 静态网格体枢轴点管理功能

### CI/CD工作流
- **修复** update-release-assets工作流重复删除资产导致404错误
- **优化** 并发控制策略，取消旧任务只保留最新任务
- **优化** 下载artifacts时去重，避免处理重复文件
- **优化** 改进日志输出格式，添加序号和文件大小信息

</details>

---

## 版本 v1.9.2 (2025-11-17)

<details>
<summary><strong>主要更新</strong></summary>

### 新增功能
- **X_AssetEditor**: 命名冲突检测系统、变体命名支持、数字后缀规范化、纹理打包后缀支持
- **BlueprintAssist**: 插件启用开关、高级搜索功能、节点展开限制、调试设置
- **BlueprintScreenshotTool**: 完整集成蓝图截图工具，支持多显示器环境

### 重要修复
- **BlueprintAssist**: 修复晃动节点断开连接后节点不跟随鼠标问题
- **X_AssetEditor**: 修复手动重命名保护机制，解决自动规范化覆盖问题
- **AutoSizeComments**: 修复取消标题样式时无条件应用默认字体大小的问题
- **兼容性**: 修复 UE 5.4 版本 FCompression API 兼容性问题

### 性能优化
- **BlueprintScreenshotTool**: CPU占用降低约60%，使用BFS避免栈溢出
- **MaterialTools**: 材质函数智能连接优化，支持自动回溯接入
- **第三方插件**: 检测外部插件避免重复加载，提升启动性能

</details>

<details>
<summary><strong>X_AssetEditor 模块</strong></summary>

- **修复** 手动重命名保护机制，基于调用堆栈检测彻底解决自动规范化覆盖问题
- **修复** 数字后缀规范化逻辑，移至重命名流程最终步骤，确保_1正确转换为_01格式
- **增强** 批量重命名功能，即使前缀正确的资产也会检查数字后缀规范化需求
- **新增** 包含蒙太奇通知在内的部分资产前缀映射规则
- **新增** 命名冲突检测系统，自动避免重命名失败
- **新增** 变体命名支持，兼容Allar Style Guide规范
- **新增** 数字后缀规范化，自动转换为两位数格式
- **新增** 纹理打包后缀支持，包含_ERO、_ARM等组合
- **优化** 资产规范化失败后抛出资产详细信息
- **优化** 用户操作上下文检测，支持UE 5.3-5.7版本
- **优化** 重命名逻辑，集成智能冲突解决方案
- **优化** MaterialTools 材质函数智能连接，失败时回溯 MaterialAttributes 链路并自动接入 BaseColor/自发光 节点

</details>

<details>
<summary><strong>BlueprintAssist 模块</strong></summary>

- **修复** 晃动节点断开连接后节点不跟随鼠标，保持逻辑链连接
- **修复** BlueprintAssistTypes.h与BlueprintAssistUtils.h循环依赖
- **修复** API宏不一致问题，统一使用XTOOLS_BLUEPRINTASSIST_API
- **修复** 宏重定义警告，移除BlueprintAssistSettings.h中的重复宏定义
- **修复** TryCreateConnection调用，使用TryCreateConnectionUnsafe
- **修复** UE 5.4版本 FCompression::GetMaximumCompressedSize API兼容性问题
- **新增** 插件启用开关
- **新增** bSkipAutoFormattingAfterBreakingPins设置，断开引脚时跳过自动格式化
- **新增** ExpandNodesMaxDist设置，限制节点展开的最大水平距离
- **新增** BlueprintAssistDebug调试设置支持
- **调整** 晃动断开连接灵敏度（MinShakeDistance 5→30，DotProduct <0→<-0.5）
- **优化** 启动流程，检测到外部BlueprintAssist插件时集成版保持空载
- **本地化** 所有用户可见文本（100+设置项 + 30+菜单项）

</details>

<details>
<summary><strong>BlueprintScreenshotTool 模块</strong></summary>

- **集成** 蓝图截图工具模块
- **新增** 插件启用开关
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
<summary><strong>其他模块更新</strong></summary>

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
<summary><strong>第三方插件集成优化</strong></summary>

- **优化** XTools_BlueprintAssist 启动流程，检测到外部 BlueprintAssist 插件启用时集成版保持空载
- **优化** XTools_AutoSizeComments、XTools_ElectronicNodes、XTools_BlueprintScreenshotTool、XTools_SwitchLanguage 在检测到外部插件启用时保持空载
- **优化** XTools_EnhancedCodeFlow 子系统创建逻辑，检测到外部 EnhancedCodeFlow 插件启用时不创建集成版子系统

</details>

---

## 版本 v1.9.1 (2025-11-13)

<details>
<summary><strong>主要更新</strong></summary>

### 新增功能
- 集成 BlueprintScreenshotTool 蓝图截图模块，支持快捷键截图与结果通知
- 资产命名系统新增命名冲突检测、变体命名、数字后缀规范化、纹理打包后缀与蒙太奇通知前缀等能力

### 重要修复
- 修复 UE 5.6 GetPasteLocation API 变化导致的 CI 编译错误
- 修复 BlueprintAssist 模块中 FVector2D/FVector2f 类型转换问题
- 修复资产自动重命名功能导致的编辑器崩溃
- 修复模块重命名后各模块 API 导出宏（XTools_ 前缀）不一致问题
- 修复 EnhancedCodeFlow 模块移动构造函数实现错误
- 修复 BlueprintAssist 晃动节点断开连接后节点不跟随鼠标的问题
- 修复 BlueprintScreenshotTool 在插件禁用、首次截图、内存管理、DPI、多显示器和失败提示等场景下的异常
- 修正部分运行时与编辑器工具在参数校验失败时的错误信息不一致问题

### 性能优化
- 完善 UE 5.3-5.7 版本兼容性处理，统一条件编译策略
- 优化资产重命名流程与用户操作上下文检测，提升稳定性和启动性能
- 优化 FieldSystemExtensions 默认行为、Sort 模块冗余结构与 BlueprintScreenshotTool 工具栏显示
- 统一 XTools 核心工具及部分编辑器模块的错误/关键告警日志到 FXToolsErrorReporter
- 优化 MaterialTools 材质函数智能连接，失败时回溯 MaterialAttributes 链路并自动接入节点

</details>

---

## 版本 v1.9.0 (2025-11-06)

<details>
<summary><strong>主要更新</strong></summary>

### 新增功能
- 集成 AutoSizeComments、BlueprintAssist、ElectronicNodes 三个编辑器增强插件
- 为集成插件提供完整中文化配置和默认设置优化，提升开箱即用体验

### 重要修复
- 修复 K2Node 通配符引脚类型丢失问题（ForEachArray/Map/Set 等节点），确保编辑器重启后类型保持正确
- 修复 BlueprintAssist、ElectronicNodes 等第三方插件在 UE 5.0+ 与 5.6 下的编译错误
- 修复 FieldSystemExtensions、BlueprintExtensionsRuntime 等模块在新版本引擎下的警告与编译问题

### 性能优化
- 完善 XTools 版本宏系统与跨 UE 5.3-5.6 的兼容性处理
- 优化材质工具、采样等相关模块的 API 使用与实现细节，提升可维护性与性能

</details>

---

## 版本 v1.8.x (2025-11-05)

<details>
<summary><strong>兼容性与采样工具</strong></summary>

- 修复 XTools 采样功能在 UE 5.4-5.6 下的头文件依赖与 API 兼容问题
- 修复 FieldSystemExtensions 在 UE 5.6 下 BufferCommand 弃用导致的兼容性问题
- 新增基于 GeometryCore 的原生表面采样模式，显著提升采样性能
- 修复采样目标误判、Noise 应用错误、除零和整型溢出等崩溃风险
- 改进错误信息、参数校验和调试日志，便于定位采样问题

</details>

<details>
<summary><strong>BlueprintExtensions 与 MaterialTools</strong></summary>

- 优化 MaterialTools 核心 API 使用和智能连接系统，实现更高性能与更好可维护性
- 优化 PointSampling 模块算法和内存使用，提升大规模点云处理性能
- 完善 XTools 采样工具的可视化调试与运行时兼容性

</details>

---

## 版本 v1.8.x (2025-11-04)

<details>
<summary><strong>FieldSystemExtensions 模块</strong></summary>

- 新增 FieldSystemExtensions 模块与 AXFieldSystemActor，提供高性能 Chaos / GeometryCollection 筛选能力
- 支持按对象类型、Actor 类 / Tag、运行时过滤等多种筛选方案
- 提供 UXFieldSystemLibrary 辅助函数，简化筛选器创建与复用

</details>

<details>
<summary><strong>蓝图循环与本地化</strong></summary>

- 新增带延迟的 ForLoop / ForEach 蓝图节点，支持逐帧 / 渐进式逻辑
- 为循环节点补充中英文搜索关键词，提升节点检索体验
- 重构 BlueprintExtensions 模块架构（Runtime + UncookedOnly），统一节点分类与本地化

</details>

---

## 版本 v1.8.x (2025-11-01)

<details>
<summary><strong>多版本支持与构建系统</strong></summary>

- 明确支持 UE 5.3-5.6 的版本策略，完善关键 API 的条件编译处理
- 修复 Shipping 构建错误，统一日志类别并改进错误上报
- 优化 CI/CD 工作流：修复编码和压缩问题，增加并发控制和构建统计
- 为 EnhancedCodeFlow 时间轴新增 PlayRate 播放速率参数（默认 1.0）

</details>
