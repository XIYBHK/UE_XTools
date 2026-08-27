/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PointSamplingTypes.h"
#include "FormationSamplingLibrary.generated.h"

class UStaticMesh;
class UTexture2D;

/**
 * 阵型采样函数库
 * 提供各种图案阵型的点位生成功能
 *
 * 职责范围：
 * - 矩形类阵型（实心、空心、螺旋）
 * - 三角形类阵型（实心、空心）
 * - 圆形阵型
 * - 雪花类阵型（完整雪花、雪花弧形）
 * - 样条线采样
 * - 网格采样（静态网格体、骨骼插槽）
 * - 纹理像素采样
 */
UCLASS()
class POINTSAMPLING_API UFormationSamplingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// 矩形类阵型采样
	// ============================================================================

	/**
	 * 生成中心对齐的二维矩形网格点阵
	 * @param RowCount 行数
	 * @param ColumnCount 列数
	 * @param HorizontalSpacing 横向（X轴）点间距
	 * @param VerticalSpacing 纵向（Y轴）点间距
	 * @param CenterLocation 中心位置
	 * @param Rotation 旋转（世界空间时生效）
	 * @param CoordinateSpace 坐标空间
	 * @return 点位数组，按行优先顺序排列
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|矩形",
		meta = (DisplayName = "生成矩形网格点阵",
			ToolTip = "生成中心对齐的二维矩形网格点阵，可分别设置横向和纵向点间距。",
			Keywords = "矩形 网格 点阵 横向间距 纵向间距 rectangle grid spacing"))
	static TArray<FVector> GenerateRectangleGrid(
		UPARAM(DisplayName = "行数", meta = (ClampMin = "1", UIMin = "1")) int32 RowCount = 5,
		UPARAM(DisplayName = "列数", meta = (ClampMin = "1", UIMin = "1")) int32 ColumnCount = 5,
		UPARAM(DisplayName = "横向间距", meta = (ClampMin = "0.001", UIMin = "1.0")) float HorizontalSpacing = 100.0f,
		UPARAM(DisplayName = "纵向间距", meta = (ClampMin = "0.001", UIMin = "1.0")) float VerticalSpacing = 100.0f,
		UPARAM(DisplayName = "中心位置") FVector CenterLocation = FVector::ZeroVector,
		UPARAM(DisplayName = "旋转") FRotator Rotation = FRotator::ZeroRotator,
		UPARAM(DisplayName = "坐标空间") EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local
	);

	/**
	 * 生成实心矩形点阵
	 * @param PointCount 总点数
	 * @param CenterLocation 中心位置
	 * @param Rotation 旋转
	 * @param Spacing 点间距
	 * @param RowCount 行数（0=自动计算）
	 * @param ColumnCount 列数（0=自动计算）
	 * @param Height 高度，支持3D矩形点阵（1=2D平面）
	 * @param CoordinateSpace 坐标空间
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomSeed 随机种子
	 * @return 点位数组
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|矩形",
		meta = (DisplayName = "生成实心矩形点阵", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateSolidRectangle(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 100.0f,
		int32 RowCount = 0,
		int32 ColumnCount = 0,
		float Height = 1.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成空心矩形点阵
	 * @param Height 高度，支持3D矩形点阵（1=2D平面）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|矩形",
		meta = (DisplayName = "生成空心矩形点阵", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateHollowRectangle(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 100.0f,
		int32 RowCount = 0,
		int32 ColumnCount = 0,
		float Height = 1.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成螺旋矩形点阵（从内向外螺旋）
	 * @param Height 高度，支持3D矩形点阵（1=2D平面）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|矩形",
		meta = (DisplayName = "生成螺旋矩形点阵", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateSpiralRectangle(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 100.0f,
		float SpiralTurns = 2.0f,
		float Height = 1.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	// ============================================================================
	// 三角形类阵型采样
	// ============================================================================

	/**
	 * 生成实心三角形点阵
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|三角形",
		meta = (DisplayName = "生成实心三角形点阵", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateSolidTriangle(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 100.0f,
		bool bInverted = false,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成空心三角形点阵
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|三角形",
		meta = (DisplayName = "生成空心三角形点阵", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateHollowTriangle(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 100.0f,
		bool bInverted = false,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	// ============================================================================
	// 圆形和雪花类阵型采样
	// ============================================================================

	/**
	 * 生成圆形/球体点阵
	 * @param PointCount 总点数；紧密堆叠未指定实心圆盘环间距时用作基础密度目标，实际点数随Z层级密度变化
	 * @param CenterLocation 中心位置
	 * @param Rotation 旋转
	 * @param Radius 半径
	 * @param bIs3D 是否为3D球体（false=2D圆形，true=3D球体）
	 * @param bSolid 是否生成实心圆/球（Uniform和ClosePacked模式有效）
	 * @param DistributionMode 分布模式（均匀/斐波那契/泊松/紧密堆叠）
	 * @param MinDistance 泊松分布的最小距离（仅Poisson模式有效）
	 * @param StartAngle 起始角度（仅Uniform模式有效）
	 * @param bClockwise 是否顺时针（仅Uniform模式有效）
	 * @param CoordinateSpace 坐标空间
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomSeed 随机种子
	 * @param SolidRingSpacing 实心圆盘内同心环的目标间距，0为自动
	 * @param SolidLayerSpacing 实心球相邻层的Z轴间距，0为自动；超过保持球形的最大值时自动限制
	 * @param SolidLayerDensity 紧密堆叠实心球的Z层级密度，1为基础层数，越大时仅增加水平截面数量并保持每层环密度
	 * @param MinimumPointsPerRing 实心球每个纬度截面的最小点数；总点数不足时优先保持总数
	 * @return 变换数组，位置为点位，旋转默认朝向圆/球外侧
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|圆形",
				meta = (DisplayName = "生成圆形/球体点阵", AdvancedDisplay = "MinDistance,StartAngle,bClockwise,JitterStrength,RandomSeed,bUseCache,SolidRingSpacing,MinimumPointsPerRing"))
	static TArray<FTransform> GenerateCircle(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Radius = 200.0f,
		bool bIs3D = false,
		bool bSolid = false,
		ECircleDistributionMode DistributionMode = ECircleDistributionMode::Uniform,
		float MinDistance = 50.0f,
		float StartAngle = 0.0f,
		bool bClockwise = true,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0,
		UPARAM(DisplayName = "启用缓存") bool bUseCache = true,
		UPARAM(DisplayName = "实心圆盘环间距") float SolidRingSpacing = 0.0f,
		UPARAM(DisplayName = "Z轴层间距") float SolidLayerSpacing = 0.0f,
		UPARAM(DisplayName = "Z层级密度", meta = (ClampMin = "1.0", UIMin = "1.0")) float SolidLayerDensity = 1.0f,
		UPARAM(DisplayName = "每个纬度层最小点数") int32 MinimumPointsPerRing = 6,
		UPARAM(DisplayName = "排序方式") EPointArrayOrderMode PointOrder = EPointArrayOrderMode::Preserve,
		UPARAM(DisplayName = "反转索引") bool bReverseOrder = false
	);

	/**
	 * 重排位置数组的索引顺序。坐标旋转用于将输出坐标还原到阵型自身坐标系；
	 * 世界空间旋转阵型应传入生成时使用的旋转，本地/原始空间保持默认即可。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|排序",
		meta = (DisplayName = "排序点位数组", Keywords = "点位 排序 索引 下到上 左到右 顺时针 逆时针",
			ToolTip = "仅重排数组索引，不修改点位坐标。圆形和球体排序使用中心位置、起始角和坐标旋转定义自身坐标系。",
			AdvancedDisplay = "CenterLocation,CoordinateRotation,StartAngle,LayerTolerance,bReverseOrder"))
	static TArray<FVector> SortPointArray(
		UPARAM(DisplayName = "点位") const TArray<FVector>& Points,
		UPARAM(DisplayName = "排序方式") EPointArrayOrderMode PointOrder = EPointArrayOrderMode::Preserve,
		UPARAM(DisplayName = "中心位置") FVector CenterLocation = FVector::ZeroVector,
		UPARAM(DisplayName = "坐标旋转") FRotator CoordinateRotation = FRotator::ZeroRotator,
		UPARAM(DisplayName = "起始角") float StartAngle = 0.0f,
		UPARAM(DisplayName = "分层容差") float LayerTolerance = 1.0f,
		UPARAM(DisplayName = "反转索引") bool bReverseOrder = false
	);

	/** 重排变换数组的索引顺序，保留每个变换的平移、旋转和缩放。 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|排序",
		meta = (DisplayName = "排序变换数组", Keywords = "变换 排序 索引 下到上 左到右 顺时针 逆时针",
			ToolTip = "仅重排数组索引，不修改各变换。圆形和球体排序使用中心位置、起始角和坐标旋转定义自身坐标系。",
			AdvancedDisplay = "CenterLocation,CoordinateRotation,StartAngle,LayerTolerance,bReverseOrder"))
	static TArray<FTransform> SortTransformArray(
		UPARAM(DisplayName = "变换") const TArray<FTransform>& Transforms,
		UPARAM(DisplayName = "排序方式") EPointArrayOrderMode PointOrder = EPointArrayOrderMode::Preserve,
		UPARAM(DisplayName = "中心位置") FVector CenterLocation = FVector::ZeroVector,
		UPARAM(DisplayName = "坐标旋转") FRotator CoordinateRotation = FRotator::ZeroRotator,
		UPARAM(DisplayName = "起始角") float StartAngle = 0.0f,
		UPARAM(DisplayName = "分层容差") float LayerTolerance = 1.0f,
		UPARAM(DisplayName = "反转索引") bool bReverseOrder = false
	);

	/**
	 * 生成雪花形点阵（多层同心圆）
	 * @param SnowflakeLayers 圆环层数
	 * @param Spacing 环与环间距
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|雪花",
		meta = (DisplayName = "生成雪花形点阵", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateSnowflake(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Radius = 300.0f,
		int32 SnowflakeLayers = 3,
		float Spacing = 200.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成雪花弧形点阵（部分圆弧的多层）
	 * @param ArcAngle 弧度范围（度）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|雪花",
		meta = (DisplayName = "生成雪花弧形点阵", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateSnowflakeArc(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Radius = 300.0f,
		int32 SnowflakeLayers = 3,
		float Spacing = 150.0f,
		float ArcAngle = 180.0f,
		float StartAngle = -90.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	// ============================================================================
	// 军事阵型采样
	// ============================================================================

	/**
	 * 生成楔形阵型 (适用于突破战术)
	 * 特点：尖端向前，形成V形，便于集中火力突破防线
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|军事阵型",
		meta = (DisplayName = "生成楔形阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateWedgeFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 200.0f,
		float WedgeAngle = 60.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成纵队阵型 (适用于通过狭窄地形)
	 * 特点：单列纵队，最小横向宽度，适用于通过桥梁、走廊等狭窄区域
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|军事阵型",
		meta = (DisplayName = "生成纵队阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateColumnFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 150.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成横队阵型 (适用于火力覆盖)
	 * 特点：单排横队，最大横向火力覆盖，适用于阵地防御或火力压制
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|军事阵型",
		meta = (DisplayName = "生成横队阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateLineFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 200.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成V形阵型 (适用于防御战术)
	 * 特点：尖端向后，形成倒V形，便于两翼包抄和后方防御
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|军事阵型",
		meta = (DisplayName = "生成V形阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateVeeFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 200.0f,
		float VeeAngle = 45.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成梯形阵型 (适用于侧翼攻击)
	 * @param Direction 梯形方向 (-1=左梯形, 1=右梯形)
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|军事阵型",
		meta = (DisplayName = "生成梯形阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateEchelonFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 200.0f,
		int32 Direction = 1,
		float EchelonAngle = 30.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	// ============================================================================
	// 几何阵型采样
	// ============================================================================

	/**
	 * 生成蜂巢阵型 (六边形网格)
	 * 特点：最紧凑的2D填充模式，自然界中最优的点分布
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|几何阵型",
		meta = (DisplayName = "生成蜂巢阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateHexagonalGrid(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 100.0f,
		int32 Rings = 3,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成星形阵型 (五角星)
	 * @param PointsCount 星角数量 (5=五角星, 6=六角星等)
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|几何阵型",
		meta = (DisplayName = "生成星形阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateStarFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float OuterRadius = 200.0f,
		float InnerRadius = 100.0f,
		int32 PointsCount = 5,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成阿基米德螺旋阵型
	 * 特点：等距螺旋线，适用于自然生长、漩涡效果
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|几何阵型",
		meta = (DisplayName = "生成阿基米德螺旋", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateArchimedeanSpiral(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Spacing = 20.0f,
		float Turns = 3.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成对数螺旋阵型 (黄金螺旋)
	 * 特点：斐波那契螺旋，自然界中最常见的螺旋形态
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|几何阵型",
		meta = (DisplayName = "生成对数螺旋", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateLogarithmicSpiral(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float GrowthFactor = 1.1f,
		float AngleStep = 20.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成心脏形阵型
	 * 特点：心形曲线，适用于浪漫、爱心等视觉效果
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|几何阵型",
		meta = (DisplayName = "生成心脏阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateHeartFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Size = 200.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成花瓣阵型
	 * @param PetalCount 花瓣数量
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|几何阵型",
		meta = (DisplayName = "生成花瓣阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateFlowerFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float OuterRadius = 200.0f,
		float InnerRadius = 100.0f,
		int32 PetalCount = 5,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	// ============================================================================
	// 高级圆形阵型 (基于数学几何)
	// ============================================================================

	/**
	 * 生成黄金螺旋阵型（最自然的螺旋分布）
	 * 特点：斐波那契数列相关的黄金角，产生最均匀的螺旋分布
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|高级圆形",
		meta = (DisplayName = "生成黄金螺旋阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateGoldenSpiralFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float MaxRadius = 200.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成圆形网格阵型（极坐标网格）
	 * 特点：基于角度和半径的规则网格，便于控制密度
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|高级圆形",
		meta = (DisplayName = "生成圆形网格阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateCircularGridFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float MaxRadius = 200.0f,
		int32 RadialDivisions = 5,
		int32 AngularDivisions = 12,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成玫瑰曲线阵型（数学艺术曲线）
	 * @param Petals 花瓣数量（决定曲线复杂度）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|高级圆形",
		meta = (DisplayName = "生成玫瑰曲线阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateRoseCurveFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float MaxRadius = 200.0f,
		int32 Petals = 5,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	/**
	 * 生成同心圆环阵型（多层圆环分布）
	 * @param PointCount PointsPerRing为空时的目标点数
	 * @param PointsPerRing 每层的显式点数；非空时优先遵循该数组
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|高级圆形",
		meta = (DisplayName = "生成同心圆环阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateConcentricRingsFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		const TArray<int32>& PointsPerRing,
		float MaxRadius = 200.0f,
		int32 RingCount = 3,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
	);

	// ============================================================================
	// 样条线采样
	// ============================================================================

	/**
	 * 沿样条线生成点阵
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|样条线",
		meta = (DisplayName = "沿样条线生成点阵"))
	static TArray<FVector> GenerateAlongSpline(
		int32 PointCount,
		USplineComponent* SplineComponent,
		bool bClosedSpline = false,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World
	);

	/**
	 * 在样条线边界内生成泊松采样点阵
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|样条线",
		meta = (DisplayName = "样条线边界泊松采样", AdvancedDisplay = "MinDistance,RandomSeed"))
	static TArray<FVector> GenerateSplineBoundary(
		int32 TargetPointCount,
		USplineComponent* SplineComponent,
		float MinDistance = 50.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World,
		int32 RandomSeed = 0
	);

	// ============================================================================
	// 网格采样
	// ============================================================================

	/**
	 * 从静态网格体顶点生成点阵
	 *
	 * @param StaticMesh 静态网格体资产
	 * @param Transform 变换（位置、旋转、缩放）
	 * @param MaxPoints 最大点数（0=不限制，>0=智能降采样到目标数量）
	 * @param LODLevel LOD 级别（0=最高精度）
	 * @param bBoundaryVerticesOnly 仅边界顶点（暂未实现）
	 * @param DeduplicationRadius 去重半径（0=不去重，>0=移除距离小于此值的点）
	 * @param bGridAlignedDedup 网格对齐去重（true=对齐到规则网格，false=保留原始位置）
	 * @param CoordinateSpace 坐标空间
	 *
	 * 推荐设置：
	 * - 快速预览：MaxPoints = 500-1000
	 * - 普通使用：MaxPoints = 1000-5000
	 * - 高精度：MaxPoints = 10000+
	 * - 去重建议：DeduplicationRadius = 1.0-10.0（根据模型尺度调整）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|网格",
		meta = (DisplayName = "从静态网格体生成点阵",
			AdvancedDisplay = "LODLevel,bBoundaryVerticesOnly,DeduplicationRadius,bGridAlignedDedup"))
	static TArray<FVector> GenerateFromStaticMesh(
		UStaticMesh* StaticMesh,
		FTransform Transform,
		int32 MaxPoints = 1000,
		int32 LODLevel = 0,
		bool bBoundaryVerticesOnly = false,
		float DeduplicationRadius = 0.0f,
		bool bGridAlignedDedup = false,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World
	);

	/**
	 * 从静态网格体生成规则体素点位，适合后续生成小方块/乐高块。
	 *
	 * 使用三角形驱动体素化：先标记与模型表面三角面相交的体素，再可选对封闭模型内部做填充。
	 * 返回的颜色优先使用CPU可读资产顶点色并按表面命中点插值，缺失或不可读时尝试通过UV0采样材质BaseColor/Albedo/Diffuse贴图参数或材质引用的颜色贴图候选，再回退材质颜色参数、编辑器BaseColor链简单颜色常量或白色；内部颜色继承邻近表面体素，材质索引为体素命中表面的推荐主导材质槽。
	 * 注意：运行时使用需要 StaticMesh 资产启用 Allow CPU Access；内部填充假定网格基本封闭。
	 *
	 * @param StaticMesh 静态网格体资产
	 * @param Transform 模型世界变换
	 * @param VoxelSize 体素边长（世界单位，cm）
	 * @param FillMode 填充模式：仅表面 或 表面+内部填充
	 * @param LODLevel LOD 级别（0=最高精度）
	 * @param MaxVoxelCount 最大输出体素数量保护（同步节点最多500万个）；达到上限会截断或提前停止扫描
	 * @return 体素中心点、推荐实例变换、体素边长和颜色/材质信息
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|网格",
		meta = (DisplayName = "从静态网格体生成体素点位",
			AdvancedDisplay = "LODLevel,MaxVoxelCount",
			VoxelSize = "50.0",
			MaxVoxelCount = "1000000",
			ToolTip = "将静态网格体转换为规则体素点位，用于生成小方块/乐高块。支持仅表面或内部填充，并返回推荐实例变换、体素边长、颜色与推荐主导材质索引信息。\n\n推荐实例变换缩放保持1；如小方块原始尺寸不是1cm，请用体素边长/方块原始尺寸计算实例缩放。\n颜色优先使用CPU可读资产顶点色并按表面命中点插值；顶点色缺失或不可读时会尝试通过UV0采样材质BaseColor/Albedo/Diffuse贴图参数，或从材质引用贴图中保守选择颜色贴图候选（编辑器源数据或运行时直接可读Texture2D格式），再回退材质颜色参数、编辑器BaseColor链简单颜色常量或白色；不会完整求值复杂材质图。\nMaxVoxelCount是最大输出体素数量保护，同步节点最多500万个；表面模式达到上限会提前停止扫描，内部填充输出达到上限会截断，并另有工作内存和体素化工作量保护；极端模型也可能因内部保护返回部分表面结果。\n运行时要求：StaticMesh需启用Allow CPU Access。\n内部填充要求：模型应基本封闭；开口/自交/极薄模型可能只产生表面点或局部误填。\n负缩放会镜像体素网格索引。"))
	static TArray<FMeshVoxelPoint> GenerateVoxelPointsFromStaticMesh(
		UPARAM(DisplayName = "静态网格体") UStaticMesh* StaticMesh,
		UPARAM(DisplayName = "变换") FTransform Transform,
		UPARAM(DisplayName = "体素边长", meta = (ClampMin = "0.001", UIMin = "1.0")) float VoxelSize = 50.0f,
		UPARAM(DisplayName = "填充模式") EMeshVoxelFillMode FillMode = EMeshVoxelFillMode::SurfaceOnly,
		UPARAM(DisplayName = "LOD级别", meta = (ClampMin = "0", UIMin = "0")) int32 LODLevel = 0,
		UPARAM(DisplayName = "最大体素数量", meta = (ClampMin = "1", UIMin = "1", ClampMax = "5000000", UIMax = "1000000")) int32 MaxVoxelCount = 1000000
	);

#if WITH_EDITOR
	/**
	 * 验证纹理是否设置为未压缩格式（用于调试，仅编辑器可用）
	 *
	 * @param Texture 要验证的纹理
	 * @return 如果纹理设置正确返回 true，否则返回 false 并输出错误日志
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|纹理",
		meta = (DisplayName = "验证纹理采样设置",
			ToolTip = "检查纹理是否设置为未压缩格式，并输出详细的设置信息",
			DevelopmentOnly))
	static bool ValidateTextureForSampling(UTexture2D* Texture);
#endif
};
