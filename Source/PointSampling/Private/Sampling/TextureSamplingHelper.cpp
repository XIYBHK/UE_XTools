/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "Sampling/TextureSamplingHelper.h"
#include "Sampling/PointDeduplicationHelper.h"
#include "PointSamplingTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Float16.h"
#include "TextureResource.h"
#include "PixelFormat.h"
#include "Algorithms/PoissonDiskSampling.h"
#include "Kismet/KismetRenderingLibrary.h"

// ============================================================================
// 纹理采样常量定义
// ============================================================================
namespace TextureSamplingConstants
{
	/** 编辑器模式下最大允许采样点数 */
	constexpr int32 MaxAllowedPointsEditor = 100000;

	/** 运行时模式下最大允许采样点数 */
	constexpr int32 MaxAllowedPointsRuntime = 50000;

	/** Alpha 通道方差检测阈值（低于此值认为 Alpha 无效） */
	constexpr float AlphaStdDevThreshold = 10.0f;
}

// ============================================================================
// 辅助函数（参考 UE SubUVAnimation.cpp 的标准实现）
// ============================================================================

/**
 * 采样纹理像素（简化实现，尝试读取未压缩纹理数据）
 */
FLinearColor FTextureSamplingHelper::SampleTexturePixel(UTexture2D* Texture, float U, float V)
{
	if (!ensure(Texture))
	{
		return FLinearColor::Black;
	}

	// 验证纹理坐标范围
	if (!ensure(FMath::IsWithinInclusive(U, 0.0f, 1.0f) && FMath::IsWithinInclusive(V, 0.0f, 1.0f)))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 纹理坐标超出范围: U=%.3f, V=%.3f"), U, V);
		return FLinearColor::White;
	}

	// 尝试从PlatformData读取（仅对未压缩格式有效）
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogPointSampling, Verbose, TEXT("[纹理采样] PlatformData不可用，使用默认颜色"));
		return FLinearColor::White;
	}

	EPixelFormat PixelFormat = PlatformData->PixelFormat;
	if (PixelFormat != PF_B8G8R8A8 && PixelFormat != PF_R8G8B8A8)
	{
		// 仅在Verbose时警告，避免刷屏（因为某些压缩格式确实无法直接读取）
		UE_LOG(LogPointSampling, Verbose, TEXT("[纹理采样] 不支持直接读取的像素格式: %d"), static_cast<int32>(PixelFormat));
		return FLinearColor::White;
	}

	FTexture2DMipMap& Mip0 = PlatformData->Mips[0];
	const void* RawData = Mip0.BulkData.LockReadOnly();
	if (!RawData)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 无法锁定纹理数据"));
		return FLinearColor::White;
	}

	const int32 Width = PlatformData->SizeX;
	const int32 Height = PlatformData->SizeY;

	// 计算像素坐标
	const int32 PixelX = FMath::Clamp(FMath::RoundToInt(U * (Width - 1)), 0, Width - 1);
	const int32 PixelY = FMath::Clamp(FMath::RoundToInt(V * (Height - 1)), 0, Height - 1);

	// 读取像素数据
	const uint8* PixelData = static_cast<const uint8*>(RawData);
	const int64 PixelIndex = ((int64)PixelY * Width + PixelX) * 4;
	const int64 DataSize = Mip0.BulkData.GetBulkDataSize();

	// 边界检查：确保访问不会越界
	if (PixelIndex + 3 >= DataSize)
	{
		Mip0.BulkData.Unlock();
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 像素索引越界: Index=%lld, DataSize=%lld"), PixelIndex, DataSize);
		return FLinearColor::White;
	}

	FColor PixelColor;
	if (PixelFormat == PF_B8G8R8A8)
	{
		// BGRA格式 -> RGBA转换
		PixelColor = FColor(
			PixelData[PixelIndex + 2], // R (从B位置)
			PixelData[PixelIndex + 1], // G (从G位置)
			PixelData[PixelIndex + 0], // B (从R位置)
			PixelData[PixelIndex + 3]  // A (Alpha位置)
		);
	}
	else // PF_R8G8B8A8
	{
		// RGBA格式直接读取
		PixelColor = FColor(
			PixelData[PixelIndex + 0], // R
			PixelData[PixelIndex + 1], // G
			PixelData[PixelIndex + 2], // B
			PixelData[PixelIndex + 3]  // A
		);
	}

	Mip0.BulkData.Unlock();

	return FLinearColor(PixelColor);
}

// ============================================================================
// 纹理采样辅助函数
// ============================================================================

#if WITH_EDITOR
bool FTextureSamplingHelper::IsSupportedSourceFormat(ETextureSourceFormat Format)
{
	// 只支持 UE 官方支持的格式
	return Format == TSF_G8 || Format == TSF_BGRA8 || Format == TSF_RGBA16 || Format == TSF_RGBA16F;
}

uint32 FTextureSamplingHelper::GetBytesPerPixel(ETextureSourceFormat Format)
{
	switch (Format)
	{
	case TSF_G8:		return 1;
	case TSF_BGRA8:		return 4;
	case TSF_RGBA16:	return 8;
	case TSF_RGBA16F:	return sizeof(FFloat16) * 4;
	default:			return 0;
	}
}
#endif // WITH_EDITOR

bool FTextureSamplingHelper::ShouldUseAlphaChannel(UTexture2D* Texture)
{
	if (!Texture)
	{
		return false;
	}

#if WITH_EDITOR
	// 根据 CompressionSettings 判断纹理类型
	TextureCompressionSettings CompressionSettings = Texture->CompressionSettings;

	// 明确使用 Alpha 通道的纹理类型
	if (CompressionSettings == TC_Alpha ||		// Alpha 专用（BC4）
		CompressionSettings == TC_Masks)		// 蒙版纹理
	{
		return true;
	}

	// 不应该使用 Alpha 的纹理类型
	if (CompressionSettings == TC_Normalmap ||		// 法线贴图（BC5）
		CompressionSettings == TC_Grayscale ||		// 灰度图
		CompressionSettings == TC_Displacementmap ||
		CompressionSettings == TC_DistanceFieldFont)
	{
		return false;
	}
#endif

	// 默认纹理：需要智能判断Alpha通道是否包含有用信息
	// 问题：很多PNG虽然有Alpha通道，但Alpha值全是255（完全不透明），真正的图案在RGB中
	if (!Texture->HasAlphaChannel())
	{
		// 没有Alpha通道，必须使用Luminance
		return false;
	}

#if WITH_EDITOR
	// 编辑器模式：检查源数据的Alpha通道是否有效
	FTextureSource& Source = Texture->Source;
	if (Source.IsValid())
	{
		ETextureSourceFormat SourceFormat = Source.GetFormat();

		// 灰度图没有Alpha通道
		if (SourceFormat == TSF_G8)
		{
			return false;
		}

		// 对于BGRA8格式，采样部分像素检查Alpha通道方差
		if (SourceFormat == TSF_BGRA8)
		{
			TArray64<uint8> RawData;
			if (Source.GetMipData(RawData, 0))
			{
				int32 Width = Source.GetSizeX();
				int32 Height = Source.GetSizeY();

				// 采样策略：每隔N个像素采样一次（降低计算量）
				const int32 SampleStep = FMath::Max(1, Width / 32);  // 最多采样32x32个点
				const int32 BytesPerPixel = 4;

				TArray<uint8> AlphaSamples;
				AlphaSamples.Reserve((Width / SampleStep) * (Height / SampleStep));

				for (int32 Y = 0; Y < Height; Y += SampleStep)
				{
					for (int32 X = 0; X < Width; X += SampleStep)
					{
						int64 PixelIndex = ((int64)Y * Width + X) * BytesPerPixel;
						if (PixelIndex + 3 < RawData.Num())
						{
							uint8 Alpha = RawData[PixelIndex + 3];  // BGRA格式，Alpha在索引3
							AlphaSamples.Add(Alpha);
						}
					}
				}

				// 计算Alpha通道的统计信息
				if (AlphaSamples.Num() > 0)
				{
					// 计算平均值
					int32 Sum = 0;
					for (uint8 Alpha : AlphaSamples)
					{
						Sum += Alpha;
					}
					float Mean = Sum / (float)AlphaSamples.Num();

					// 计算方差
					float Variance = 0.0f;
					for (uint8 Alpha : AlphaSamples)
					{
						float Diff = Alpha - Mean;
						Variance += Diff * Diff;
					}
					Variance /= AlphaSamples.Num();

					// 计算标准差
					float StdDev = FMath::Sqrt(Variance);

					// 判断阈值：标准差小于阈值说明Alpha通道几乎是常数（例如全255或全0）
					if (StdDev < TextureSamplingConstants::AlphaStdDevThreshold)
					{
						// Alpha通道没有有效信息，使用Luminance
						UE_LOG(LogPointSampling, Log,
							TEXT("[纹理采样] Alpha通道方差过低（均值=%.1f, 标准差=%.1f），切换到Luminance通道"),
							Mean, StdDev);
						return false;
					}

					// Alpha通道有有效信息
					UE_LOG(LogPointSampling, Log,
						TEXT("[纹理采样] Alpha通道有效（均值=%.1f, 标准差=%.1f），使用Alpha通道"),
						Mean, StdDev);
					return true;
				}
			}
		}
	}
#endif

	// 运行时或无法检测：默认使用Alpha（保持向后兼容）
	return true;
}

