# 待发布更新 (UNRELEASED)

> 此文档记录尚未发布的功能更新、修复和移除，用于下次版本发布时的描述参考。
> 发布新版本时，将此文件内容合并到 CHANGELOG.md，然后清空此文件（保留说明部分）。

---

## ObjectPool

- **新增** `objectpool.stats` 控制台命令，在屏幕左上角显示对象池统计信息
- **新增** `GetAllPoolStats()` 和 `GetPoolCount()` 公开接口

---

## BlueprintExtensions

- **修复** `SGraphNodeCasePairedPinsNode` 使用 NSLOCTEXT 宏支持本地化
- **优化** `GetCasePinCount()` 从 O(n²) 优化为 O(n) 复杂度
- **重构** `K2Node_ForEachMap` 复用 `FK2NodePinTypeHelpers` 辅助类，减少重复代码

---

## GeometryTool

- **新增** 基于形状组件的点阵生成功能
- **新增** 支持球体和立方体表面点阵生成
- **新增** 支持随机旋转、缩放和噪声参数
- **新增** 支持朝向原点的变换控制
- **新增** 自定义矩形区域点阵生成
- **新增** 圆形多层次点阵生成
- **本地化** 完整的蓝图节点中文参数名

---

## RandomShuffles

- **修复** 性能统计线程安全漏洞和代码规范问题

---

## ObjectPool

- **优化** 代码结构与清理冗余注释

---

## X_AssetEditor

- **重构** 资产重命名触发逻辑
- **修复** 自动化流程误触发资产重命名（增强上下文检测，检查自动化测试/Cooker状态）
- **优化** 资产重命名触发逻辑，采用 Factory 时间窗（10s）+ 资产类型双重匹配机制，完美兼容用户交互（如选取父类）并彻底排除误触发
- **重构** 移除冗余的手动重命名检测逻辑，简化委托绑定，提升代码可维护性

---

## PointSampling

- **修复** 泊松缓存键哈希/相等性契约不一致（改用强比较）
- **修复** 泊松缓存键缺少 Scale 和 MaxAttempts 字段
- **修复** ApplyTransform 中除零风险（SafeScale 检查）
- **修复** 随机种子参数失效（新增 GeneratePoisson2D/3DFromStream）
- **修复** 理想采样坐标归一化错误（[0..W] 误当 [-W/2..W/2]）
- **修复** FloatRGBA 格式 FP16/FP32 平台差异（动态判断字节数）
- **修复** 理想采样数据越界风险（添加大小验证和范围 Clamp）
- **优化** 泊松采样日志级别从 Log 降为 Verbose（减少刷屏）
- **优化** EPoissonCoordinateSpace 枚举文档（详细说明 World/Local/Raw 差异）
- **调整** RenderCore 从 Public 依赖移至 Private（符合 IWYU 原则）
- **调整** 新增 RHI 模块依赖（用于 EPixelFormat 定义）
- **移除** MeshSamplingHelper 中未使用的 SocketRotation 和 SocketTransform 变量
- **优化** 采样辅助模块核心逻辑为内部函数，统一随机源处理
- **优化** 重构为 Runtime 和 Editor 模块，优化构建配置
- **新增** 基于泊松圆盘采样的纹理点阵生成功能
- **新增** 多种点阵生成算法（圆形/矩形/三角形/样条线/网格）
- **新增** 3D 矩形点阵生成支持
- **优化** 矩形和圆形阵列型采样算法
- **新增** 军事战术阵列型和几何艺术阵列型生成功能

---

## SortEditor

- **修复** K2Node_SmartSort 提升枚举引脚为变量时第二个引脚消失
- **修复** 提升为变量后排序模式无效（始终使用默认值）
- **修复** Vector 数组连接后标题错误显示为"结构体属性排序"
- **重构** 使用统一入口函数替代 Switch 分支（SortVectorsUnified/SortActorsUnified）
- **优化** 提升为变量时使用 AdvancedView 叠加不常用引脚
- **新增** 排序模式引脚连接状态检测（连接时显示所有可能引脚）
- **新增** 排序模式引脚连接变化监听（PinConnectionListChanged）
- **优化** 动态引脚 Tooltip 更新（描述各引脚适用场景）

---

## FieldSystemExtensions

- **修复** GC 缓存去重、IsValid 替代、日志降噪、UTF-8 编译参数
- **新增** TMap 版本的 FFieldSystemCacheMap 支持智能缓存键去重
- **新增** TSet 版本的 FFieldSystemCacheSet 用于快速查询
- **重构** 统一使用 Map+Set 组合替代单一 Map，提升查找性能并解决缓存键哈希冲突
- **重构** 替代所有 IsValid 检查，统一使用 SafePointer 模板
- **优化** 日志级别优化（非关键日志降级为 Verbose）
- **新增** UTF-8 编译参数支持（兼容跨平台源文件）

---

## BlueprintExtensionsRuntime

- **修复** 除零/空指针/WorldContext 崩溃，Transform 按值返回

---

## XTools_EnhancedCodeFlow

- **修复** 时间轴回调和精度问题，增加循环功能

---

## XTools

- **修复** 移除 try/catch、缓存键 Hash/Equals 简约、IWYU 安全、日志降噪、UTF-8 编译参数

---

## CI/CD

- **优化** 移除 Artifact 上传前的手动压缩，直接使用 upload-artifact@v4 自动压缩
- **优化** Release zip 仅在 tag push 时创建（非 tag 构建不再双重压缩）
- **清理** 从仓库移除误提交的 ci 日志目录
- **新增** build-plugin-optimized.yml 支持 workflow_dispatch 事件触发发布包准备
- **新增** build-plugin-optimized.yml 移除仅限 tag push 的限制，提高灵活性
- **新增** update-release-assets.yml 优化输出变量处理逻辑
- **新增** update-release-assets.yml 修复 fallback 机制的输出时机问题
- **新增** update-release-assets.yml 改进本地文件检测的日志提示

---

## 质量保证

- **检查** 全面审查 20 个 K2Node 节点，确认无类似引脚消失问题

---

## 📋 日志格式说明

### 模块分类
按模块名称分为独立章节，每个章节内使用简洁的列表格式记录变更

### 记录格式
\`\`\`markdown
## 模块名

- 类型 要描述
- 类型 要描述
\`\`\`

### 类型说明（单字或双字）:
- **新增** / 新增功能
- **优化** / 改进现有功能
- **修复** / 修复问题
- **移除** / 删除功能
- **调整** / 参数或行为调整
- **集成** / 新增模块集成
- **本地化** / 本地化工作

### 内容要求
1. **简洁**：每行记录一个独立变更，描述控制在 20 字内
2. **清晰**：使用"动词 + 对象 + （效果）"格式
3. **关键信息**：包含重要的参数变化、性能数据、修复的问题
4. **不要**：避免详细技术细节、文件路径、长篇问题分析

### 发布流程
1. 开发过程中持续在此文件顶部添加新记录
2. 版本发布前整理和分类内容
3. 复制内容到 CHANGELOG.md，更新版本号和日期
4. 清空此文件（保留说明部分），准备下一轮开发

---
