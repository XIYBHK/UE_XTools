# FormationSystem - 编队系统

Runtime 模块。编队生成 + 转换 + 移动控制。依赖 AIModule。

## KEY CLASSES

| 类 | 职责 |
|----|------|
| `UFormationLibrary` | 蓝图静态API，编队生成 |
| `UFormationManagerComponent` | 编队生命周期管理，单位分配 |
| `UFormationMovementComponent` | 单位移动控制 |
| `FFormationData` | 位置数组 + 元数据 |

## FORMATION TYPES

Square, Circle, Line, Triangle, Arrow, Spiral, SolidCircle, Zigzag, Custom

## TRANSITION MODES (7种)

| 模式 | 算法 | 适用场景 |
|------|------|----------|
| OptimizedAssignment | 相对位置匹配 | 最佳视觉效果 |
| SimpleAssignment | 绝对距离匹配 | 简单场景 |
| DirectMapping | 索引映射 | 固定编队 |
| DirectRelativePositionMatching | 左对左右对右 | 对称编队 |
| RTSFlockMovement | Boids 算法 | RTS 游戏 |
| PathAwareAssignment | 碰撞避让 | 复杂地形 |
| SpatialOrderMapping | 空间填充曲线 | 大规模编队 |

## GOTCHAS

- 编队位置是相对偏移，需 `GetWorldPositions()` 转世界坐标
- 转换不同单位数的编队时，多余单位被忽略或新建位置
- OptimizedAssignment 是 O(n^2) 复杂度，大编队慎用