float FTextureSamplingHelper::CalculatePixelSamplingValue(const FColor& Color, bool bUseAlpha)
{
	if (bUseAlpha)
	{
		// 使用 Alpha 通道
		return Color.A / 255.0f;
	}
	else
	{
		// 使用感知亮度公式（参考 UE ImportanceSamplingLibrary）
		// Luminance = 0.299*R + 0.587*G + 0.114*B
		float Luminance = (0.299f * Color.R + 0.587f * Color.G + 0.114f * Color.B) / 255.0f;
		return Luminance;
	}
}

float FTextureSamplingHelper::CalculateRadiusFromDensity(float Density, float MinRadius, float MaxRadius)
{
	// 确保密度在有效范围内
	Density = FMath::Clamp(Density, 0.0f, 1.0f);

	// 密度越高，半径越小（采样越密集）
	// 使用平滑的曲线映射，确保过渡自然
	float Radius = MaxRadius - (Density * (MaxRadius - MinRadius));

	return FMath::Max(MinRadius, Radius);
}

// ============================================================================
// 主入口函数
// ============================================================================
TArray<FVector> FTextureSamplingHelper::GenerateFromTexture(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float Spacing,
	float PixelThreshold,
	float TextureScale)
{
#if WITH_EDITOR
	// 编辑器模式优先使用源码数据
	return GenerateFromTextureSource(Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale);
#else
	// 运行时模式使用平台数据
	return GenerateFromTexturePlatformData(Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale);
#endif
}

TArray<FVector> FTextureSamplingHelper::GenerateFromTextureWithPoisson(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float MinRadius,
	float MaxRadius,
	float PixelThreshold,
	float TextureScale,
	int32 MaxAttempts)
{
#if WITH_EDITOR
	return GenerateFromTextureSourceWithPoisson(Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold, TextureScale, MaxAttempts);
#else
	return GenerateFromTexturePlatformDataWithPoisson(Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold, TextureScale, MaxAttempts);
#endif
}

// ============================================================================
// 编辑器版本实现
// ============================================================================

#if WITH_EDITOR

