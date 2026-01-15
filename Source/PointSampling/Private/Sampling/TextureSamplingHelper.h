/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "PointSamplingTypes.h"

class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;

/**
 * 纹理采样算法辅助类
 *
 * 职责：实现从纹理像素生成点位的算法
 * - 基于像素亮度/Alpha 值采样
 * - 支持纹理缩放和阈值过滤
 * - 支持Material Instance采样（所有压缩格式）
 */
class FTextureSamplingHelper
{
public:
	// ============================================================================
	// 智能统一接口（推荐使用，自动处理所有格式）
	// ============================================================================

	/**
	 * 智能纹理采样（自动选择最优方法）
	 *
	 * 自动检测纹理格式：
	 * - 未压缩格式（BGRA8/RGBA8）：直接读取（更快）
	 * - 压缩格式（DXT/BC等）：自动创建Material并渲染（通用）
	 *
	 * @param Texture 纹理对象（支持所有UE格式）
	 * @param MaxSampleSize 最大采样尺寸
	 * @param Spacing 像素步长
	 * @param PixelThreshold 像素采样阈值 (0-1)
	 * @param TextureScale 纹理缩放
	 * @param SamplingChannel 采样通道选择
	 * @return 基于像素的点位数组（局部坐标，居中）
	 */
	static TArray<FVector> GenerateFromTextureAuto(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float Spacing,
		float PixelThreshold,
		float TextureScale,
		ETextureSamplingChannel SamplingChannel = ETextureSamplingChannel::Auto
	);

	/**
	 * 智能纹理采样（泊松圆盘，自动选择最优方法）
	 */
	static TArray<FVector> GenerateFromTextureAutoWithPoisson(
		UTexture2D* Texture,
		int32 MaxSampleSize,
		float MinRadius,
		float MaxRadius,
		float PixelThreshold,
		float TextureScale,
		ETextureSamplingChannel SamplingChannel = ETextureSamplingChannel::Auto,
		int32 MaxAttempts = 30
	);

	// ============================================================================
	// Material Instance 采样（高级用法）
	// ============================================================================

	/**
	 * 从材质实例采样生成点阵（支持所有纹理压缩格式）
	 *
	 * 原理：将Material渲染到RenderTarget，然后从RenderTarget读取像素
	 * 优点：支持所有UE纹理格式（DXT/BC压缩等）、支持材质节点处理
	 *
	 * @param Material 材质实例（应包含要采样的纹理）
	 * @param MaxSampleSize 最大采样尺寸
	 * @param Spacing 像素步长
	 * @param PixelThreshold 像素采样阈值 (0-1)
	 * @param TextureScale 纹理缩放
	 * @param SamplingChannel 采样通道选择
	 * @return 基于像素的点位数组（局部坐标，居中）
	 */
	static TArray<FVector> GenerateFromMaterial(
		UMaterialInterface* Material,
		int32 MaxSampleSize,
		float Spacing,
		float PixelThreshold,
		float TextureScale,
		ETextureSamplingChannel SamplingChannel = ETextureSamplingChannel::Auto
	);

	/**
	 * 从材质实例采样生成点阵（基于泊松圆盘）
	 */
	static TArray<FVector> GenerateFromMaterialWithPoisson(
		UMaterialInterface* Material,
		int32 MaxSampleSize,
		float MinRadius,
		float MaxRadius,
		float PixelThreshold,
		float TextureScale,
		ETextureSamplingChannel SamplingChannel = ETextureSamplingChannel::Auto,
		int32 MaxAttempts = 30
	);

	// ============================================================================
	// 原有纹理采样函数（保持向后兼容）
	// ============================================================================
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
	 * 根据通道选择计算像素采样值（新增）
	 * @param Color 像素颜色
	 * @param Channel 通道选择
	 * @return 采样值 (0-1)
	 */
	static float CalculatePixelSamplingValueByChannel(const FLinearColor& Color, ETextureSamplingChannel Channel);

	/**
	 * 采样纹理像素（简化实现，尝试读取未压缩纹理数据）
	 * @param Texture 纹理对象
	 * @param U U坐标 (0-1)
	 * @param V V坐标 (0-1)
	 * @return 像素颜色
	 */
	static FLinearColor SampleTexturePixel(UTexture2D* Texture, float U, float V);

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

	// ============================================================================
	// Material Instance 采样私有辅助函数
	// ============================================================================

	/**
	 * 检查纹理格式是否支持直接读取（未压缩格式）
	 * @param Texture 纹理对象
	 * @return true=可以直接读取，false=需要使用Material方法
	 */
	static bool IsTextureFormatDirectReadable(UTexture2D* Texture);

	/**
	 * 验证平台数据纹理格式是否支持，并输出错误日志
	 * @param PixelFormat 像素格式
	 * @param FunctionName 调用函数名（用于日志）
	 * @return true=格式支持，false=格式不支持
	 */
	static bool ValidateAndLogPlatformTextureFormat(EPixelFormat PixelFormat, const TCHAR* FunctionName);

	/**
	 * 创建临时材质用于纹理渲染
	 * @param Texture 源纹理
	 * @param World World Context（用于创建MaterialInstanceDynamic）
	 * @return 创建的材质实例
	 */
	static UMaterialInstanceDynamic* CreateTemporaryMaterialForTexture(UTexture2D* Texture, UWorld* World);

	/**
	 * 创建临时RenderTarget用于材质渲染
	 * @param Size RenderTarget尺寸
	 * @return 创建的RenderTarget对象
	 */
	static UTextureRenderTarget2D* CreateTemporaryRenderTarget(int32 Size);

	/**
	 * 将材质渲染到RenderTarget
	 * @param Material 要渲染的材质
	 * @param RenderTarget 目标RenderTarget
	 * @return 是否渲染成功
	 */
	static bool RenderMaterialToTarget(UMaterialInterface* Material, UTextureRenderTarget2D* RenderTarget);

	/**
	 * 从RenderTarget读取像素数据并生成点阵
	 * @param RenderTarget 源RenderTarget
	 * @param MaxSampleSize 最大采样尺寸
	 * @param Spacing 像素步长
	 * @param PixelThreshold 像素阈值
	 * @param TextureScale 纹理缩放
	 * @param SamplingChannel 采样通道
	 * @return 生成的点位数组
	 */
	static TArray<FVector> GeneratePointsFromRenderTarget(
		UTextureRenderTarget2D* RenderTarget,
		int32 MaxSampleSize,
		float Spacing,
		float PixelThreshold,
		float TextureScale,
		ETextureSamplingChannel SamplingChannel
	);

	/**
	 * 从RenderTarget读取像素数据并生成泊松采样点阵
	 */
	static TArray<FVector> GeneratePointsFromRenderTargetWithPoisson(
		UTextureRenderTarget2D* RenderTarget,
		int32 MaxSampleSize,
		float MinRadius,
		float MaxRadius,
		float PixelThreshold,
		float TextureScale,
		ETextureSamplingChannel SamplingChannel,
		int32 MaxAttempts
	);
};
