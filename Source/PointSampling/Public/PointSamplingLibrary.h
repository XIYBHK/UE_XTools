/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PointSamplingTypes.h"
#include "PointSamplingLibrary.generated.h"

class UBoxComponent;

/**
 * 点采样蓝图函数库
 * 
 *  完全匹配原XToolsLibrary的Blueprint接口
 */
UCLASS()
class POINTSAMPLING_API UPointSamplingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ============================================================================
	// 基础2D/3D采样
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样 2D"))
	static TArray<FVector2D> GeneratePoissonPoints2D(
		float Width,
		float Height,
		float Radius,
		int32 MaxAttempts = 30
	);

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样 3D"))
	static TArray<FVector> GeneratePoissonPoints3D(
		float Width,
		float Height,
		float Depth,
		float Radius,
		int32 MaxAttempts = 30
	);

	// ============================================================================
	// Box组件采样（完全匹配原XToolsLibrary签名）
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样（Box组件）",
			AdvancedDisplay = "TargetPointCount,JitterStrength,bUseCache"))
	static TArray<FVector> GeneratePoissonPointsInBox(
		UPARAM(ref) UBoxComponent* BoxComponent,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f,
		bool bUseCache = true
	);

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson",
		meta = (DisplayName = "泊松采样（Box参数）",
			ScriptMethod = "GeneratePoissonPointsInBox",
			AdvancedDisplay = "TargetPointCount,JitterStrength,bUseCache"))
	static TArray<FVector> GeneratePoissonPointsInBoxByVector(
		FVector BoxExtent,
		FTransform Transform,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f,
		bool bUseCache = true
	);

	// ============================================================================
	// FromStream版本（流送）
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson|Stream",
		meta = (DisplayName = "泊松采样（流送-Box组件）",
			AdvancedDisplay = "TargetPointCount,JitterStrength"))
	static TArray<FVector> GeneratePoissonPointsInBoxFromStream(
		const FRandomStream& RandomStream,
		UPARAM(ref) UBoxComponent* BoxComponent,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f
	);

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Poisson|Stream",
		meta = (DisplayName = "泊松采样（流送-Box参数）",
			ScriptMethod = "GeneratePoissonPointsInBoxByVector",
			AdvancedDisplay = "TargetPointCount,JitterStrength"))
	static TArray<FVector> GeneratePoissonPointsInBoxByVectorFromStream(
		const FRandomStream& RandomStream,
		FVector BoxExtent,
		FTransform Transform,
		float Radius = 50.0f,
		int32 MaxAttempts = 30,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		int32 TargetPointCount = 0,
		float JitterStrength = 0.0f
	);

	// ============================================================================
	// 缓存管理
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Cache",
		meta = (DisplayName = "清空泊松采样缓存"))
	static void ClearPoissonSamplingCache();

	UFUNCTION(BlueprintCallable, Category = "Point Sampling|Cache",
		meta = (DisplayName = "获取泊松缓存统计"))
	static void GetPoissonSamplingCacheStats(int32& OutHits, int32& OutMisses);

	// ============================================================================
	// 采样质量验证
	// ============================================================================

	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|分析",
		meta = (DisplayName = "分析采样点统计",
			ToolTip = "计算点集的距离统计信息，包括最小距离、最大距离、平均距离和质心位置。\n\n参数:\nPoints - 要分析的点集\nOutMinDistance - 输出最小点间距\nOutMaxDistance - 输出最大点间距\nOutAvgDistance - 输出平均点间距\nOutCentroid - 输出点集质心位置",
			Keywords = "统计,分析,距离,质心"))
	static void AnalyzeSamplingStats(
		const TArray<FVector>& Points,
		float& OutMinDistance,
		float& OutMaxDistance,
		float& OutAvgDistance,
		FVector& OutCentroid
	);

	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|分析",
		meta = (DisplayName = "验证泊松采样质量",
			ToolTip = "验证泊松采样结果是否满足最小距离约束。\n\n参数:\nPoints - 要验证的点集\nExpectedMinDistance - 期望的最小距离\nTolerance - 容差范围 (0.0-1.0)\n\n返回值:\n如果所有点对距离都大于等于 (ExpectedMinDistance * (1-Tolerance)) 则返回true",
			Keywords = "验证,质量,泊松,约束"))
	static bool ValidatePoissonSampling(
		const TArray<FVector>& Points,
		float ExpectedMinDistance,
		float Tolerance = 0.1f
	);

	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|分析",
		meta = (DisplayName = "计算点集分布均匀性"))
	static float CalculateDistributionUniformity(const TArray<FVector>& Points);

	// ============================================================================
	// 纹理采样（智能统一接口）
	// ============================================================================

	/**
	 * 从纹理生成点阵（智能模式，支持所有格式）
	 *
	 * 自动检测纹理格式并选择最优方法：
	 * - 未压缩格式（BGRA8/RGBA8）：直接读取（更快）
	 * - 压缩格式（DXT/BC等）：自动渲染（通用）
	 *
	 * @param Texture 纹理对象（支持所有UE格式，包括DXT/BC压缩）
	 * @param MaxSampleSize 最大采样尺寸（纹理会被降采样到此尺寸）
	 * @param Spacing 像素步长（值越大点越稀疏）
	 * @param PixelThreshold 像素阈值 (0-1)，只采样高于此值的像素
	 * @param TextureScale 纹理缩放（影响生成点位的物理尺寸）
	 * @param SamplingChannel 采样通道（自动/Alpha/亮度/红/绿/蓝）
	 * @return 点位数组（局部坐标，居中）
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|纹理",
		meta = (DisplayName = "从纹理生成点阵",
			ToolTip = "智能纹理采样，自动支持所有格式（DXT/BC压缩等）。\n\n参数:\nTexture - 纹理对象\nMaxSampleSize - 最大采样尺寸\nSpacing - 像素步长\nPixelThreshold - 像素阈值(0-1)\nTextureScale - 纹理缩放\nSamplingChannel - 采样通道\n\n返回值:\n点位数组（局部坐标，居中）",
			Keywords = "纹理,texture,图片,image,采样,点阵,pattern",
			AdvancedDisplay = "SamplingChannel"))
	static TArray<FVector> GeneratePointsFromTexture(
		UTexture2D* Texture,
		int32 MaxSampleSize = 512,
		float Spacing = 1.0f,
		float PixelThreshold = 0.5f,
		float TextureScale = 1.0f,
		ETextureSamplingChannel SamplingChannel = ETextureSamplingChannel::Auto
	);

	/**
	 * 从纹理生成点阵（泊松圆盘采样）
	 *
	 * 基于纹理密度的泊松圆盘采样，分布更均匀
	 *
	 * @param Texture 纹理对象
	 * @param MaxSampleSize 最大采样尺寸
	 * @param MinRadius 最小采样半径（密集区域）
	 * @param MaxRadius 最大采样半径（稀疏区域）
	 * @param PixelThreshold 像素阈值 (0-1)
	 * @param TextureScale 纹理缩放
	 * @param SamplingChannel 采样通道
	 * @param MaxAttempts 最大尝试次数
	 * @return 基于纹理密度的泊松采样点位数组
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|纹理",
		meta = (DisplayName = "从纹理生成点阵（泊松采样）",
			ToolTip = "基于纹理密度的泊松圆盘采样，分布更均匀。\n\n参数:\nTexture - 纹理对象\nMaxSampleSize - 最大采样尺寸\nMinRadius - 最小半径\nMaxRadius - 最大半径\nPixelThreshold - 像素阈值\nTextureScale - 纹理缩放\nSamplingChannel - 采样通道\nMaxAttempts - 最大尝试次数",
			Keywords = "纹理,texture,图片,泊松,poisson,采样,点阵",
			AdvancedDisplay = "SamplingChannel,MaxAttempts"))
	static TArray<FVector> GeneratePointsFromTextureWithPoisson(
		UTexture2D* Texture,
		int32 MaxSampleSize = 512,
		float MinRadius = 10.0f,
		float MaxRadius = 50.0f,
		float PixelThreshold = 0.5f,
		float TextureScale = 1.0f,
		ETextureSamplingChannel SamplingChannel = ETextureSamplingChannel::Auto,
		int32 MaxAttempts = 30
	);

	// ============================================================================
	// 通用阵型生成器 (基于模式选择)
	// ============================================================================

	/**
	 * 通用阵型生成器 - 根据模式自动选择合适的阵型算法
	 * 这是一个统一的接口，支持所有阵型模式的生成
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|点采样|通用阵型",
		meta = (DisplayName = "生成阵型（通用）",
			ToolTip = "根据指定的阵型模式生成点集。这是所有阵型生成器的统一接口。\n\n参数:\nMode - 阵型模式（选择具体的阵型类型）\nPointCount - 点数量\nCenterLocation - 中心位置\nRotation - 旋转角度\nCoordinateSpace - 坐标空间\n其他参数根据模式自动选择相关参数",
			Keywords = "阵型,formation,pattern,通用"))
	static TArray<FVector> GenerateFormation(
		EPointSamplingMode Mode,
		int32 PointCount,
		FVector CenterLocation,
		FRotator Rotation = FRotator::ZeroRotator,
		EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local,
		float Spacing = 100.0f,
		float JitterStrength = 0.0f,
		int32 RandomSeed = 0,
		// 阵型特定参数
		float Param1 = 0.0f, // 根据模式不同含义不同
		float Param2 = 0.0f, // 根据模式不同含义不同
		int32 Param3 = 0     // 根据模式不同含义不同
	);
};
