/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "TextureSamplingHelper.h"
#include "Engine/Texture2D.h"
#include "Math/UnrealMathUtility.h"

TArray<FVector> FTextureSamplingHelper::GenerateFromTexture(
	UTexture2D* Texture,
	float PixelThreshold,
	float TextureScale)
{
	TArray<FVector> Points;

	if (!Texture || !Texture->GetPlatformData())
	{
		return Points;
	}

	// 获取纹理数据
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (PlatformData->Mips.Num() == 0)
	{
		return Points;
	}

	// 使用第一个 Mip 层级（最高分辨率）
	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	int32 Width = Mip.SizeX;
	int32 Height = Mip.SizeY;

	// 锁定纹理数据进行读取
	const FColor* TextureData = reinterpret_cast<const FColor*>(Mip.BulkData.LockReadOnly());
	if (!TextureData)
	{
		return Points;
	}

	// 预估点数（用于预分配）
	Points.Reserve(Width * Height / 4); // 假设大约 25% 的像素会被采样

	// 遍历所有像素
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			int32 PixelIndex = Y * Width + X;
			const FColor& PixelColor = TextureData[PixelIndex];

			// 计算采样值
			float SamplingValue = CalculatePixelSamplingValue(PixelColor);

			// 如果采样值高于阈值，创建点位
			if (SamplingValue >= PixelThreshold)
			{
				// 将像素坐标转换为世界坐标
				// X: 0 ~ Width  -> -Width/2 ~ Width/2
				// Y: 0 ~ Height -> -Height/2 ~ Height/2
				FVector Point(
					(X - Width * 0.5f) * TextureScale,
					(Y - Height * 0.5f) * TextureScale,
					0.0f
				);

				Points.Add(Point);
			}
		}
	}

	// 解锁纹理数据
	Mip.BulkData.Unlock();

	return Points;
}

float FTextureSamplingHelper::CalculatePixelSamplingValue(const FColor& Color)
{
	// 使用 Alpha 通道作为采样值（如果 Alpha 接近 0，则使用亮度）
	if (Color.A > 10) // Alpha 有效
	{
		return Color.A / 255.0f;
	}
	else
	{
		// 使用感知亮度公式：Luminance = 0.299*R + 0.587*G + 0.114*B
		float Luminance = (0.299f * Color.R + 0.587f * Color.G + 0.114f * Color.B) / 255.0f;
		return Luminance;
	}
}
