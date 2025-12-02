/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "TextureSamplingHelper.h"
#include "PointDeduplicationHelper.h"
#include "PointSamplingTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "Math/UnrealMathUtility.h"
#include "TextureResource.h"
#include "PixelFormat.h"
#include "Algorithms/PoissonDiskSampling.h"

// ============================================================================
// 辅助函数（参考 UE SubUVAnimation.cpp 的标准实现）
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

					// 判断阈值：标准差小于10说明Alpha通道几乎是常数（例如全255或全0）
					const float MinStdDevThreshold = 10.0f;

					if (StdDev < MinStdDevThreshold)
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
	TArray<FVector> Points;

	// 验证纹理有效性
	if (!Texture)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 纹理指针为空"));
		return Points;
	}

#if WITH_EDITOR
	// 编辑器模式：优先使用 Source 数据（支持所有压缩格式）
	if (Texture->Source.IsValid())
	{
		UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 使用编辑器源数据（支持所有格式）"));
		return GenerateFromTextureSource(Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale);
	}
	else
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 编辑器源数据无效，尝试使用运行时数据"));
	}
#endif

	// 运行时模式或编辑器源数据不可用：使用 PlatformData（仅支持未压缩格式）
	return GenerateFromTexturePlatformData(Texture, MaxSampleSize, Spacing, PixelThreshold, TextureScale);
}

// ============================================================================
// 基于泊松圆盘采样的纹理密度采样（改进算法）
// ============================================================================
TArray<FVector> FTextureSamplingHelper::GenerateFromTextureWithPoisson(
	UTexture2D* Texture,
	int32 MaxSampleSize,
	float MinRadius,
	float MaxRadius,
	float PixelThreshold,
	float TextureScale,
	int32 MaxAttempts)
{
	TArray<FVector> Points;

	// 验证纹理有效性
	if (!Texture)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 纹理指针为空"));
		return Points;
	}

	// 验证参数
	if (MinRadius <= 0.0f || MaxRadius < MinRadius || MaxAttempts <= 0)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 参数无效: MinRadius=%.2f, MaxRadius=%.2f, MaxAttempts=%d"), MinRadius, MaxRadius, MaxAttempts);
		return Points;
	}

#if WITH_EDITOR
	// 编辑器模式：优先使用 Source 数据（支持所有压缩格式）
	if (Texture->Source.IsValid())
	{
		UE_LOG(LogPointSampling, Log, TEXT("[纹理密度采样] 使用编辑器源数据（支持所有格式）"));
		return GenerateFromTextureSourceWithPoisson(Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold, TextureScale, MaxAttempts);
	}
	else
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理密度采样] 编辑器源数据无效，尝试使用运行时数据"));
	}
#endif

	// 运行时模式或编辑器源数据不可用：使用 PlatformData（仅支持未压缩格式）
	return GenerateFromTexturePlatformDataWithPoisson(Texture, MaxSampleSize, MinRadius, MaxRadius, PixelThreshold, TextureScale, MaxAttempts);
}

