/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "PointSamplingTypes.h"

class UTexture2D;

/**
 * 纹理采样算法辅助类
 *
 * 职责：实现从纹理像素生成点位的算法
 * - 基于像素亮度/Alpha 值采样
 * - 支持纹理缩放和阈值过滤
 */
class FTextureSamplingHelper
{
public:
	/**
	 * 从纹理像素生成点阵（智能降采样方法）
	 * @param Texture 纹理引用
	 * @param MaxSampleSize 最大采样尺寸（纹理会被降采样到此尺寸以下，控制最大点数量）
	 * @param Spacing 像素步长（采样间隔，值越大点越稀疏）
	 * @param PixelThreshold 像素采样阈值 (0-1)，只有亮度/Alpha 高于此值的像素会被采样
	 * @param TextureScale 纹理缩放（影响生成点位的物理尺寸）
	 * @return 基于像素的点位数组（局部坐标，居中）
	 */
	static TArray<FVector> GenerateFromTexture(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float Spacing,
		float PixelThreshold,
		float TextureScale
	);

	/**
	 * 从纹理像素生成点阵（基于泊松圆盘采样的改进算法）
	 * @param Texture 纹理引用
	 * @param MaxSampleSize 最大采样尺寸（纹理会被降采样到此尺寸以下，控制最大点数量）
	 * @param MinRadius 最小采样半径（密集区域）
	 * @param MaxRadius 最大采样半径（稀疏区域）
	 * @param PixelThreshold 像素采样阈值 (0-1)，只有亮度/Alpha 高于此值的像素会被采样
	 * @param TextureScale 纹理缩放（影响生成点位的物理尺寸）
	 * @param MaxAttempts 最大尝试次数（泊松采样参数）
	 * @return 基于纹理密度的泊松采样点位数组（局部坐标，居中）
	 */
	static TArray<FVector> GenerateFromTextureWithPoisson(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float MinRadius,
		float MaxRadius,
		float PixelThreshold,
		float TextureScale,
		int32 MaxAttempts = 30
	);

private:
#if WITH_EDITOR
	/**
	 * 检查纹理源格式是否受支持（参考 UE SubUVAnimation.cpp）
	 */
	static bool IsSupportedSourceFormat(ETextureSourceFormat Format);

	/**
	 * 获取每像素字节数（参考 UE SubUVAnimation.cpp）
	 */
	static uint32 GetBytesPerPixel(ETextureSourceFormat Format);
#endif

	/**
	 * 智能判断是否应该使用 Alpha 通道采样
	 * 基于纹理的 CompressionSettings 和 Alpha 通道属性
	 */
	static bool ShouldUseAlphaChannel(UTexture2D* Texture);

	/**
	 * 计算像素的采样值（基于 Alpha 或亮度）
	 * @param Color 像素颜色
	 * @param bUseAlpha 是否使用 Alpha 通道
	 * @return 采样值 (0-1)
	 */
	static float CalculatePixelSamplingValue(const FColor& Color, bool bUseAlpha);

	/**
	 * 根据纹理密度计算采样半径
	 * @param Density 纹理密度 (0-1)
	 * @param MinRadius 最小半径
	 * @param MaxRadius 最大半径
	 * @return 计算出的半径
	 */
	static float CalculateRadiusFromDensity(float Density, float MinRadius, float MaxRadius);

#if WITH_EDITOR
	/**
	 * 从编辑器源数据生成点阵（支持所有压缩格式）
	 */
	static TArray<FVector> GenerateFromTextureSource(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float Spacing,
		float PixelThreshold,
		float TextureScale
	);

	/**
	 * 从编辑器源数据生成点阵（基于泊松圆盘采样）
	 */
	static TArray<FVector> GenerateFromTextureSourceWithPoisson(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float MinRadius,
		float MaxRadius,
		float PixelThreshold,
		float TextureScale,
		int32 MaxAttempts
	);
	
	/**
	 * 获取纹理在指定坐标的密度值（编辑器版本）
	 */
	static float GetTextureDensityAtCoordinate(
		UTexture2D* Texture,
		const FVector2D& Coordinate,
		bool bUseAlphaChannel,
		ETextureSourceFormat SourceFormat,
		const uint8* SourceData,
		int32 OriginalWidth,
		int32 OriginalHeight,
		uint32 BytesPerPixel
	);
#endif

	/**
	 * 从运行时平台数据生成点阵（仅支持未压缩格式）
	 */
	static TArray<FVector> GenerateFromTexturePlatformData(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float Spacing,
		float PixelThreshold,
		float TextureScale
	);

	/**
	 * 从运行时平台数据生成点阵（基于泊松圆盘采样）
	 */
	static TArray<FVector> GenerateFromTexturePlatformDataWithPoisson(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float MinRadius,
		float MaxRadius,
		float PixelThreshold,
		float TextureScale,
		int32 MaxAttempts
	);
	
	/**
	 * 获取纹理在指定坐标的密度值（平台数据版本，运行时可用）
	 */
	static float GetTextureDensityAtCoordinatePlatform(
		UTexture2D* Texture,
		const FVector2D& Coordinate,
		bool bUseAlphaChannel,
		EPixelFormat PixelFormat,
		const uint8* PixelData,
		int32 OriginalWidth,
		int32 OriginalHeight,
		uint32 BytesPerPixel
	);
};