float FTextureSamplingHelper::GetTextureDensityAtCoordinate(
	UTexture2D* Texture,
	const FVector2D& Coordinate,
	bool bUseAlphaChannel,
	ETextureSourceFormat SourceFormat,
	const uint8* SourceData,
	int32 OriginalWidth,
	int32 OriginalHeight,
	uint32 BytesPerPixel)
{
	// 将归一化坐标转换为像素坐标
	int32 PixelX = FMath::Clamp(FMath::RoundToInt(Coordinate.X * (OriginalWidth - 1)), 0, OriginalWidth - 1);
	int32 PixelY = FMath::Clamp(FMath::RoundToInt(Coordinate.Y * (OriginalHeight - 1)), 0, OriginalHeight - 1);

	int64 PixelIndex = ((int64)PixelY * OriginalWidth + PixelX) * BytesPerPixel;

	float SamplingValue = 0.0f;

	if (SourceFormat == TSF_G8)
	{
		// 灰度 8 位：直接使用灰度值
		SamplingValue = SourceData[PixelIndex] / 255.0f;
	}
	else if (SourceFormat == TSF_BGRA8)
	{
		// BGRA8 格式（通道顺序：B, G, R, A）
		uint8 B = SourceData[PixelIndex + 0];
		uint8 G = SourceData[PixelIndex + 1];
		uint8 R = SourceData[PixelIndex + 2];
		uint8 A = SourceData[PixelIndex + 3];
		FColor Color(R, G, B, A);
		SamplingValue = CalculatePixelSamplingValue(Color, bUseAlphaChannel);
	}
	else if (SourceFormat == TSF_RGBA16)
	{
		// RGBA16 格式（16位/通道）
		uint16 R = *((uint16*)(&SourceData[PixelIndex + 0]));
		uint16 G = *((uint16*)(&SourceData[PixelIndex + 2]));
		uint16 B = *((uint16*)(&SourceData[PixelIndex + 4]));
		uint16 A = *((uint16*)(&SourceData[PixelIndex + 6]));
		FColor Color(FMath::RoundToInt(R / 257.0f), FMath::RoundToInt(G / 257.0f), FMath::RoundToInt(B / 257.0f), FMath::RoundToInt(A / 257.0f));
		SamplingValue = CalculatePixelSamplingValue(Color, bUseAlphaChannel);
	}
	else if (SourceFormat == TSF_RGBA16F)
	{
		// RGBA16F 格式（半浮点/通道）
		FFloat16 R = *((FFloat16*)(&SourceData[PixelIndex + 0]));
		FFloat16 G = *((FFloat16*)(&SourceData[PixelIndex + 2]));
		FFloat16 B = *((FFloat16*)(&SourceData[PixelIndex + 4]));
		FFloat16 A = *((FFloat16*)(&SourceData[PixelIndex + 6]));
		FColor Color(FMath::RoundToInt(R.GetFloat() * 255.0f), FMath::RoundToInt(G.GetFloat() * 255.0f), FMath::RoundToInt(B.GetFloat() * 255.0f), FMath::RoundToInt(A.GetFloat() * 255.0f));
		SamplingValue = CalculatePixelSamplingValue(Color, bUseAlphaChannel);
	}

	return SamplingValue;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromTextureSource(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float Spacing,
	float PixelThreshold,
	float TextureScale)
{
	TArray<FVector> Points;

	// 获取源纹理数据（Mip 0，未压缩）
	FTextureSource& Source = Texture->Source;
	if (!Source.IsValid())
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 纹理源数据无效"));
		return Points;
	}

	// 检查源格式是否受支持
	ETextureSourceFormat SourceFormat = Source.GetFormat();
	if (!IsSupportedSourceFormat(SourceFormat))
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理采样] 不支持的源纹理格式: %d（仅支持 G8/BGRA8/RGBA16/RGBA16F）"), (int32)SourceFormat);
		return Points;
	}

	// 获取源纹理尺寸
	int32 OriginalWidth = Source.GetSizeX();
	int32 OriginalHeight = Source.GetSizeY();

	if (OriginalWidth <= 0 || OriginalHeight <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 源纹理尺寸无效: %dx%d"), OriginalWidth, OriginalHeight);
		return Points;
	}

	// 锁定 Mip 0 数据
	TArray64<uint8> RawData;
	if (!Source.GetMipData(RawData, 0))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 无法获取源纹理 Mip 数据"));
		return Points;
	}
	const uint8* SourceData = RawData.GetData();
	uint32 BytesPerPixel = GetBytesPerPixel(SourceFormat);

	// 智能判断使用哪个通道采样
	bool bUseAlphaChannel = ShouldUseAlphaChannel(Texture);
	const TCHAR* ChannelName = bUseAlphaChannel ? TEXT("Alpha") : TEXT("Luminance");

	UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 源格式=%d, 尺寸=%dx%d, 压缩=%d, 采样通道=%s"),
		(int32)SourceFormat, OriginalWidth, OriginalHeight,
		(int32)Texture->CompressionSettings, ChannelName);

	// 验证参数
	if (MaxSampleSize <= 0 || Spacing <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 参数无效: MaxSampleSize=%d, Spacing=%f"), MaxSampleSize, Spacing);
		return Points;
	}

	// 计算降采样比率
	const float DownsampleRatio = FMath::Max(1.0f, FMath::Max((float)OriginalWidth / MaxSampleSize, (float)OriginalHeight / MaxSampleSize));
	const int32 SampleWidth = FMath::Max(1, FMath::RoundToInt(OriginalWidth / DownsampleRatio));
	const int32 SampleHeight = FMath::Max(1, FMath::RoundToInt(OriginalHeight / DownsampleRatio));
	const int32 Step = FMath::Max(1, FMath::RoundToInt(Spacing));

	// 限制最大点数
	const int32 EstimatedMaxPoints = (SampleWidth / Step) * (SampleHeight / Step);

	if (EstimatedMaxPoints > TextureSamplingConstants::MaxAllowedPointsEditor)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 预期点数 %d 超过限制 %d，减少采样密度"), EstimatedMaxPoints, TextureSamplingConstants::MaxAllowedPointsEditor);
		return Points; // 直接返回空数组，避免性能问题
	}

	UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 开始采样纹理 %dx%d -> %dx%d (步长=%d, 阈值=%.2f)"),
		OriginalWidth, OriginalHeight, SampleWidth, SampleHeight, Step, PixelThreshold);

	Points.Reserve(EstimatedMaxPoints / 4); // 预估25%会通过阈值

	// 遍历采样点
	for (int32 SampleY = 0; SampleY < SampleHeight; SampleY += Step)
	{
		for (int32 SampleX = 0; SampleX < SampleWidth; SampleX += Step)
		{
			// 映射回原始纹理坐标
			int32 OriginalX = FMath::RoundToInt(SampleX * DownsampleRatio);
			int32 OriginalY = FMath::RoundToInt(SampleY * DownsampleRatio);
			
			// 边界检查
			OriginalX = FMath::Clamp(OriginalX, 0, OriginalWidth - 1);
			OriginalY = FMath::Clamp(OriginalY, 0, OriginalHeight - 1);

			// 获取归一化坐标
			FVector2D Coord((float)OriginalX / (OriginalWidth - 1), (float)OriginalY / (OriginalHeight - 1));

			// 获取密度
			float SamplingValue = GetTextureDensityAtCoordinate(Texture, Coord, bUseAlphaChannel, SourceFormat, SourceData, OriginalWidth, OriginalHeight, BytesPerPixel);

			// 阈值过滤
			if (SamplingValue >= PixelThreshold)
			{
				// 转换为世界坐标 (居中, Y轴翻转)
				const float WorldX = (Coord.X - 0.5f) * OriginalWidth * TextureScale;
				const float WorldY = (0.5f - Coord.Y) * OriginalHeight * TextureScale;

				Points.Add(FVector(WorldX, WorldY, 0.0f));
			}
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 完成，生成 %d 个点"), Points.Num());

	// 去除重复点
	if (Points.Num() > 1)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			TextureScale * 0.1f, // 小容差
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Verbose, TEXT("[纹理采样] 去重: %d -> %d (移除 %d)"),
				OriginalCount, Points.Num(), RemovedCount);
		}
	}

	return Points;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromTextureSourceWithPoisson(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float MinRadius,
	float MaxRadius,
	float PixelThreshold,
	float TextureScale,
	int32 MaxAttempts)
{
	TArray<FVector> Points;

	// 获取源纹理数据（Mip 0，未压缩）
	FTextureSource& Source = Texture->Source;
	if (!Source.IsValid())
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 纹理源数据无效"));
		return Points;
	}

	// 检查源格式是否受支持
	ETextureSourceFormat SourceFormat = Source.GetFormat();
	if (!IsSupportedSourceFormat(SourceFormat))
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理密度采样] 不支持的源纹理格式: %d（仅支持 G8/BGRA8/RGBA16/RGBA16F）"), (int32)SourceFormat);
		return Points;
	}

	// 获取源纹理尺寸
	int32 OriginalWidth = Source.GetSizeX();
	int32 OriginalHeight = Source.GetSizeY();

	if (OriginalWidth <= 0 || OriginalHeight <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 源纹理尺寸无效: %dx%d"), OriginalWidth, OriginalHeight);
		return Points;
	}

	// 锁定 Mip 0 数据
	TArray64<uint8> RawData;
	if (!Source.GetMipData(RawData, 0))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 无法获取源纹理 Mip 数据"));
		return Points;
	}

	// 智能判断使用哪个通道采样
	bool bUseAlphaChannel = ShouldUseAlphaChannel(Texture);
	const TCHAR* ChannelName = bUseAlphaChannel ? TEXT("Alpha") : TEXT("Luminance");
	uint32 BytesPerPixel = GetBytesPerPixel(SourceFormat);

	UE_LOG(LogPointSampling, Log, TEXT("[纹理密度采样] 源格式=%d, 尺寸=%dx%d, 压缩=%d, 采样通道=%s"),
		(int32)SourceFormat, OriginalWidth, OriginalHeight,
		(int32)Texture->CompressionSettings, ChannelName);

	// 使用现有泊松圆盘采样生成初始点集
	// 计算采样区域大小
	float Width = OriginalWidth * TextureScale;
	float Height = OriginalHeight * TextureScale;

	// 使用最小半径生成泊松点集（确保最大密度）
	TArray<FVector2D> PoissonPoints = FPoissonDiskSampling::GeneratePoisson2D(Width, Height, MinRadius, MaxAttempts);
	UE_LOG(LogPointSampling, Log, TEXT("[纹理密度采样] 生成初始泊松点集: %d 个点"), PoissonPoints.Num());

	// 获取原始数据指针
	const uint8* SourceData = RawData.GetData();

	// 遍历泊松点集，根据纹理密度筛选和调整
	for (const FVector2D& PoissonPoint : PoissonPoints)
	{
		// 将泊松点坐标转换为纹理归一化坐标
		FVector2D NormalizedCoords;
		// 注意：FPoissonDiskSampling::GeneratePoisson2D 输出范围为 [0..Width]x[0..Height]
		// 纹理归一化坐标系：左上角(0,0)，右下角(1,1)
		NormalizedCoords.X = FMath::Clamp(PoissonPoint.X / Width, 0.0f, 1.0f);
		NormalizedCoords.Y = FMath::Clamp(PoissonPoint.Y / Height, 0.0f, 1.0f);

		// 确保坐标在有效范围内
		if (NormalizedCoords.X < 0.0f || NormalizedCoords.X > 1.0f ||
			NormalizedCoords.Y < 0.0f || NormalizedCoords.Y > 1.0f)
		{
			continue;
		}

		// 获取当前坐标的纹理密度值
		float Density = GetTextureDensityAtCoordinate(Texture, NormalizedCoords, bUseAlphaChannel, SourceFormat, SourceData, OriginalWidth, OriginalHeight, BytesPerPixel);

		// 如果密度低于阈值，跳过该点
		if (Density < PixelThreshold)
		{
			continue;
		}

		// 将点添加到结果集（局部坐标，居中，Y轴翻转以匹配纹理坐标系）
		const float LocalX = PoissonPoint.X - Width * 0.5f;
		const float LocalY = Height * 0.5f - PoissonPoint.Y;
		Points.Add(FVector(LocalX, LocalY, 0.0f));
	}

	UE_LOG(LogPointSampling, Log, TEXT("[纹理密度采样] 根据纹理密度筛选后剩余 %d 个点"), Points.Num());

	// 去除重复/重叠点位
	if (Points.Num() > 0)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			MinRadius * 0.1f,  // 容差：最小半径的10%
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log,
				TEXT("[纹理密度采样] 去重：原始 %d 个点 -> 去除 %d 个重复点 -> 剩余 %d 个点"),
				OriginalCount, RemovedCount, Points.Num());
		}
	}

	return Points;
}
#endif // WITH_EDITOR

