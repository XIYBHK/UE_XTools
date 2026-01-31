# XTools - Unreal Engine 5.3+ 实用工具插件

[![UE Version](https://img.shields.io/badge/UE-5.3--5.7-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-Internal-red.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)
[![Version](https://img.shields.io/badge/Version-1.9.5-brightgreen.svg)](https://github.com/XIYBHK/UE_XTools)

XTools 是一个为 Unreal Engine 5.3+ 设计的模块化工具插件，提供丰富的蓝图节点和 C++ 功能库，显著提升开发效率。

## 🚀 快速开始

### 安装

```bash
# 克隆到项目 Plugins 目录
cd YourProject/Plugins
git clone https://github.com/XIYBHK/UE_XTools.git

# 重新生成项目文件并编译
```

### 依赖
- **Unreal Engine**: 5.3 - 5.7
- **编辑器插件**: EditorScriptingUtilities（资产工具需要）

## 📦 核心模块

### 运行时模块

| 模块 | 功能 | 主要用途 |
|------|------|----------|
| **XToolsCore** | 跨版本兼容性层 | 提供 UE 5.3-5.7 版本兼容性和统一错误处理 |
| **Sort** | 智能排序系统 | 支持基础类型、向量、Actor、通用结构体的排序和数组操作 |
| **RandomShuffles** | PRD 随机系统 | 基于 DOTA2 算法的伪随机分布，提供更公平的随机体验 |
| **EnhancedCodeFlow** | 异步流程控制 | 延迟执行、时间轴动画、协程支持、性能分析 |
| **PointSampling** | 高级几何采样 | 泊松圆盘采样、基于面积的网格采样、等距样条线采样、纹理密度采样 |
| **ComponentTimelineRuntime** | 组件时间轴 | 在任意组件中使用时间轴功能，支持网络复制 |
| **BlueprintExtensionsRuntime** | 蓝图扩展库 | 14+ 自定义 K2Node，增强循环、Map操作、变量反射 |
| **ObjectPool** | 对象池系统 | Actor 对象池，支持预热、自动扩池、状态重置 |
| **FormationSystem** | 编队系统 | 编队管理和移动控制 |
| **FieldSystemExtensions** | 场系统扩展 | Chaos/GeometryCollection 高性能筛选 |

### 编辑器模块

| 模块 | 功能 | 主要用途 |
|------|------|----------|
| **XTools** | 主模块 | 插件入口、编辑器工具集成、欢迎界面 |
| **X_AssetEditor** | 资产编辑器 | 批量碰撞设置、材质函数应用、命名规范化、纹理打包支持 |
| **XTools_BlueprintAssist** | 蓝图助手 | 节点自动格式化、智能连线、快捷操作（已汉化） |
| **XTools_AutoSizeComments** | 自动注释框 | 注释框自动调整大小、样式管理（已汉化） |
| **XTools_ElectronicNodes** | 电子节点 | 蓝图连线美化、曲线样式（已汉化） |
| **XTools_BlueprintScreenshotTool** | 蓝图截图 | 高质量蓝图截图导出工具（已汉化） |
| **XTools_SwitchLanguage** | 语言切换 | 编辑器语言快速切换工具 |

## 🎯 主要特性

### 蓝图节点扩展
- **增强循环**: ForLoop/ForEach 系列节点，支持 Break 和反向遍历
- **通配符支持**: Assign、ForEachArray/Map/Set 等节点支持任意类型
- **Map 扩展**: 按索引访问、值查找、随机获取等 20+ Map 操作
- **变量反射**: 通过字符串动态访问对象变量

### 智能排序
- **多类型支持**: 整数、浮点、字符串、向量、Actor、自定义结构体
- **灵活排序**: 按距离、坐标轴、属性名称排序
- **数组工具**: 反转、截取、去重、插入等操作

### PRD 随机系统
- **公平随机**: 避免极端运气，提供更稳定的概率体验
- **状态管理**: 自动管理不同系统的独立随机状态
- **分布测试**: 内置概率分布验证工具

### 对象池优化
- **性能提升**: 避免频繁创建/销毁 Actor 的开销
- **智能管理**: 自动扩池、内存回收、使用率监控
- **无缝集成**: 自定义 K2Node 完全兼容原生 SpawnActor

### 高级几何采样
- **泊松圆盘采样**: 基于Bridson算法的O(N)高效实现，支持2D/3D空间
- **智能网格采样**: 基于三角形面积加权的均匀采样，支持边界顶点提取
- **等距样条线**: 基于弧长的精确等距采样，高质量Catmull-Rom插值
- **纹理密度采样**: 支持压缩和未压缩纹理的像素级密度控制
- **军事战术阵型**: 楔形、纵队、横队、V形、梯形等经典战术阵型
- **几何艺术阵型**: 蜂巢六边形、星形、螺旋、心形、花瓣等数学图形
- **高级阵型算法**: 黄金螺旋、圆形网格、玫瑰曲线、同心圆环等专业算法
- **通用阵型生成器**: 统一的阵型生成接口，支持所有阵型模式
- **采样质量验证**: 统计分析和泊松约束验证工具

### 资产工具
- **批量操作**: 碰撞设置、材质函数、重命名
- **命名规范**: 自动前缀、变体命名、数字后缀规范化
- **冲突检测**: 智能避免命名冲突

## 🛠️ 开发工具

### VS Code 任务
```bash
# 完整编译
Build Editor (Full & Formal)

# 热重载
Live Coding (Official)

# 清理构建
Clean Build
```

### 自动化脚本
```powershell
# 多版本打包（UE 5.3-5.7）
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots "UE_5.3","UE_5.4" -Follow

# 清理插件
.\Scripts\Clean-UEPlugin.ps1
```

## 📊 使用示例

### 从对象池生成 Actor
```cpp
// 蓝图中使用 "从池生成Actor" 节点
// 自动处理池化、状态重置、ExposeOnSpawn 属性
```

### PRD 随机
```cpp
// 获取 PRD 随机布尔值
bool bSuccess = URandomShuffleArrayLibrary::GetPRDBool("ChestSystem", 0.1f);
```

### 智能排序
```cpp
// 按距离排序 Actor 数组
USortLibrary::SortActorArrayByDistance(ActorArray, CenterLocation);
```

### 泊松圆盘采样
```cpp
// 在Box组件内生成泊松采样点
TArray<FVector> Points = UPointSamplingLibrary::GeneratePoissonPointsInBox(
    BoxComponent, 50.0f, 30, EPoissonCoordinateSpace::Local, 100
);

// 验证采样质量
bool bIsValid = UPointSamplingLibrary::ValidatePoissonSampling(Points, 50.0f);
```

### 基于面积的网格采样
```cpp
// 从静态网格生成基于面积的均匀采样点
TArray<FVector> Points = UFormationSamplingLibrary::GenerateFromStaticMesh(
    StaticMesh, Transform, 0, false, 1000
);
```

### 通用阵型生成器
```cpp
// 使用通用生成器创建各种阵型
TArray<FVector> WedgePoints = UPointSamplingLibrary::GenerateFormation(
    EPointSamplingMode::Wedge, 50, CenterLocation, Rotation,
    EPoissonCoordinateSpace::Local, 200.0f, 0.0f, 0, 60.0f // WedgeAngle
);

TArray<FVector> HexPoints = UPointSamplingLibrary::GenerateFormation(
    EPointSamplingMode::HexagonalGrid, 100, CenterLocation, Rotation,
    EPoissonCoordinateSpace::Local, 50.0f, 0.0f, 0, 3.0f // Rings
);
```

### 军事战术阵型
```cpp
// 生成战术阵型
TArray<FVector> WedgeFormation = UFormationSamplingLibrary::GenerateWedgeFormation(
    30, CenterLocation, Rotation, 150.0f, 45.0f // Spacing, WedgeAngle
);

TArray<FVector> ColumnFormation = UFormationSamplingLibrary::GenerateColumnFormation(
    20, CenterLocation, Rotation, 100.0f // Spacing
);
```

### 几何艺术阵型
```cpp
// 生成艺术几何图形
TArray<FVector> HeartShape = UFormationSamplingLibrary::GenerateHeartFormation(
    100, CenterLocation, Rotation, 200.0f // Size
);

TArray<FVector> FlowerShape = UFormationSamplingLibrary::GenerateFlowerFormation(
    150, CenterLocation, Rotation, 150.0f, 30.0f, 8 // OuterRadius, InnerRadius, PetalCount
);
```

### 高级阵型算法
```cpp
// 生成黄金螺旋（最自然的螺旋分布）
TArray<FVector> GoldenSpiral = UFormationSamplingLibrary::GenerateGoldenSpiralFormation(
    200, CenterLocation, Rotation, 300.0f // MaxRadius
);

// 生成玫瑰曲线（数学艺术曲线）
TArray<FVector> RoseCurve = UFormationSamplingLibrary::GenerateRoseCurveFormation(
    150, CenterLocation, Rotation, 200.0f, 5 // MaxRadius, Petals
);

// 生成圆形网格（极坐标规则网格）
TArray<FVector> CircularGrid = UFormationSamplingLibrary::GenerateCircularGridFormation(
    100, CenterLocation, Rotation, 250.0f, 4, 8 // MaxRadius, RadialDivisions, AngularDivisions
);

// 生成同心圆环（多层圆环分布）
TArray<int32> PointsPerRing = {8, 16, 24, 32};
TArray<FVector> ConcentricRings = UFormationSamplingLibrary::GenerateConcentricRingsFormation(
    200, CenterLocation, Rotation, 300.0f, 4, PointsPerRing // MaxRadius, RingCount, PointsPerRing
);
```

## 📈 版本历史

### v1.9.3 (2025-12-15)
- 详见更新日志：`Docs/版本变更/CHANGELOG.md`

### v1.9.2 (2025-11-17)
- 修复手动重命名保护机制和数字后缀规范化
- 修复晃动节点断开连接问题
- 优化 BlueprintScreenshotTool 性能（CPU占用降低60%）
- 修复 UE 5.4 版本 FCompression API 兼容性问题

### v1.9.1 (2025-11-13)
- 集成 BlueprintScreenshotTool 蓝图截图模块
- 资产命名系统增强（冲突检测、变体命名、数字后缀规范化）
- 统一错误处理到 FXToolsErrorReporter
- 修复 UE 5.6 编译错误和多个模块稳定性问题

### v1.9.0 (2025-11-06)
- 集成并汉化 AutoSizeComments、BlueprintAssist、ElectronicNodes
- 修复 K2Node 通配符类型丢失问题
- 完善跨版本兼容性（UE 5.3-5.6）

[查看完整更新日志](Docs/版本变更/CHANGELOG.md)

## ⚠️ 注意事项

- **内部使用**: 本插件仅供内部使用，请勿对外传播
- **性能考虑**: 贝塞尔曲线匀速模式计算量大，避免每帧调用
- **初始化要求**: ComponentTimeline 需在 BeginPlay 中初始化
- **对象池预热**: 初始化时有性能开销，但运行时效率显著提升

## 🤝 贡献

内部团队成员请通过 GitHub Issues 提交问题和建议。

## 📄 许可证

本插件采用内部许可证，仅供授权团队使用。集成的第三方组件保留其原始许可证。

详见 [LICENSE](LICENSE) 文件。

## 🙏 致谢

- **第三方插件作者**: fpwong (BlueprintAssist, AutoSizeComments), Herobrine20XX (ElectronicNodes), Gradess Games (BlueprintScreenshotTool)
- **算法参考**: DOTA2 PRD 算法实现

---

**作者**: XIYBHK
**仓库**: [github.com/XIYBHK/UE_XTools](https://github.com/XIYBHK/UE_XTools)
