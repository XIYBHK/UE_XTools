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

## 注意事项

1. 异步操作使用时需要注意内存管理，及时清理，避免内存泄漏
2. 贝塞尔曲线计算时，控制点数量需要至少为2个
3. 调试显示功能仅在开发版本中可用
4. 组件查找功能设有最大深度限制（默认100层）

## 许可证

本插件采用MIT许可证。详见LICENSE文件。

## 版本历史

### v1.0.0
- 初始版本发布
- 实现基础异步工具功能
- 实现贝塞尔曲线工具
- 实现组件查找功能
