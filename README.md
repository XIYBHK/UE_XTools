# XTools 插件

XTools是一个为Unreal Engine开发的自用工具插件，提供了一些便捷的功能和工具。

## 功能特性及使用说明

### 1. 异步工具 (AsyncTools)

异步工具提供了一个灵活的异步操作框架，用于处理需要随时间变化的操作。

#### 蓝图使用方法：

1. 创建节点：右键 → 搜索"Async Action"
2. 设置基本参数：
   - Time：操作总时长
   - First Delay：开始前的延迟时间
   - Delta Seconds：更新频率（默认0.033秒）

3. 曲线相关参数（可选）：
   - Use Curve：是否使用曲线
   - Curve Float：曲线资源
   - Curve Value：初始值
   - A/B：自定义参数

4. 事件回调：
   - On Start：开始时
   - On Update：每次更新时
   - On Complete：完成时
   - On Progress：进度更新时（0-1）

5. 控制函数：
   - Pause：暂停
   - Resume：继续
   - Cancel：取消
   - Set Loop：设置是否循环
   - Set Time Scale：设置时间缩放

### 2. 贝塞尔曲线工具

用于计算和可视化贝塞尔曲线。

#### 蓝图使用方法：

1. 创建节点：右键 → 搜索"Calculate Bezier Point"

2. 基本设置：
   - Points：添加控制点（至少2个点）
   - Progress：计算位置（0-1）

3. 调试选项：
   - Show Debug：显示调试图形
   - Duration：显示持续时间
   - Debug Colors：自定义调试颜色
    

4. 运动控制：
   - Speed Mode：
     - Default：默认模式
     - Constant：匀速模式
   - Speed Curve：速度调整曲线

### 3. 组件查找工具

用于在组件层级中查找特定的父级Actor。

#### 蓝图使用方法：

1. 创建节点：右键 → 搜索"Find Parent Component By Class"

2. 设置参数：
   - Component：起始组件
   - Actor Class：目标Actor类型
   - Actor Tag：标签（可选）

3. 查找规则：
   - 同时指定类和标签：返回首个匹配的
   - 仅指定类：返回最高层级匹配的
   - 仅指定标签：返回首个匹配的
   - 都不指定：返回最顶层父级

### 4. 组件时间轴工具

提供了在组件和对象中使用时间轴的功能，支持属性动画和事件触发。

#### 蓝图使用方法：

1. 组件时间轴：
   - 创建节点：右键 → XTools → "添加组件时间轴"
   - 双击节点打开时间轴编辑器
   - 在BeginPlay中调用"初始化组件时间轴"函数

2. 对象时间轴（实验性功能）：
   - 在项目设置中启用："XTools → 组件时间轴设置 → 在所有蓝图中启用时间轴"
   - 创建节点：右键 → XTools → "添加对象时间轴"
   - 在BeginPlay中调用"初始化对象时间轴"函数

3. 时间轴功能：
   - 支持浮点数、向量、颜色等属性的动画
   - 支持事件轨道触发自定义事件
   - 支持循环播放和自动播放
   - 支持网络复制

4. 注意事项：
   - 组件时间轴：仅支持组件蓝图
   - 对象时间轴：支持非组件、非Actor的对象蓝图
   - 必须在BeginPlay中进行初始化
   - 对象时间轴需要指定有效的Actor作为宿主

### 5. 增强代码流工具 (EnhancedCodeFlow)

提供了一套强大的代码流程控制工具，用于处理复杂的异步操作、协程和时间轴控制。

#### 蓝图使用方法：

1. 基础功能：
   - ECF系统暂停/继续：控制整个ECF系统的暂停状态
   - ECF动作状态：检查特定动作是否正在运行
   - ECF动作控制：暂停、继续或停止特定动作

2. 时间轴功能：
   - 标准时间轴：在指定时间内对数值进行插值
     - ECF时间轴：处理单一数值的渐变
     - ECF向量时间轴：处理三维向量的渐变
     - ECF颜色时间轴：处理线性颜色的渐变
   
   - 自定义时间轴：使用自定义曲线进行插值
     - ECF自定义时间轴：自定义浮点值的变化曲线
     - ECF自定义向量时间轴：自定义向量值的变化曲线
     - ECF自定义颜色时间轴：自定义颜色值的变化曲线

3. 异步和协程功能：
   - ECF异步延迟：在指定时间后异步执行操作
   - ECF异步延迟帧：在指定帧数后异步执行操作
   - ECF异步执行：异步执行操作并在完成后触发回调
   - ECF协程等待：创建协程并等待条件满足
   - ECF协程执行：在协程中执行一系列操作

4. 循环控制：
   - ECF异步循环：持续异步执行指定操作直到手动停止
   - ECF协程循环：在协程中循环执行操作
   - ECF条件等待：在条件满足时执行操作

5. 事件回调：
   - OnTick：时间轴更新时触发
   - OnFinished：操作完成时触发
   - OnAsyncComplete：异步操作完成时触发
   - OnCoroutineComplete：协程完成时触发
   - 支持传递当前值、时间和状态信息

6. 高级特性：
   - 混合函数：支持线性、指数等多种插值方式
   - 时间控制：精确的时间和帧控制
   - 句柄系统：通过句柄精确控制每个动作
   - 协程管理：支持协程的暂停、继续和终止
   - 异步队列：管理多个异步操作的执行顺序

#### 注意事项：
- 所有时间轴功能都支持暂停、继续和停止操作
- 自定义时间轴需要提供有效的曲线资源
- 异步操作要注意正确处理回调和清理
- 协程操作需要正确管理生命周期
- 循环执行时要设置适当的退出条件
- 异步和协程操作要注意避免死锁

## 注意事项

1. 异步操作使用时需要注意内存管理，及时清理，避免内存泄漏
2. 贝塞尔曲线计算时，控制点数量需要至少为2个
3. 调试显示功能仅在开发版本中可用
4. 组件查找功能设有最大深度限制（默认100层）
5. 组件时间轴初始化必须在BeginPlay中调用
6. 对象时间轴使用时需要注意Actor宿主的生命周期

## 许可证

本插件采用MIT许可证。详见LICENSE文件。

## 版本历史

### v1.0.0
- 初始版本发布
- 实现基础异步工具功能
- 实现贝塞尔曲线工具
- 实现组件查找功能

### v1.1.0
- 新增组件时间轴功能
- 支持在组件和对象中使用时间轴
- 添加时间轴编辑器支持
- 实现时间轴网络复制功能

### v1.2.0
- 新增EnhancedCodeFlow功能模块
- 实现全套时间轴控制功能
- 添加异步操作和延迟执行支持
- 优化循环控制和事件回调机制