#if WITH_EDITOR
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
	float DownsampleRatio = 1.0f;
	if (OriginalWidth > MaxSampleSize || OriginalHeight > MaxSampleSize)
	{
		DownsampleRatio = FMath::Max((float)OriginalWidth / MaxSampleSize, (float)OriginalHeight / MaxSampleSize);
	}

	int32 SampleWidth = FMath::Max(1, FMath::RoundToInt(OriginalWidth / DownsampleRatio));
	int32 SampleHeight = FMath::Max(1, FMath::RoundToInt(OriginalHeight / DownsampleRatio));
	int32 Step = FMath::Max(1, FMath::RoundToInt(Spacing));

	// 限制最大点数
	int32 EstimatedMaxPoints = (SampleWidth / Step) * (SampleHeight / Step);
	const int32 MaxAllowedPoints = 50000;

	if (EstimatedMaxPoints > MaxAllowedPoints)
	{
		int32 RequiredStep = FMath::CeilToInt(FMath::Sqrt((float)(SampleWidth * SampleHeight) / MaxAllowedPoints));
		Step = FMath::Max(Step, RequiredStep);
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 预期点数 %d 超过限制，调整 Spacing 到 %d"), EstimatedMaxPoints, Step);
	}

	Points.Reserve((SampleWidth / Step) * (SampleHeight / Step) / 2);

	// 获取每像素字节数
	uint32 BytesPerPixel = GetBytesPerPixel(SourceFormat);

	// 遍历采样点
	for (int32 SampleY = 0; SampleY < SampleHeight; SampleY += Step)
	{
		for (int32 SampleX = 0; SampleX < SampleWidth; SampleX += Step)
		{
			int32 OriginalX = FMath::RoundToInt(SampleX * DownsampleRatio);
			int32 OriginalY = FMath::RoundToInt(SampleY * DownsampleRatio);

			if (OriginalX >= OriginalWidth || OriginalY >= OriginalHeight)
				continue;

			int64 PixelIndex = ((int64)OriginalY * OriginalWidth + OriginalX) * BytesPerPixel;
			if (PixelIndex + BytesPerPixel > RawData.Num())
				continue;

			// 解析像素值
			float SamplingValue = 0.0f;

			if (SourceFormat == TSF_G8)
			{
				// 灰度 8 位：直接使用灰度值
				SamplingValue = RawData[PixelIndex] / 255.0f;
			}
			else if (SourceFormat == TSF_BGRA8)
			{
				// BGRA8 格式（通道顺序：B, G, R, A）
				uint8 B = RawData[PixelIndex + 0];
				uint8 G = RawData[PixelIndex + 1];
				uint8 R = RawData[PixelIndex + 2];
				uint8 A = RawData[PixelIndex + 3];
				FColor Color(R, G, B, A);
				SamplingValue = CalculatePixelSamplingValue(Color, bUseAlphaChannel);
			}
			else
			{
				// TSF_RGBA16 和 TSF_RGBA16F 暂不支持（需要额外的浮点转换，不常用）
				continue;
			}

			// 阈值过滤
			if (SamplingValue >= PixelThreshold)
			{
				// 坐标转换（Y 轴翻转）
				float NormalizedX = (OriginalX / (float)OriginalWidth) - 0.5f;
				float NormalizedY = 0.5f - (OriginalY / (float)OriginalHeight);

				FVector Point(
					NormalizedX * OriginalWidth * TextureScale,
					NormalizedY * OriginalHeight * TextureScale,
					0.0f
				);

				Points.Add(Point);
			}
		}
	}

	UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 从源纹理生成 %d 个点（尺寸=%dx%d, 格式=%d, 通道=%s）"),
		Points.Num(), OriginalWidth, OriginalHeight, (int32)SourceFormat, ChannelName);

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

