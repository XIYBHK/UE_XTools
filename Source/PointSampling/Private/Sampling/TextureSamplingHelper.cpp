/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "TextureSamplingHelper.h"
#include "Engine/Texture2D.h"
#include "Math/UnrealMathUtility.h"
#include "TextureResource.h"

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
	if (PixelFormat != PF_B8G8R8A8 && PixelFormat != PF_R8G8B8A8)
	{
		UE_LOG(LogPointSampling, Warning, TEXT("[纹理采样] 不支持的纹理格式，请使用 RGBA8 格式（当前格式: %d）"), (int32)PixelFormat);
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

			// 计算采样值（使用 Alpha 通道）
			float SamplingValue = CalculatePixelSamplingValue(PixelColor, true);

			// 如果采样值高于阈值，创建点位
			if (SamplingValue >= PixelThreshold)
			{
				// 将像素坐标转换为局部坐标（居中，保持原始纵横比）
				float NormalizedX = (SampleX / (float)SampleWidth) - 0.5f;
				float NormalizedY = (SampleY / (float)SampleHeight) - 0.5f;

				FVector Point(
					NormalizedX * SampleWidth * TextureScale,
					NormalizedY * SampleHeight * TextureScale,
					0.0f
				);

				Points.Add(Point);
			}
		}
	}

	// 解锁纹理数据
	Mip0.BulkData.Unlock();

	UE_LOG(LogPointSampling, Log, TEXT("[纹理采样] 从纹理(%dx%d, 格式=%d)降采样到(%dx%d), Spacing=%d, 生成了 %d 个点"),
		OriginalWidth, OriginalHeight, (int32)PixelFormat, SampleWidth, SampleHeight, Step, Points.Num());

	return Points;
}

float FTextureSamplingHelper::CalculatePixelSamplingValue(const FColor& Color, bool bHasAlphaChannel)
{
	// 如果纹理有 Alpha 通道，优先使用 Alpha 值（用于透明度采样）
	if (bHasAlphaChannel)
	{
		return Color.A / 255.0f;
	}
	else
	{
		// 无 Alpha 通道时，使用感知亮度公式：Luminance = 0.299*R + 0.587*G + 0.114*B
		float Luminance = (0.299f * Color.R + 0.587f * Color.G + 0.114f * Color.B) / 255.0f;
		return Luminance;
	}
}
