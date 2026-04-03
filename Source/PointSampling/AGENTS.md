# PointSampling - 几何采样算法

Runtime 模块。泊松圆盘 + 纹理密度 + 军事/几何阵型。带缓存系统。

## KEY CLASSES

| 类 | 职责 |
|----|------|
| `UPointSamplingLibrary` | 泊松/纹理/通用阵型采样 |
| `UFormationSamplingLibrary` | 军事战术 + 几何艺术阵型 |

## SAMPLING MODES (50+)

- **泊松**: 2D/3D，Bridson 算法 O(n)，严格最小距离保证
- **纹理**: 压缩/未压缩自动检测，像素级密度控制
- **军事**: Wedge/Column/Line/Vee/Echelon
- **几何**: Hexagonal/Star/Heart/Flower/GoldenSpiral/RoseCurve/CircularGrid/ConcentricRings

## COORDINATE SPACES

| 空间 | 含义 | 缓存Key包含 |
|------|------|-------------|
| `World` | 绝对世界坐标 | Extent+Radius+Position+Rotation |
| `Local` | 相对组件坐标 | Extent+Radius+Scale |
| `Raw` | 算法原始输出 | Extent+Radius+Scale |

## CACHING

- 缓存键: Extent + Radius + MaxAttempts + CoordinateSpace + (空间相关参数)
- `ClearPoissonSamplingCache()` 手动清除
- `GetPoissonSamplingCacheStats()` → Hits/Misses
- 移动/旋转组件会使 World 空间缓存失效

## GOTCHAS

- `JitterStrength > 0` 破坏严格泊松最小距离保证
- 纹理采样压缩格式走 Render-to-Texture (慢)，未压缩直接读取 (快)
- 六边形网格最优单位数 = 三角数序列
- 贝塞尔匀速模式计算量大，避免每帧调用，缓存结果
- 缓存键必须正确实现 `GetTypeHash()` + `operator==`
