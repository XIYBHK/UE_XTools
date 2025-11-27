/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
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

private:
	/**
	 * 计算像素的采样值（基于 Alpha 或亮度）
	 * @param Color 像素颜色
	 * @param bHasAlphaChannel 纹理是否有 Alpha 通道
	 * @return 采样值 (0-1)
	 */
	static float CalculatePixelSamplingValue(const FColor& Color, bool bHasAlphaChannel);
};