// ============================================================================
// 平台数据版本实现（运行时）
// ============================================================================

float FTextureSamplingHelper::GetTextureDensityAtCoordinatePlatform(
	UTexture2D* Texture,
	const FVector2D& Coordinate,
	bool bUseAlphaChannel,
	EPixelFormat PixelFormat,
	const uint8* PixelData,
	int32 OriginalWidth,
	int32 OriginalHeight,
	uint32 BytesPerPixel)
{
	// 将归一化坐标转换为像素坐标
	int32 PixelX = FMath::Clamp(FMath::RoundToInt(Coordinate.X * (OriginalWidth - 1)), 0, OriginalWidth - 1);
	int32 PixelY = FMath::Clamp(FMath::RoundToInt(Coordinate.Y * (OriginalHeight - 1)), 0, OriginalHeight - 1);

	int64 PixelIndex = ((int64)PixelY * OriginalWidth + PixelX) * BytesPerPixel;

	float SamplingValue = 0.0f;

	// 处理不同的像素格式
	if (PixelFormat == PF_B8G8R8A8 || PixelFormat == PF_R8G8B8A8 || PixelFormat == PF_A8R8G8B8)
	{
		// BGRA8 或 RGBA8 格式
		uint8 B, G, R, A;

		if (PixelFormat == PF_B8G8R8A8)
		{
			// BGRA8 格式（通道顺序：B, G, R, A）
			B = PixelData[PixelIndex + 0];
			G = PixelData[PixelIndex + 1];
			R = PixelData[PixelIndex + 2];
			A = PixelData[PixelIndex + 3];
		}
		else if (PixelFormat == PF_R8G8B8A8)
		{
			// RGBA8 格式（通道顺序：R, G, B, A）
			R = PixelData[PixelIndex + 0];
			G = PixelData[PixelIndex + 1];
			B = PixelData[PixelIndex + 2];
			A = PixelData[PixelIndex + 3];
		}
		else // PF_A8R8G8B8
		{
			// ARGB8 格式（通道顺序：A, R, G, B）
			A = PixelData[PixelIndex + 0];
			R = PixelData[PixelIndex + 1];
			G = PixelData[PixelIndex + 2];
			B = PixelData[PixelIndex + 3];
		}

		FColor Color(R, G, B, A);
		SamplingValue = CalculatePixelSamplingValue(Color, bUseAlphaChannel);
	}
	else if (PixelFormat == PF_FloatRGBA)
	{
		// FloatRGBA：不同平台可能是 FP32x4（16字节）或 FP16x4（8字节）
		float R = 0.0f, G = 0.0f, B = 0.0f, A = 0.0f;

		if (BytesPerPixel == 16)
		{
			const float* FloatData = reinterpret_cast<const float*>(PixelData + PixelIndex);
			R = FloatData[0];
			G = FloatData[1];
			B = FloatData[2];
			A = FloatData[3];
		}
		else if (BytesPerPixel == 8)
		{
			const FFloat16* HalfData = reinterpret_cast<const FFloat16*>(PixelData + PixelIndex);
			R = HalfData[0].GetFloat();
			G = HalfData[1].GetFloat();
			B = HalfData[2].GetFloat();
			A = HalfData[3].GetFloat();
		}
		else
		{
			return 0.0f;
		}

		FColor Color(
			FMath::Clamp(FMath::RoundToInt(R * 255.0f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(G * 255.0f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(B * 255.0f), 0, 255),
			FMath::Clamp(FMath::RoundToInt(A * 255.0f), 0, 255)
		);
		SamplingValue = CalculatePixelSamplingValue(Color, bUseAlphaChannel);
	}

	return SamplingValue;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromTexturePlatformData(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float Spacing,
	float PixelThreshold,
	float TextureScale)
{
	TArray<FVector> Points;

	// 检查平台数据有效性（运行时纹理）
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 纹理平台数据无效"));
		return Points;
	}

	// 验证参数
	if (MaxSampleSize <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 最大采样尺寸无效: %d"), MaxSampleSize);
		return Points;
	}

	if (Spacing <= 0.0f)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 间距无效: %f"), Spacing);
		return Points;
	}

	// 获取纹理尺寸（使用运行时数据）
	int32 OriginalWidth = Texture->GetSizeX();
	int32 OriginalHeight = Texture->GetSizeY();

	if (OriginalWidth <= 0 || OriginalHeight <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 纹理尺寸无效: %dx%d"), OriginalWidth, OriginalHeight);
		return Points;
	}

	// 检查纹理格式是否支持
	EPixelFormat PixelFormat = PlatformData->PixelFormat;

	if (!ValidateAndLogPlatformTextureFormat(PixelFormat, TEXT("纹理采样")))
	{
		return Points;
	}

	// 获取纹理资源
	FTextureResource* TextureResource = Texture->GetResource();
	if (!TextureResource)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 纹理资源无效"));
		return Points;
	}

	// 获取 Mip 0 数据（最高分辨率）
	FTexture2DMipMap& Mip0 = PlatformData->Mips[0];
	const void* RawData = Mip0.BulkData.LockReadOnly();
	if (!RawData)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 无法锁定纹理数据"));
		return Points;
	}

	// 计算BytesPerPixel
	const int64 PixelCount64 = (int64)OriginalWidth * (int64)OriginalHeight;
	const int64 DataSize64 = (int64)Mip0.BulkData.GetBulkDataSize();
	uint32 BytesPerPixel = (uint32)(DataSize64 / PixelCount64);

	const uint8* PixelData = static_cast<const uint8*>(RawData);

	// 计算降采样比率（保持纵横比）
	float DownsampleRatio = 1.0f;
	if (OriginalWidth > MaxSampleSize || OriginalHeight > MaxSampleSize)
	{
		float WidthRatio = (float)OriginalWidth / MaxSampleSize;
		float HeightRatio = (float)OriginalHeight / MaxSampleSize;
		DownsampleRatio = FMath::Max(WidthRatio, HeightRatio);
	}

	// 计算采样尺寸
	int32 SampleWidth = FMath::Max(1, FMath::RoundToInt(OriginalWidth / DownsampleRatio));
	int32 SampleHeight = FMath::Max(1, FMath::RoundToInt(OriginalHeight / DownsampleRatio));

	// 计算像素步长（结合 Spacing 参数）
	int32 Step = FMath::Max(1, FMath::RoundToInt(Spacing));

	// 智能判断使用哪个通道采样（与编辑器路径保持一致）
	bool bUseAlphaChannel = ShouldUseAlphaChannel(Texture);
	const TCHAR* ChannelName = bUseAlphaChannel ? TEXT("Alpha") : TEXT("Luminance");

	UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 运行时数据：尺寸=%dx%d, 格式=%d, 压缩=%d, 采样通道=%s"),
		OriginalWidth, OriginalHeight, (int32)PixelFormat,
		(int32)Texture->CompressionSettings, ChannelName);

	// 计算预期最大点数（防止卡死）
	int32 EstimatedMaxPoints = (SampleWidth / Step) * (SampleHeight / Step);

	if (EstimatedMaxPoints > TextureSamplingConstants::MaxAllowedPointsRuntime)
	{
		// 自动调整 Step 以限制点数
		int32 RequiredStep = FMath::CeilToInt(FMath::Sqrt((float)(SampleWidth * SampleHeight) / TextureSamplingConstants::MaxAllowedPointsRuntime));
		Step = FMath::Max(Step, RequiredStep);

		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 预期点数 %d 超过限制 %d，自动调整 Spacing 从 %.1f 到 %d"),
			EstimatedMaxPoints, TextureSamplingConstants::MaxAllowedPointsRuntime, Spacing, Step);

		EstimatedMaxPoints = (SampleWidth / Step) * (SampleHeight / Step);
	}

	// 预估点数
	int32 EstimatedPoints = (SampleWidth / Step) * (SampleHeight / Step) / 2;
	Points.Reserve(EstimatedPoints);

	// 遍历降采样后的像素（使用步长）
	for (int32 SampleY = 0; SampleY < SampleHeight; SampleY += Step)
	{
		for (int32 SampleX = 0; SampleX < SampleWidth; SampleX += Step)
		{
			// 映射回原始纹理坐标
			int32 OriginalX = FMath::RoundToInt(SampleX * DownsampleRatio);
			int32 OriginalY = FMath::RoundToInt(SampleY * DownsampleRatio);

			// 边界检查
			if (OriginalX >= OriginalWidth || OriginalY >= OriginalHeight)
			{
				continue;
			}

			// 获取归一化坐标
			FVector2D Coord((float)OriginalX / (OriginalWidth - 1), (float)OriginalY / (OriginalHeight - 1));

			// 获取密度
			float SamplingValue = GetTextureDensityAtCoordinatePlatform(Texture, Coord, bUseAlphaChannel, PixelFormat, PixelData, OriginalWidth, OriginalHeight, BytesPerPixel);

			// 如果采样值高于阈值，创建点位
			if (SamplingValue >= PixelThreshold)
			{
				// 将像素坐标转换为局部坐标（居中，Y 轴翻转以匹配纹理坐标系）
				const float NormalizedX = (OriginalX / (float)OriginalWidth) - 0.5f;  // [-0.5, 0.5]
				const float NormalizedY = 0.5f - (OriginalY / (float)OriginalHeight); // Y 轴翻转

				FVector Point(
					NormalizedX * OriginalWidth * TextureScale,  // 使用原始尺寸保持纹理比例
					NormalizedY * OriginalHeight * TextureScale,
					0.0f
				);

				Points.Add(Point);
			}
		}
	}

	// 解锁纹理数据
	Mip0.BulkData.Unlock();

	UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 从运行时数据生成 %d 个点（尺寸=%dx%d, 格式=%d, 通道=%s）"),
		Points.Num(), OriginalWidth, OriginalHeight, (int32)PixelFormat, ChannelName);

	// 去除重复/重叠点位（纹理降采样可能产生重复点）
	if (Points.Num() > 0)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			TextureScale * 0.5f,  // 容差：纹理缩放的一半
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log,
				TEXT("[纹理采样] 去重：原始 %d 个点 -> 去除 %d 个重复点 -> 剩余 %d 个点"),
				OriginalCount, RemovedCount, Points.Num());
		}
	}

	return Points;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromTexturePlatformDataWithPoisson(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float MinRadius,
	float MaxRadius,
	float PixelThreshold,
	float TextureScale,
	int32 MaxAttempts)
{
	TArray<FVector> Points;

	// 检查平台数据有效性（运行时纹理）
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 纹理平台数据无效"));
		return Points;
	}

	// 获取纹理尺寸（使用运行时数据）
	int32 OriginalWidth = Texture->GetSizeX();
	int32 OriginalHeight = Texture->GetSizeY();

	if (OriginalWidth <= 0 || OriginalHeight <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 纹理尺寸无效: %dx%d"), OriginalWidth, OriginalHeight);
		return Points;
	}

	// 检查纹理格式是否支持
	EPixelFormat PixelFormat = PlatformData->PixelFormat;

	if (!ValidateAndLogPlatformTextureFormat(PixelFormat, TEXT("纹理密度采样")))
	{
		return Points;
	}

	// 获取 Mip 0 数据（最高分辨率）
	FTexture2DMipMap& Mip0 = PlatformData->Mips[0];
	const void* RawData = Mip0.BulkData.LockReadOnly();
	if (!RawData)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 无法锁定纹理数据"));
		return Points;
	}

	const int64 PixelCount64 = (int64)OriginalWidth * (int64)OriginalHeight;
	const int64 DataSize64 = (int64)Mip0.BulkData.GetBulkDataSize();
	if (PixelCount64 <= 0 || DataSize64 <= 0 || (DataSize64 % PixelCount64) != 0)
	{
		Mip0.BulkData.Unlock();
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 纹理数据大小异常: Data=%lld, Pixels=%lld"), DataSize64, PixelCount64);
		return Points;
	}

	uint32 BytesPerPixel = (uint32)(DataSize64 / PixelCount64);
	if ((PixelFormat == PF_B8G8R8A8 || PixelFormat == PF_R8G8B8A8 || PixelFormat == PF_A8R8G8B8) && BytesPerPixel != 4)
	{
		Mip0.BulkData.Unlock();
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 8位纹理BytesPerPixel应为4，当前=%u"), BytesPerPixel);
		return Points;
	}
	if (PixelFormat == PF_FloatRGBA && (BytesPerPixel != 8 && BytesPerPixel != 16))
	{
		Mip0.BulkData.Unlock();
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] FloatRGBA纹理BytesPerPixel应为8或16，当前=%u"), BytesPerPixel);
		return Points;
	}

	const uint8* PixelData = static_cast<const uint8*>(RawData);

	// 智能判断使用哪个通道采样
	bool bUseAlphaChannel = ShouldUseAlphaChannel(Texture);
	const TCHAR* ChannelName = bUseAlphaChannel ? TEXT("Alpha") : TEXT("Luminance");

	UE_LOG(LogPointSampling, Log, TEXT("[纹理密度采样] 运行时数据：尺寸=%dx%d, 格式=%d, 压缩=%d, 采样通道=%s"),
		OriginalWidth, OriginalHeight, (int32)PixelFormat,
		(int32)Texture->CompressionSettings, ChannelName);

	// 使用现有泊松圆盘采样生成初始点集
	// 计算采样区域大小
	float Width = OriginalWidth * TextureScale;
	float Height = OriginalHeight * TextureScale;

	// 使用最小半径生成泊松点集（确保最大密度）
	TArray<FVector2D> PoissonPoints = FPoissonDiskSampling::GeneratePoisson2D(Width, Height, MinRadius, MaxAttempts);
	UE_LOG(LogPointSampling, Log, TEXT("[纹理密度采样] 生成初始泊松点集: %d 个点"), PoissonPoints.Num());

	// 遍历泊松点集，根据纹理密度筛选和调整
	for (const FVector2D& PoissonPoint : PoissonPoints)
	{
		// 将泊松点坐标转换为纹理归一化坐标
		FVector2D NormalizedCoords;
		NormalizedCoords.X = FMath::Clamp(PoissonPoint.X / Width, 0.0f, 1.0f);
		NormalizedCoords.Y = FMath::Clamp(PoissonPoint.Y / Height, 0.0f, 1.0f);

		// 获取当前坐标的纹理密度值
		float Density = GetTextureDensityAtCoordinatePlatform(
			Texture,
			NormalizedCoords,
			bUseAlphaChannel,
			PixelFormat,
			PixelData,
			OriginalWidth,
			OriginalHeight,
			BytesPerPixel
		);

		// 如果密度低于阈值，跳过该点
		if (Density < PixelThreshold)
		{
			continue;
		}

		// 将点添加到结果集（局部坐标，居中，Y轴翻转以匹配纹理坐标系）
		const float LocalX = PoissonPoint.X - Width * 0.5f;
		const float LocalY = Height * 0.5f - PoissonPoint.Y;
		Points.Add(FVector(LocalX, LocalY, 0.0f));
	}

	// 解锁纹理数据
	Mip0.BulkData.Unlock();

	UE_LOG(LogPointSampling, Log, TEXT("[纹理密度采样] 从运行时数据生成 %d 个点（尺寸=%dx%d, 格式=%d, 通道=%s）"),
		Points.Num(), OriginalWidth, OriginalHeight, (int32)PixelFormat, ChannelName);

	// 去除重复/重叠点位
	if (Points.Num() > 0)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			MinRadius * 0.1f,  // 容差：最小半径的10%
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log,
				TEXT("[纹理密度采样] 去重：原始 %d 个点 -> 去除 %d 个重复点 -> 剩余 %d 个点"),
				OriginalCount, RemovedCount, Points.Num());
		}
	}

	return Points;
}