// 基于纹理密度的泊松圆盘采样（编辑器源数据版本）
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
		NormalizedCoords.X = (PoissonPoint.X / Width) + 0.5f; // 从 [-Width/2, Width/2] 转换到 [0, 1]
		NormalizedCoords.Y = 0.5f - (PoissonPoint.Y / Height); // 从 [-Height/2, Height/2] 转换到 [0, 1] 并翻转Y轴

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

		// 将点添加到结果集
		Points.Add(FVector(PoissonPoint.X, PoissonPoint.Y, 0.0f));
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

	// 支持的格式：BGRA8, RGBA8, FloatRGBA
	bool bIsSupportedFormat = (PixelFormat == PF_B8G8R8A8) ||
	                          (PixelFormat == PF_R8G8B8A8) ||
	                          (PixelFormat == PF_A8R8G8B8) ||
	                          (PixelFormat == PF_FloatRGBA);

	if (!bIsSupportedFormat)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理采样] 纹理格式不支持！当前格式: %d (%s)"),
			(int32)PixelFormat,
			GetPixelFormatString(PixelFormat));

		UE_LOG(LogPointSampling, Error, TEXT("[纹理采样] 请在纹理设置中修改："));
		UE_LOG(LogPointSampling, Error, TEXT("  1. Compression Settings -> VectorDisplacementmap (RGBA8)"));
		UE_LOG(LogPointSampling, Error, TEXT("  2. Mip Gen Settings -> NoMipmaps"));
		UE_LOG(LogPointSampling, Error, TEXT("  3. sRGB -> 取消勾选"));
		UE_LOG(LogPointSampling, Error, TEXT("  4. 点击 'Save' 保存并重新导入纹理"));

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

	int32 BytesPerPixel = 4; // BGRA8 或 RGBA8 格式
	int64 DataSize = Mip0.BulkData.GetBulkDataSize();
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
	const int32 MaxAllowedPoints = 50000; // 最大允许 5 万个点

	if (EstimatedMaxPoints > MaxAllowedPoints)
	{
		// 自动调整 Step 以限制点数
		int32 RequiredStep = FMath::CeilToInt(FMath::Sqrt((float)(SampleWidth * SampleHeight) / MaxAllowedPoints));
		Step = FMath::Max(Step, RequiredStep);

		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 预期点数 %d 超过限制 %d，自动调整 Spacing 从 %.1f 到 %d"),
			EstimatedMaxPoints, MaxAllowedPoints, Spacing, Step);

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

			// 读取像素（BGRA8 格式）
			int64 PixelIndex = ((int64)OriginalY * OriginalWidth + OriginalX) * BytesPerPixel;
			if (PixelIndex + BytesPerPixel > DataSize)
			{
				continue;
			}

			// 解析 BGRA8 格式像素（注意通道顺序）
			uint8 B = PixelData[PixelIndex + 0];
			uint8 G = PixelData[PixelIndex + 1];
			uint8 R = PixelData[PixelIndex + 2];
			uint8 A = PixelData[PixelIndex + 3];

			FColor PixelColor(R, G, B, A);

			// 计算采样值（智能选择 Alpha 或亮度）
			float SamplingValue = CalculatePixelSamplingValue(PixelColor, bUseAlphaChannel);

			// 如果采样值高于阈值，创建点位
			if (SamplingValue >= PixelThreshold)
			{
				// 将像素坐标转换为局部坐标（居中，Y 轴翻转以匹配纹理坐标系）
				// 纹理坐标系：左上角为 (0,0)，右下角为 (width, height)
				// 世界坐标系：居中于原点，X 向右，Y 向前（需翻转纹理 Y 轴）
				float NormalizedX = (OriginalX / (float)OriginalWidth) - 0.5f;  // [-0.5, 0.5]
				float NormalizedY = 0.5f - (OriginalY / (float)OriginalHeight); // Y 轴翻转

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

// ============================================================================
// 辅助函数实现
// ============================================================================

// 根据纹理密度计算采样半径（非编辑器专用）
float FTextureSamplingHelper::CalculateRadiusFromDensity(float Density, float MinRadius, float MaxRadius)
{
	// 确保密度在有效范围内
	Density = FMath::Clamp(Density, 0.0f, 1.0f);
	
	// 密度越高，半径越小（采样越密集）
	// 使用平滑的曲线映射，确保过渡自然
	float Radius = MaxRadius - (Density * (MaxRadius - MinRadius));
	
	return FMath::Max(MinRadius, Radius);
}

#if WITH_EDITOR
// 获取指定坐标的纹理密度值（源数据版本）
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
#endif // WITH_EDITOR

// 获取指定坐标的纹理密度值（平台数据版本，运行时可用）
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
		// FloatRGBA 格式（浮点/通道）
		const float* FloatData = reinterpret_cast<const float*>(PixelData + PixelIndex);
		float R = FloatData[0];
		float G = FloatData[1];
		float B = FloatData[2];
		float A = FloatData[3];
		
		FColor Color(FMath::RoundToInt(R * 255.0f), FMath::RoundToInt(G * 255.0f), FMath::RoundToInt(B * 255.0f), FMath::RoundToInt(A * 255.0f));
		SamplingValue = CalculatePixelSamplingValue(Color, bUseAlphaChannel);
	}
	
	return SamplingValue;
}

// 基于纹理密度的泊松圆盘采样（运行时平台数据版本）
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

	// 支持的格式：BGRA8, RGBA8, FloatRGBA
	bool bIsSupportedFormat = (PixelFormat == PF_B8G8R8A8) ||
		                      (PixelFormat == PF_R8G8B8A8) ||
		                      (PixelFormat == PF_A8R8G8B8) ||
		                      (PixelFormat == PF_FloatRGBA);

	if (!bIsSupportedFormat)
	{
		UE_LOG(LogPointSampling, Error, TEXT("[纹理密度采样] 纹理格式不支持！当前格式: %d (%s)"),
			(int32)PixelFormat,
			GetPixelFormatString(PixelFormat));

		UE_LOG(LogPointSampling, Error, TEXT("[纹理密度采样] 请在纹理设置中修改："));
		UE_LOG(LogPointSampling, Error, TEXT("  1. Compression Settings -> VectorDisplacementmap (RGBA8)"));
		UE_LOG(LogPointSampling, Error, TEXT("  2. Mip Gen Settings -> NoMipmaps"));
		UE_LOG(LogPointSampling, Error, TEXT("  3. sRGB -> 取消勾选"));
		UE_LOG(LogPointSampling, Error, TEXT("  4. 点击 'Save' 保存并重新导入纹理"));

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

	uint32 BytesPerPixel = 4; // BGRA8, RGBA8 或 FloatRGBA 格式
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
		NormalizedCoords.X = (PoissonPoint.X / Width) + 0.5f; // 从 [-Width/2, Width/2] 转换到 [0, 1]
		NormalizedCoords.Y = 0.5f - (PoissonPoint.Y / Height); // 从 [-Height/2, Height/2] 转换到 [0, 1] 并翻转Y轴

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

		// 将点添加到结果集
		Points.Add(FVector(PoissonPoint.X, PoissonPoint.Y, 0.0f));
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
