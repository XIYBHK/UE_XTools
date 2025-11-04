# 2025-11-04

## ⏱️ 新增带延迟的循环节点

### K2Node_ForLoopWithDelay - 带延迟的范围循环
- **引脚**: FirstIndex, LastIndex, Delay → Index, Loop Body, Completed, Break
- **用途**: 逐个生成对象、序列动画、分帧处理
- **实现**: 使用UKismetSystemLibrary::Delay，通过ExpandNode生成循环逻辑

### K2Node_ForEachLoopWithDelay - 带延迟的数组遍历
- **引脚**: Array, Delay → Value, Index, Loop Body, Completed, Break
- **用途**: 逐个显示UI、对话系统、序列播放
- **关键词**: `foreach loop each delay 遍历 数组 循环 延迟 等待`

---

## 🔍 添加Loop节点搜索关键词

为所有循环节点添加`GetKeywords()`方法，支持中英文搜索：
- **K2Node_ForEachArray**: `foreach loop each 遍历 数组 循环 for array`
- **K2Node_ForEachArrayReverse**: `foreach loop each reverse 遍历 数组 循环 倒序 反向`
- **K2Node_ForEachSet**: `foreach loop each 遍历 循环 set 集合 for`
- **K2Node_ForEachMap**: `foreach loop each map 遍历 字典 循环 键值对 for`
- **K2Node_ForLoop**: `for loop 循环 for each 遍历 计数`

---

## 🌐 模块完整中文化

### K2Node节点（15个）
- **Map操作**: MapFindRef(查找引用)、MapIdentical(完全相同)、MapAppend(合并)
- **Map嵌套**: MapAdd/RemoveArrayItem、MapAdd/RemoveMapItem、MapAdd/RemoveSetItem
- **Loop循环**: ForLoop、ForEachArray、ForEachArrayReverse、ForEachSet、ForEachMap
- **变量**: Assign(引用赋值)

### BlueprintFunctionLibrary（11个库，60+函数）
- **MapExtensions**: 按索引访问、值查找、批量操作、随机获取（13函数）
- **MathExtensions**: 稳定帧、保留小数、排序插入、单位转换（10函数）
- **ObjectExtensions**: 从Map获取对象、清空/复制对象（5函数）
- **ProcessExtensions**: 按名称调用函数/事件（2函数）
- **SplineExtensions**: 路径有效性、获取起终点、简化样条（5函数）
- **TraceExtensions**: 线性/球形追踪（通道/对象）（10函数）
- **TransformExtensions**: 获取位置/旋转/各轴向（5函数）
- **VariableReflection**: 变量名列表、按字符串读写（3函数）

### 游戏功能（3个库）
- **SplineTrajectory**: 样条轨迹-平射/抛射/导弹
- **TurretRotation**: 计算炮塔旋转角度
- **SupportSystem**: 获取支点变换、稳定高度

### 清理Pironlot残留
- 内部常量: `PironlotBPFL_*` → `MapLibrary_*`
- Category: `Pironlot|*` → `XTools|Blueprint Extensions|*`

---

## 📋 最佳实践检查

### ✅ 关键修复
1. **模块架构重构**: 创建BlueprintExtensionsRuntime(Runtime)，K2Node保留在UncookedOnly
2. **K2Node错误处理**: 8个节点，50+处修复 - LOCTEXT本地化、BreakAllNodeLinks()、空指针检查
3. **节点分类统一**: `XTools|Blueprint Extensions|...` - Loops/Map/Variables层级清晰
4. **编译兼容性**: K2Node_MapFindRef使用标准ExpandNode，不依赖自定义枚举

### ✅ 已检查符合标准
- API导出宏正确、Build.cs依赖正确、节点图标完整(15/15)、模块类型正确

---

## 🏗️ BlueprintExtensions 模块架构

### 双模块设计
- **BlueprintExtensionsRuntime**(Runtime): 所有BPFL和游戏功能，支持Win64/Mac/Linux/Android/iOS
- **BlueprintExtensions**(UncookedOnly): 所有K2Node，仅编辑器使用，依赖Runtime模块

### 命名标准化
- 函数库: `XBPLib_*` → `U*ExtensionsLibrary`
- 游戏功能: `XBPFeature_*` → `U*Library`

### 核心功能
- **15个K2节点**: Loop系列、Map嵌套操作、引用赋值
- **Map扩展**: 按索引访问、值查找、批量操作、随机获取(20+函数)
- **变量反射**: 通过字符串动态读写对象变量
- **数学工具**: 稳定帧、精度控制、排序、单位转换
- **游戏功能**: 炮塔旋转、样条轨迹、支撑系统

---

# 2025-11-01

## 多版本兼容（UE 5.3-5.6）

### 版本策略
- **支持**: UE 5.3, 5.4, 5.5, 5.6
- **不支持**: UE 5.0（.NET Runtime 3.1依赖）、5.2（VS2022兼容性Bug）
- **实现**: 条件编译处理API差异，使用`ENGINE_MAJOR/MINOR_VERSION`宏

### API变更处理
- `TArray::Pop(bool)` → `Pop(EAllowShrinking)`
- `FString::LeftChopInline(int, bool)` → `LeftChopInline(int, EAllowShrinking)`
- 移除UE 5.5弃用的`bEnableUndefinedIdentifierWarnings`

## CI/CD优化
- 修复PowerShell编码问题、产物双重压缩(`.zip.zip`)
- 并发控制(`concurrency`)、60分钟超时保护
- Job Summary、构建时间统计、自托管runner清理
- 支持tag推送自动触发构建

## 构建系统优化
- 修复Shipping构建`FXToolsErrorReporter`编译错误(模板方法支持所有日志类型)
- 统一日志系统，移除`LogTemp`，使用模块日志类别

## 功能新增
- EnhancedCodeFlow时间轴新增`PlayRate`播放速率参数(默认1.0)