// ============================================================================
// 新增：根据通道选择计算像素采样值
// ============================================================================

float FTextureSamplingHelper::CalculatePixelSamplingValueByChannel(const FLinearColor& Color, ETextureSamplingChannel Channel)
{
	switch (Channel)
	{
	case ETextureSamplingChannel::Alpha:
		return Color.A;

	case ETextureSamplingChannel::Red:
		return Color.R;

	case ETextureSamplingChannel::Green:
		return Color.G;

	case ETextureSamplingChannel::Blue:
		return Color.B;

	case ETextureSamplingChannel::Luminance:
		// 感知亮度公式（ITU-R BT.601）
		return 0.299f * Color.R + 0.587f * Color.G + 0.114f * Color.B;

	case ETextureSamplingChannel::Auto:
	default:
		// 自动模式：优先使用Alpha，如果Alpha接近1.0则使用亮度
		if (FMath::Abs(Color.A - 1.0f) > 0.01f)
		{
			return Color.A;
		}
		else
		{
			return 0.299f * Color.R + 0.587f * Color.G + 0.114f * Color.B;
		}
	}
}

// ============================================================================
// Material Instance 采样实现
// ============================================================================

UTextureRenderTarget2D* FTextureSamplingHelper::CreateTemporaryRenderTarget(int32 Size)
{
	// 创建临时RenderTarget（RGBA8格式，不带Mipmap）
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	if (!RenderTarget)
	{
		return nullptr;
	}

	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->InitAutoFormat(Size, Size);
	RenderTarget->UpdateResourceImmediate(true);

	return RenderTarget;
}

