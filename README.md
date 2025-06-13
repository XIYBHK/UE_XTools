# XTools - Unreal Engine 5.3 实用工具插件

[![UE Version](https://img.shields.io/badge/UE-5.3-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)
[![Version](https://img.shields.io/badge/Version-1.7.0-brightgreen.svg)](https://github.com/XIYBHK/UE_XTools)

XTools 是一个为 Unreal Engine 5.3 设计的综合性实用工具插件，提供了丰富的蓝图节点和 C++ 功能，旨在提升开发效率和游戏体验。

## 🚀 主要特性

### 📊 智能排序系统 (Sort Module)
**全面的数组排序解决方案**
- **多类型支持**: 整数、浮点数、字符串、向量、Actor 等
- **智能结构体排序**: 通过属性名称对任意结构体数组进行排序
- **坐标轴排序**: 按 X、Y、Z 轴对 Actor 和向量进行排序
- **原地排序**: 高效的内存使用，直接修改原数组
- **索引追踪**: 返回排序后元素在原数组中的索引
- **数组操作**: 反转、截取、去重等实用功能

### 🎲 高级随机系统 (RandomShuffles Module)
**基于 DOTA2 PRD 算法的智能随机**
- **PRD 算法**: 实现 DOTA2 标准的伪随机分布算法
- **智能随机**: 避免运气过好或过差，提供更公平的随机体验
- **状态管理**: 自动管理不同系统的随机状态
- **数组洗牌**: 支持任意类型数组的随机洗牌
- **随机流支持**: 支持可重现的随机序列
- **分布测试**: 内置 PRD 分布测试工具

### ⚡ 增强代码流 (EnhancedCodeFlow Module)
**强大的异步操作和流程控制**
- **异步操作**: 延迟执行、条件等待、循环执行
- **时间轴系统**: 浮点数、向量、颜色时间轴动画
- **流程控制**: DoOnce、DoNTimes、TimeLock 等控制节点
- **性能优化**: 内置性能分析和统计支持
- **句柄管理**: 安全的动作句柄系统
- **协程支持**: 完整的协程和异步系统

### 🎬 组件时间轴 (ComponentTimeline Module)
**灵活的时间轴系统**
- **组件时间轴**: 在任意组件中使用时间轴功能
- **蓝图集成**: 完整的蓝图编辑器支持
- **运行时初始化**: 动态创建和管理时间轴组件
- **编辑器工具**: 自定义 K2Node 节点支持
- **网络复制**: 支持多人游戏环境

### 🛠️ 资产编辑器工具 (X_AssetEditor Module)
**专业的资产管理和处理工具**
- **材质函数批量应用**: 快速为材质添加特效
- **资产命名规范化**: 自动化资产命名管理
- **菲涅尔等特效**: 内置常用材质函数
- **批量处理**: 支持大量资产的批量操作
- **前缀管理**: 智能的资产前缀系统

### 📐 几何工具 (Geometry Module)
**高级几何采样和分布算法**
- **内部点阵生成**: 在静态网格碰撞体内生成均匀分布的点
- **精确变换处理**: 支持旋转、缩放的采样范围
- **智能居中算法**: 确保点阵的均匀分布
- **性能优化**: 高效的包围盒剔除算法
- **随机扰动**: 可选的噪点扰动，使分布更自然
- **调试可视化**: 丰富的调试和可视化选项

### 🔧 异步工具 (AsyncTools Module)
**高级时间插值和动画系统**
- **时间插值**: 支持线性或曲线驱动的数值插值
- **事件委托**: 完整的开始/更新/完成/进度事件系统
- **灵活控制**: 暂停/恢复/取消/循环等控制接口
- **时间缩放**: 支持时间缩放和动态参数更新
- **调试系统**: 内置调试信息显示

### 📏 贝塞尔曲线工具 (Bezier Module)
**专业的曲线计算和可视化**
- **任意阶数**: 支持任意阶数的贝塞尔曲线
- **优化计算**: 二阶和三阶曲线的特殊优化
- **运动模式**: 匀速/参数化运动模式
- **可视化**: 完整的调试可视化系统
- **自定义配置**: 灵活的颜色和参数配置

### 🔍 组件查找工具 (ComponentFinder Module)
**高效的组件层级查找**
- **智能查找**: 快速查找组件层级中的父级 Actor
- **类型过滤**: 支持类型和标签过滤
- **自动匹配**: 自动查找最顶层匹配项
- **高效算法**: 单次遍历的高效算法
- **错误处理**: 完善的错误处理机制

## 📦 模块架构

XTools 插件采用模块化设计，包含以下核心模块：

```
XTools/
├── Source/
│   ├── XTools/                    # 主模块
│   ├── Sort/                      # 排序功能
│   ├── RandomShuffles/            # 随机功能
│   ├── EnhancedCodeFlow/          # 增强代码流
│   ├── ComponentTimelineRuntime/  # 组件时间轴运行时
│   ├── ComponentTimelineUncooked/ # 组件时间轴编辑器
│   └── X_AssetEditor/             # 资产编辑器工具
├── Content/                       # 插件内容
└── Config/                        # 配置文件
```

## 🛠️ 安装方法

### 方法一：直接下载
1. 下载插件文件
2. 将 `XTools` 文件夹复制到项目的 `Plugins` 目录
3. 重新生成项目文件
4. 编译项目
5. 在编辑器中启用插件 (编辑 > 插件 > XTools)

### 方法二：Git 克隆
```bash
cd YourProject/Plugins
git clone https://github.com/XIYBHK/UE_XTools.git
```

### 依赖插件
XTools 需要以下插件支持：
- **EditorScriptingUtilities** (资产编辑器功能)

## 🎯 蓝图节点分类

### XTools|排序
- **基础类型**: 整数数组排序、浮点数组排序、字符串数组排序
- **向量**: 向量数组排序、按坐标轴排序
- **Actor**: 按距离排序、按坐标轴排序、按名称排序
- **通用**: 通用属性排序（支持任意结构体）

### XTools|数组操作
- **反转**: 反转数组元素顺序
- **截取**: 获取数组的子集
- **去重**: 移除重复元素

### XTools|随机
- **洗牌**: 随机洗牌数组
- **PRD**: 伪随机分布算法
- **状态管理**: PRD 状态清理和管理

### XTools|Timeline
- **初始化**: 组件时间轴初始化
- **管理**: 时间轴生命周期管理

### XTools|异步
- **延迟**: 延迟执行
- **时间轴**: 各种时间轴动画
- **流程控制**: 条件执行、循环控制

## 🚀 快速开始

### 排序功能示例
```cpp
// C++ 示例 - 整数数组排序
TArray<int32> Numbers = {3, 1, 4, 1, 5};
TArray<int32> SortedNumbers;
TArray<int32> OriginalIndices;

USortLibrary::SortIntegerArray(Numbers, true, SortedNumbers, OriginalIndices);
// 结果: SortedNumbers = {1, 1, 3, 4, 5}, OriginalIndices = {1, 3, 0, 2, 4}
```

### PRD 随机示例
```cpp
// 使用 PRD 算法进行随机判定
bool bCriticalHit = URandomShuffleArrayLibrary::PseudoRandomBool(0.25f, "CriticalHit");
```

### 增强代码流示例
```cpp
// 延迟执行
FECFHandle DelayHandle = FFlow::Delay(this, 2.0f, [this]()
{
    UE_LOG(LogTemp, Log, TEXT("2秒后执行"));
});
```

### 组件时间轴示例
```cpp
// 在组件的 BeginPlay 中初始化时间轴
UComponentTimelineLibrary::InitializeComponentTimelines(this);
```

## ⚙️ 配置选项

### 组件时间轴设置
在项目设置 → XTools → 组件时间轴设置中：
- **在所有蓝图中启用时间轴**: 允许在任意蓝图中使用时间轴功能

### 性能设置
EnhancedCodeFlow 模块支持：
- **Insight 分析**: 启用 Unreal Insight 性能分析
- **统计信息**: 运行时性能统计
- **调试优化**: 非发布版本的调试优化控制

## 🔧 技术细节

### 系统要求
- **Unreal Engine**: 5.3+
- **平台**: Windows (主要), Mac/Linux (部分模块)
- **C++ 标准**: C++20
- **编译器**: MSVC 2022

### 性能特性
- **内存优化**: 原地排序算法，减少内存分配
- **类型安全**: 强类型检查，避免运行时错误
- **异步支持**: 非阻塞的异步操作
- **性能分析**: 内置性能统计和分析工具

## 📚 API 参考

### 主要类
- `USortLibrary`: 排序功能库
- `URandomShuffleArrayLibrary`: 随机功能库
- `FFlow`: 增强代码流主接口
- `UComponentTimelineLibrary`: 组件时间轴功能库

### 重要结构体
- `FECFHandle`: 代码流动作句柄
- `FECFActionSettings`: 动作设置
- `ECoordinateAxis`: 坐标轴枚举

## ⚠️ 注意事项

### 使用建议
- **异步工具**: 使用时注意保存引用，避免过早被垃圾回收
- **贝塞尔曲线**: 匀速模式计算量较大，不建议每帧大量调用
- **几何工具**: 采样点数量与网格大小成正比，注意性能影响
- **组件时间轴**: 需在 BeginPlay 中初始化
- **排序工具**: 自动处理空值和无效引用

### 性能考虑
- 批量处理大量资产时可能会暂时降低编辑器性能
- PRD 算法状态会持久保存，注意内存使用
- 异步操作需要合理管理生命周期

## 📈 版本历史

### v1.7.0 (当前版本)
- ✅ **重构 AsyncTools 模块**: 改进调试信息显示系统
- ✅ **重构 X_AssetEditor 模块**: 优化材质函数处理和资产命名系统
- ✅ **优化组件查找**: 改进 GetTopmostAttachedActor 函数性能
- ✅ **增强贝塞尔曲线**: 支持匀速模式和优化计算
- ✅ **新增 PRD 测试工具**: 内置分布测试功能
- ✅ **完善文档**: 全面更新文档和工具提示

### v1.6.0
- 🆕 新增几何工具模块
- 🔧 实现模型内部点阵生成功能

### v1.5.0
- 🆕 新增 X_AssetEditor 模块
- 🔧 支持批量材质函数应用
- 🔧 增加资产命名规范化工具

### v1.4.0
- 🆕 新增 RandomShuffles 模块
- 🔧 实现 PRD 算法和随机采样

### v1.3.0
- 🆕 新增 Sort 模块
- 🔧 完整的排序工具集

### v1.2.0
- 🆕 新增 ECF 模块
- 🔧 时间轴和异步系统

### v1.1.0
- 🆕 新增组件时间轴
- 🔧 时间轴编辑器支持

### v1.0.0
- 🎉 初始版本发布
- 🔧 基础工具集实现

## 🤝 贡献指南

欢迎提交 Issue 和 Pull Request！

### 开发环境设置
1. 克隆仓库到项目的 Plugins 目录
2. 使用 UE 5.3 打开项目
3. 生成项目文件并编译
4. 在编辑器中启用插件

### 代码规范
- 遵循 UE C++ 编码规范
- 使用中文注释
- 添加适当的蓝图元数据
- 包含单元测试

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 🙏 致谢

- **Enhanced Code Flow** 模块基于 Damian Nowakowski 的工作
- **Component Timeline** 模块基于 Tomasz Klin 的贡献
- **Random Shuffles** 模块基于 Anthony Arnold (RK4XYZ) 的实现
- **PRD 算法** 参考了 DOTA2 的标准实现

## 📞 联系方式

- **作者**: XIYBHK
- **GitHub**: [XTools Repository](https://github.com/XIYBHK/UE_XTools)
- **问题反馈**: 请通过 GitHub Issues 提交

---

**XTools** - 让 Unreal Engine 开发更高效！ 🚀
