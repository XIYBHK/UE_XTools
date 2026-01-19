/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PointSamplingTypes.h"
#include "FormationSamplingLibrary.generated.h"

class USplineComponent;
class UStaticMesh;
class USkeletalMesh;
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
	 * @param PointCount 总点数
	 * @param CenterLocation 中心位置
	 * @param Rotation 旋转
	 * @param Radius 半径
	 * @param bIs3D 是否为3D球体（false=2D圆形，true=3D球体）
	 * @param DistributionMode 分布模式（均匀/斐波那契/泊松）
	 * @param MinDistance 泊松分布的最小距离（仅Poisson模式有效）
	 * @param StartAngle 起始角度（仅Uniform模式的2D圆形有效）
	 * @param bClockwise 是否顺时针（仅Uniform模式的2D圆形有效）
	 * @param CoordinateSpace 坐标空间
	 * @param JitterStrength 扰动强度(0-1)
	 * @param RandomSeed 随机种子
	 * @return 点位数组
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|圆形",
		meta = (DisplayName = "生成圆形/球体点阵", AdvancedDisplay = "MinDistance,StartAngle,bClockwise,JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateCircle(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float Radius = 200.0f,
		bool bIs3D = false,
		ECircleDistributionMode DistributionMode = ECircleDistributionMode::Uniform,
		float MinDistance = 50.0f,
		float StartAngle = 0.0f,
		bool bClockwise = true,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0
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
		float OuterRadius,
		float InnerRadius,
		int32 PetalCount,
		EPoissonCoordinateSpace CoordinateSpace,
		float JitterStrength,
		int32 RandomSeed
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
		float MaxRadius,
		EPoissonCoordinateSpace CoordinateSpace,
		float JitterStrength,
		int32 RandomSeed
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
		float MaxRadius,
		int32 RadialDivisions,
		int32 AngularDivisions,
		EPoissonCoordinateSpace CoordinateSpace,
		float JitterStrength,
		int32 RandomSeed
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
		float MaxRadius,
		int32 Petals,
		EPoissonCoordinateSpace CoordinateSpace,
		float JitterStrength,
		int32 RandomSeed
	);

	/**
	 * 生成同心圆环阵型（多层圆环分布）
	 * @param PointsPerRing 每层的点数数组
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|高级圆形",
		meta = (DisplayName = "生成同心圆环阵型", AdvancedDisplay = "JitterStrength,RandomSeed"))
	static TArray<FVector> GenerateConcentricRingsFormation(
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation,
		float MaxRadius,
		int32 RingCount,
		const TArray<int32>& PointsPerRing,
		EPoissonCoordinateSpace CoordinateSpace,
		float JitterStrength,
		int32 RandomSeed
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
	 * @param CoordinateSpace 坐标空间
	 *
	 * 推荐设置：
	 * - 快速预览：MaxPoints = 500-1000
	 * - 普通使用：MaxPoints = 1000-5000
	 * - 高精度：MaxPoints = 10000+
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|网格",
		meta = (DisplayName = "从静态网格体生成点阵",
			AdvancedDisplay = "LODLevel,bBoundaryVerticesOnly"))
	static TArray<FVector> GenerateFromStaticMesh(
		UStaticMesh* StaticMesh,
		FTransform Transform,
		int32 MaxPoints = 1000,
		int32 LODLevel = 0,
		bool bBoundaryVerticesOnly = false,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World
	);

	/**
	 * 从骨骼网格体插槽生成点阵
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|网格",
		meta = (DisplayName = "从骨骼插槽生成点阵"))
	static TArray<FVector> GenerateFromSkeletalSockets(
		USkeletalMesh* SkeletalMesh,
		FTransform Transform,
		const FString& SocketNamePrefix = TEXT(""),
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World
	);

	/**
	 * 从骨骼网格体骨骼生成点阵（完整版）
	 *
	 * @param SkeletalMesh 骨骼网格体资产
	 * @param Transform 应用到每个骨骼的变换
	 * @param Config 骨骼采样配置
	 * @param CoordinateSpace 坐标空间
	 * @return 骨骼变换数据数组（包含位置、旋转、骨骼名称等完整信息）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|网格",
		meta = (DisplayName = "从骨骼网格体骨骼生成点阵"))
	static TArray<FBoneTransformData> GenerateFromSkeletalBones(
		USkeletalMesh* SkeletalMesh,
		FTransform Transform,
		FSkeletalBoneSamplingConfig Config,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World
	);

	/**
	 * 从骨骼网格体骨骼生成点阵（便捷版）
	 *
	 * 简化版本，返回纯位置数组，适用于快速获取骨骼位置
	 *
	 * @param SkeletalMesh 骨骼网格体资产
	 * @param Transform 应用到每个骨骼的变换
	 * @param CoordinateSpace 坐标空间
	 * @return 骨骼位置数组
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|网格",
		meta = (DisplayName = "从骨骼网格体骨骼生成位置点阵（便捷）"))
	static TArray<FVector> GenerateFromSkeletalBones_Simple(
		USkeletalMesh* SkeletalMesh,
		FTransform Transform,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::World
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