bool FTextureSamplingHelper::RenderMaterialToTarget(UMaterialInterface* Material, UTextureRenderTarget2D* RenderTarget)
{
	if (!Material || !RenderTarget)
	{
		return false;
	}

	// 使用KismetRenderingLibrary绘制材质到RenderTarget
	// 注意：使用Material作为WorldContextObject，因为Material->GetWorld()可能返回nullptr
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(
		const_cast<UMaterialInterface*>(Material),
		RenderTarget,
		const_cast<UMaterialInterface*>(Material)
	);

	return true;
}

TArray<FVector> FTextureSamplingHelper::GeneratePointsFromRenderTarget(
	UTextureRenderTarget2D* RenderTarget,
	int32 MaxSampleSize,
	float Spacing,
	float PixelThreshold,
	float TextureScale,
	ETextureSamplingChannel SamplingChannel)
{
	TArray<FVector> Points;

	if (!RenderTarget)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material采样] RenderTarget无效"));
		return Points;
	}

	// 读取RenderTarget像素数据
	TArray<FColor> PixelData;
	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material采样] 无法获取RenderTarget资源"));
		return Points;
	}

	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	ReadFlags.SetLinearToGamma(false);

	if (!RTResource->ReadPixels(PixelData, ReadFlags))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material采样] 无法读取RenderTarget像素"));
		return Points;
	}

	const int32 OriginalWidth = RenderTarget->SizeX;
	const int32 OriginalHeight = RenderTarget->SizeY;

	if (PixelData.Num() != OriginalWidth * OriginalHeight)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[Material采样] 像素数量不匹配: 期望%dx%d=%d, 实际%d"),
			OriginalWidth, OriginalHeight, OriginalWidth * OriginalHeight, PixelData.Num());
		return Points;
	}

	// 计算降采样比率
	const float DownsampleRatio = FMath::Max(1.0f, FMath::Max((float)OriginalWidth / MaxSampleSize, (float)OriginalHeight / MaxSampleSize));
	const int32 SampleWidth = FMath::Max(1, FMath::RoundToInt(OriginalWidth / DownsampleRatio));
	const int32 SampleHeight = FMath::Max(1, FMath::RoundToInt(OriginalHeight / DownsampleRatio));
	const int32 Step = FMath::Max(1, FMath::RoundToInt(Spacing));

	// 限制最大点数
	const int32 EstimatedMaxPoints = (SampleWidth / Step) * (SampleHeight / Step);
	const int32 MaxAllowedPoints = 100000;

	if (EstimatedMaxPoints > MaxAllowedPoints)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material采样] 预期点数 %d 超过限制 %d"), EstimatedMaxPoints, MaxAllowedPoints);
		return Points;
	}

	UE_LOG(LogPointSampling, Log, TEXT("[Material采样] 开始采样 %dx%d -> %dx%d (步长=%d, 阈值=%.2f, 通道=%d)"),
		OriginalWidth, OriginalHeight, SampleWidth, SampleHeight, Step, PixelThreshold, (int32)SamplingChannel);

	Points.Reserve(EstimatedMaxPoints / 4);

	// 遍历采样点
	for (int32 SampleY = 0; SampleY < SampleHeight; SampleY += Step)
	{
		for (int32 SampleX = 0; SampleX < SampleWidth; SampleX += Step)
		{
			// 映射回原始纹理坐标
			int32 OriginalX = FMath::RoundToInt(SampleX * DownsampleRatio);
			int32 OriginalY = FMath::RoundToInt(SampleY * DownsampleRatio);

			OriginalX = FMath::Clamp(OriginalX, 0, OriginalWidth - 1);
			OriginalY = FMath::Clamp(OriginalY, 0, OriginalHeight - 1);

			// 获取像素颜色
			const int32 PixelIndex = OriginalY * OriginalWidth + OriginalX;
			const FColor& PixelColor = PixelData[PixelIndex];
			const FLinearColor LinearColor = FLinearColor(PixelColor);

			// 根据通道选择计算采样值
			float SamplingValue = CalculatePixelSamplingValueByChannel(LinearColor, SamplingChannel);

			// 阈值过滤
			if (SamplingValue >= PixelThreshold)
			{
				// 转换为世界坐标（居中，Y轴翻转）
				const float NormalizedX = (float)OriginalX / (OriginalWidth - 1);
				const float NormalizedY = (float)OriginalY / (OriginalHeight - 1);

				const float WorldX = (NormalizedX - 0.5f) * OriginalWidth * TextureScale;
				const float WorldY = (0.5f - NormalizedY) * OriginalHeight * TextureScale;

				Points.Add(FVector(WorldX, WorldY, 0.0f));
			}
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("[Material采样] 完成，生成 %d 个点"), Points.Num());

	// 去重
	if (Points.Num() > 1)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			TextureScale * 0.1f,
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Verbose, TEXT("[Material采样] 去重: %d -> %d (移除 %d)"),
				OriginalCount, Points.Num(), RemovedCount);
		}
	}

	return Points;
}

