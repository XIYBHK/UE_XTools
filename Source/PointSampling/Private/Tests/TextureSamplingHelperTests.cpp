/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Sampling/TextureSamplingHelper.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTextureSamplingHelper_ReadsLastG8Pixel,
	"XTools.PointSampling.Texture.ReadsLastG8Pixel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTextureSamplingHelper_ReadsLastG8Pixel::RunTest(const FString& Parameters)
{
	const uint8 SourceData[] = { 0, 255 };
	const float Density = FTextureSamplingHelper::GetTextureDensityAtCoordinate(
		nullptr,
		FVector2D(1.0, 0.0),
		false,
		TSF_G8,
		SourceData,
		2,
		1,
		1);

	TestEqual(TEXT("G8纹理的最后一个像素应使用单字节边界读取"), Density, 1.0f);
	return true;
}

#endif
