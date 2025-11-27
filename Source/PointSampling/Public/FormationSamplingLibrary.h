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
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|网格",
		meta = (DisplayName = "从静态网格体生成点阵"))
	static TArray<FVector> GenerateFromStaticMesh(
		UStaticMesh* StaticMesh,
		FTransform Transform,
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

	// ============================================================================
	// 纹理采样
	// ============================================================================

	/**
	 * 从纹理像素生成点阵（智能降采样方法）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|纹理",
		meta = (DisplayName = "从纹理像素生成点阵"))
	static TArray<FVector> GenerateFromTexture(
		UTexture2D* Texture,
		FVector CenterLocation,
		FRotator Rotation,
		int32 MaxSampleSize = 512,
		float Spacing = 20.0f,
		float PixelThreshold = 0.5f,
		float TextureScale = 1.0f,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local
	);
};