TArray<FVector> FTextureSamplingHelper::GeneratePointsFromRenderTargetWithPoisson(
	UTextureRenderTarget2D* RenderTarget,
	int32 MaxSampleSize,
	float MinRadius,
	float MaxRadius,
	float PixelThreshold,
	float TextureScale,
	ETextureSamplingChannel SamplingChannel,
	int32 MaxAttempts)
{
	TArray<FVector> Points;

	if (!RenderTarget)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material泊松采样] RenderTarget无效"));
		return Points;
	}

	// 读取RenderTarget像素数据
	TArray<FColor> PixelData;
	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material泊松采样] 无法获取RenderTarget资源"));
		return Points;
	}

	FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
	ReadFlags.SetLinearToGamma(false);

	if (!RTResource->ReadPixels(PixelData, ReadFlags))
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material泊松采样] 无法读取RenderTarget像素"));
		return Points;
	}

	const int32 OriginalWidth = RenderTarget->SizeX;
	const int32 OriginalHeight = RenderTarget->SizeY;

	// 使用泊松圆盘采样生成初始点集
	float Width = OriginalWidth * TextureScale;
	float Height = OriginalHeight * TextureScale;

	TArray<FVector2D> PoissonPoints = FPoissonDiskSampling::GeneratePoisson2D(Width, Height, MinRadius, MaxAttempts);
	UE_LOG(LogPointSampling, Log, TEXT("[Material泊松采样] 生成初始泊松点集: %d 个点"), PoissonPoints.Num());

	// 遍历泊松点，根据纹理密度筛选
	for (const FVector2D& PoissonPoint : PoissonPoints)
	{
		// 转换为纹理坐标
		FVector2D NormalizedCoords;
		NormalizedCoords.X = FMath::Clamp(PoissonPoint.X / Width, 0.0f, 1.0f);
		NormalizedCoords.Y = FMath::Clamp(PoissonPoint.Y / Height, 0.0f, 1.0f);

		// 获取像素索引
		int32 PixelX = FMath::Clamp(FMath::RoundToInt(NormalizedCoords.X * (OriginalWidth - 1)), 0, OriginalWidth - 1);
		int32 PixelY = FMath::Clamp(FMath::RoundToInt(NormalizedCoords.Y * (OriginalHeight - 1)), 0, OriginalHeight - 1);
		int32 PixelIndex = PixelY * OriginalWidth + PixelX;

		// 获取密度值
		const FColor& PixelColor = PixelData[PixelIndex];
		const FLinearColor LinearColor = FLinearColor(PixelColor);
		float Density = CalculatePixelSamplingValueByChannel(LinearColor, SamplingChannel);

		// 密度过滤
		if (Density < PixelThreshold)
		{
			continue;
		}

		// 添加点（局部坐标，居中，Y轴翻转）
		const float LocalX = PoissonPoint.X - Width * 0.5f;
		const float LocalY = Height * 0.5f - PoissonPoint.Y;
		Points.Add(FVector(LocalX, LocalY, 0.0f));
	}

	UE_LOG(LogPointSampling, Log, TEXT("[Material泊松采样] 筛选后剩余 %d 个点"), Points.Num());

	// 去重
	if (Points.Num() > 0)
	{
		int32 OriginalCount, RemovedCount;
		FPointDeduplicationHelper::RemoveDuplicatePointsWithStats(
			Points,
			MinRadius * 0.1f,
			OriginalCount,
			RemovedCount
		);

		if (RemovedCount > 0)
		{
			UE_LOG(LogPointSampling, Log, TEXT("[Material泊松采样] 去重: %d -> %d (移除 %d)"),
				OriginalCount, RemovedCount, Points.Num());
		}
	}

	return Points;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromMaterial(
	UMaterialInterface* Material,
	int32 MaxSampleSize,
	float Spacing,
	float PixelThreshold,
	float TextureScale,
	ETextureSamplingChannel SamplingChannel)
{
	TArray<FVector> Points;

	if (!Material)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material采样] 材质无效"));
		return Points;
	}

	// 创建临时RenderTarget
	UTextureRenderTarget2D* RenderTarget = CreateTemporaryRenderTarget(MaxSampleSize);
	if (!RenderTarget)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[Material采样] 无法创建RenderTarget"));
		return Points;
	}

	// 渲染材质到RenderTarget
	if (!RenderMaterialToTarget(Material, RenderTarget))
	{
		UE_LOG(LogPointSampling, Error, TEXT("[Material采样] 材质渲染失败"));
		return Points;
	}

	// 从RenderTarget生成点阵
	Points = GeneratePointsFromRenderTarget(RenderTarget, MaxSampleSize, Spacing, PixelThreshold, TextureScale, SamplingChannel);

	// 清理临时RenderTarget
	RenderTarget->ConditionalBeginDestroy();

	return Points;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromMaterialWithPoisson(
	UMaterialInterface* Material,
	int32 MaxSampleSize,
	float MinRadius,
	float MaxRadius,
	float PixelThreshold,
	float TextureScale,
	ETextureSamplingChannel SamplingChannel,
	int32 MaxAttempts)
{
	TArray<FVector> Points;

	if (!Material)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[Material泊松采样] 材质无效"));
		return Points;
	}

	// 创建临时RenderTarget
	UTextureRenderTarget2D* RenderTarget = CreateTemporaryRenderTarget(MaxSampleSize);
	if (!RenderTarget)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[Material泊松采样] 无法创建RenderTarget"));
		return Points;
	}

	// 渲染材质到RenderTarget
	if (!RenderMaterialToTarget(Material, RenderTarget))
	{
		UE_LOG(LogPointSampling, Error, TEXT("[Material泊松采样] 材质渲染失败"));
		return Points;
	}

	// 从RenderTarget生成泊松采样点阵
	Points = GeneratePointsFromRenderTargetWithPoisson(
		RenderTarget,
		MaxSampleSize,
		MinRadius,
		MaxRadius,
		PixelThreshold,
		TextureScale,
		SamplingChannel,
		MaxAttempts
	);

	// 清理临时RenderTarget
	RenderTarget->ConditionalBeginDestroy();

	return Points;
}

// ============================================================================
// 智能统一接口实现
// ============================================================================

bool FTextureSamplingHelper::IsTextureFormatDirectReadable(UTexture2D* Texture)
{
	if (!Texture)
	{
		return false;
	}

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		return false;
	}

	EPixelFormat PixelFormat = PlatformData->PixelFormat;

	// 只有未压缩的8位格式可以直接读取
	bool bIsDirectReadable = (PixelFormat == PF_B8G8R8A8) ||
							 (PixelFormat == PF_R8G8B8A8) ||
							 (PixelFormat == PF_A8R8G8B8) ||
							 (PixelFormat == PF_FloatRGBA);

	return bIsDirectReadable;
}

bool FTextureSamplingHelper::ValidateAndLogPlatformTextureFormat(EPixelFormat PixelFormat, const TCHAR* FunctionName)
{
	// 支持的格式：BGRA8, RGBA8, FloatRGBA
	bool bIsSupportedFormat = (PixelFormat == PF_B8G8R8A8) ||
		(PixelFormat == PF_R8G8B8A8) ||
		(PixelFormat == PF_A8R8G8B8) ||
		(PixelFormat == PF_FloatRGBA);

	if (!bIsSupportedFormat)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[%s] 纹理格式不支持！当前格式: %d (%s)"),
			FunctionName,
			(int32)PixelFormat,
			GetPixelFormatString(PixelFormat));

		UE_LOG(LogPointSampling, Error, TEXT("[%s] 请在纹理设置中修改："), FunctionName);
		UE_LOG(LogPointSampling, Error, TEXT("  1. Compression Settings -> VectorDisplacementmap (RGBA8)"));
		UE_LOG(LogPointSampling, Error, TEXT("  2. Mip Gen Settings -> NoMipmaps"));
		UE_LOG(LogPointSampling, Error, TEXT("  3. sRGB -> 取消勾选"));
		UE_LOG(LogPointSampling, Error, TEXT("  4. 点击 'Save' 保存并重新导入纹理"));
	}

	return bIsSupportedFormat;
}

UMaterialInstanceDynamic* FTextureSamplingHelper::CreateTemporaryMaterialForTexture(UTexture2D* Texture, UWorld* World)
{
	if (!Texture || !World)
	{
		return nullptr;
	}

	// 创建简单的材质：Texture Sample -> Emissive Color
	// 注意：这里我们使用运行时创建的材质实例
	// 需要一个基础材质作为模板

	// 方案1：尝试使用引擎内置的简单材质
	UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	if (!BaseMaterial)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[智能采样] 无法加载基础材质"));
		return nullptr;
	}

	// 创建动态材质实例
	UMaterialInstanceDynamic* MatInst = UMaterialInstanceDynamic::Create(Cast<UMaterialInterface>(BaseMaterial), World);
	if (!MatInst)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[智能采样] 无法创建材质实例"));
		return nullptr;
	}

	// 设置纹理参数（如果基础材质支持）
	MatInst->SetTextureParameterValue(FName("Texture"), Texture);

	return MatInst;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromTextureAuto(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float Spacing,
	float PixelThreshold,
	float TextureScale,
	ETextureSamplingChannel SamplingChannel)
{
	TArray<FVector> Points;

	if (!Texture)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[智能采样] 纹理无效"));
		return Points;
	}

	// 检查是否可以直接读取
	bool bCanDirectRead = IsTextureFormatDirectReadable(Texture);

	if (bCanDirectRead)
	{
		UE_LOG(LogPointSampling, Log, TEXT("[智能采样] 检测到未压缩格式，使用直接读取方法"));

		// 使用原有的高效方法
#if WITH_EDITOR
		Points = GenerateFromTextureSource(Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale);
#else
		Points = GenerateFromTexturePlatformData(Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale);
#endif
	}
	else
	{
		// 压缩格式处理：仅在编辑器中支持Canvas渲染方式
#if WITH_EDITOR
		UE_LOG(LogPointSampling, Log, TEXT("[智能采样] 检测到压缩格式，使用Canvas渲染方法"));

		UTextureRenderTarget2D* RenderTarget = CreateTemporaryRenderTarget(MaxSampleSize);
		if (!RenderTarget)
		{
			UE_LOG(LogPointSampling, Error, TEXT("[智能采样] 无法创建RenderTarget"));
			return Points;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(Texture, EGetWorldErrorMode::LogAndReturnNull);
		if (!World)
		{
			UObject* Outer = Texture->GetOuter();
			while (Outer && !World)
			{
				World = Outer->GetWorld();
				Outer = Outer->GetOuter();
			}
		}

		if (World)
		{
			FCanvas Canvas(
				RenderTarget->GameThread_GetRenderTargetResource(),
				nullptr,
				World,
				World->GetFeatureLevel()
			);
			Canvas.Clear(FLinearColor::Black);

			FCanvasTileItem TileItem(
				FVector2D(0, 0),
				Texture->GetResource(),
				FVector2D(RenderTarget->SizeX, RenderTarget->SizeY),
				FLinearColor::White
			);
			TileItem.BlendMode = SE_BLEND_Opaque;
			Canvas.DrawItem(TileItem);
			Canvas.Flush_GameThread();
			RenderTarget->UpdateResourceImmediate(false);

			Points = GeneratePointsFromRenderTarget(RenderTarget, MaxSampleSize, Spacing, PixelThreshold, TextureScale, SamplingChannel);
		}
		else
		{
			Points = GenerateFromTextureSource(Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale);
		}
		RenderTarget->ConditionalBeginDestroy();
#else
		// 运行时不支持压缩格式的Canvas渲染
		UE_LOG(LogPointSampling, Warning, TEXT("[智能采样] 运行时不支持压缩纹理格式，请使用未压缩格式(BGRA8/RGBA8)"));
#endif
	}

	return Points;
}

TArray<FVector> FTextureSamplingHelper::GenerateFromTextureAutoWithPoisson(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float MinRadius,
	float MaxRadius,
	float PixelThreshold,
	float TextureScale,
	ETextureSamplingChannel SamplingChannel,
	int32 MaxAttempts)
{
	TArray<FVector> Points;

	if (!Texture)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[智能泊松采样] 纹理无效"));
		return Points;
	}

	// 检查是否可以直接读取
	bool bCanDirectRead = IsTextureFormatDirectReadable(Texture);

	if (bCanDirectRead)
	{
		UE_LOG(LogPointSampling, Log, TEXT("[智能泊松采样] 检测到未压缩格式，使用直接读取方法"));

		// 使用原有的高效方法
#if WITH_EDITOR
		Points = GenerateFromTextureSourceWithPoisson(Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold, TextureScale, MaxAttempts);
#else
		Points = GenerateFromTexturePlatformDataWithPoisson(Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold, TextureScale, MaxAttempts);
#endif
	}
	else
	{
		// 压缩格式处理：仅在编辑器中支持Canvas渲染方式
#if WITH_EDITOR
		UE_LOG(LogPointSampling, Log, TEXT("[智能泊松采样] 检测到压缩格式，使用Canvas渲染方法"));

		UTextureRenderTarget2D* RenderTarget = CreateTemporaryRenderTarget(MaxSampleSize);
		if (!RenderTarget)
		{
			UE_LOG(LogPointSampling, Error, TEXT("[智能泊松采样] 无法创建RenderTarget"));
			return Points;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(Texture, EGetWorldErrorMode::LogAndReturnNull);
		if (!World)
		{
			UObject* Outer = Texture->GetOuter();
			while (Outer && !World)
			{
				World = Outer->GetWorld();
				Outer = Outer->GetOuter();
			}
		}

		if (World)
		{
			FCanvas Canvas(
				RenderTarget->GameThread_GetRenderTargetResource(),
				nullptr,
				World,
				World->GetFeatureLevel()
			);
			Canvas.Clear(FLinearColor::Black);

			FCanvasTileItem TileItem(
				FVector2D(0, 0),
				Texture->GetResource(),
				FVector2D(RenderTarget->SizeX, RenderTarget->SizeY),
				FLinearColor::White
			);
			TileItem.BlendMode = SE_BLEND_Opaque;
			Canvas.DrawItem(TileItem);
			Canvas.Flush_GameThread();
			RenderTarget->UpdateResourceImmediate(false);

			Points = GeneratePointsFromRenderTargetWithPoisson(
				RenderTarget, MaxSampleSize, MinRadius, MaxRadius,
				PixelThreshold, TextureScale, SamplingChannel, MaxAttempts
			);
		}
		else
		{
			Points = GenerateFromTextureSourceWithPoisson(Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold, TextureScale, MaxAttempts);
		}
		RenderTarget->ConditionalBeginDestroy();
#else
		// 运行时不支持压缩格式的Canvas渲染
		UE_LOG(LogPointSampling, Warning, TEXT("[智能泊松采样] 运行时不支持压缩纹理格式，请使用未压缩格式(BGRA8/RGBA8)"));
#endif
	}

	return Points;
}
