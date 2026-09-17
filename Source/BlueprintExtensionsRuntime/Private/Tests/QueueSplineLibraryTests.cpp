/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Libraries/QueueSplineLibrary.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQueueSplineLibrary_RejectsNonFiniteGeometry,
	"XTools.BlueprintExtensionsRuntime.QueueSpline.RejectsNonFiniteGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQueueSplineLibrary_RejectsNonFiniteGeometry::RunTest(const FString& Parameters)
{
	USplineComponent* Spline = NewObject<USplineComponent>(GetTransientPackage());
	if (!TestNotNull(TEXT("应创建瞬态样条组件"), Spline))
	{
		return false;
	}
	Spline->SetSplinePoints({ FVector::ZeroVector, FVector(1000.0, 0.0, 0.0) }, ESplineCoordinateSpace::Local);
	TestTrue(TEXT("测试样条应具有有效长度"), Spline->GetSplineLength() > 0.0f);

	FXToolsQueueSplineConfig ValidConfig;
	ValidConfig.UnitCount = 3;
	ValidConfig.DistanceJitter = 0.0;
	ValidConfig.SideJitter = 0.0;
	FString Message;
	TestTrue(TEXT("有限配置应通过校验"), UQueueSplineLibrary::IsQueueSplineConfigValid(Spline, ValidConfig, Message));
	FXToolsQueueSplineSlot Slot;
	TestTrue(TEXT("有限配置应生成队头"), UQueueSplineLibrary::CalculateQueueSplineSlot(Spline, 0, ValidConfig, Slot));
	TestEqual(TEXT("队头应向右偏移"), Slot.RightOffset, ValidConfig.SideOffset);
	TestEqual(TEXT("有限配置应生成全部槽位"), UQueueSplineLibrary::GenerateQueueSplineSlots(Spline, ValidConfig).Num(), 3);

	struct FGeometryField
	{
		const TCHAR* Name;
		double FXToolsQueueSplineConfig::* Member;
	};
	const FGeometryField Fields[] = {
		{ TEXT("StartDistance"), &FXToolsQueueSplineConfig::StartDistance },
		{ TEXT("Spacing"), &FXToolsQueueSplineConfig::Spacing },
		{ TEXT("FillRatio"), &FXToolsQueueSplineConfig::FillRatio },
		{ TEXT("SideOffset"), &FXToolsQueueSplineConfig::SideOffset },
		{ TEXT("DistanceJitter"), &FXToolsQueueSplineConfig::DistanceJitter },
		{ TEXT("SideJitter"), &FXToolsQueueSplineConfig::SideJitter }
	};
	const double InvalidValues[] = {
		std::numeric_limits<double>::quiet_NaN(),
		std::numeric_limits<double>::infinity(),
		-std::numeric_limits<double>::infinity()
	};
	for (const FGeometryField& Field : Fields)
	{
		for (int32 ValueIndex = 0; ValueIndex < UE_ARRAY_COUNT(InvalidValues); ++ValueIndex)
		{
			FXToolsQueueSplineConfig InvalidConfig = ValidConfig;
			InvalidConfig.*(Field.Member) = InvalidValues[ValueIndex];
			const FString Context = FString::Printf(TEXT("%s 非有限值 %d"), Field.Name, ValueIndex);
			TestFalse(Context + TEXT("应校验失败"), UQueueSplineLibrary::IsQueueSplineConfigValid(Spline, InvalidConfig, Message));
			TestFalse(Context + TEXT("应提供原因"), Message.IsEmpty());
			// 先填充所有输出字段，确保失败路径真正重置而非保留旧槽位。
			Slot.Index = 9;
			Slot.Distance = 12.0;
			Slot.RightOffset = 34.0;
			Slot.CenterLocation = FVector(1.0, 2.0, 3.0);
			Slot.TargetLocation = FVector(4.0, 5.0, 6.0);
			Slot.TargetRotation = FRotator(10.0, 20.0, 30.0);
			TestFalse(Context + TEXT("应拒绝单槽位"), UQueueSplineLibrary::CalculateQueueSplineSlot(Spline, 0, InvalidConfig, Slot));
			TestEqual(Context + TEXT("索引应重置"), Slot.Index, INDEX_NONE);
			TestEqual(Context + TEXT("距离应重置"), Slot.Distance, 0.0);
			TestEqual(Context + TEXT("偏移应重置"), Slot.RightOffset, 0.0);
			TestTrue(Context + TEXT("中心应重置"), Slot.CenterLocation.IsZero());
			TestTrue(Context + TEXT("目标应重置"), Slot.TargetLocation.IsZero());
			TestTrue(Context + TEXT("旋转应重置"), Slot.TargetRotation.IsZero());
			TestEqual(Context + TEXT("批量应为空"), UQueueSplineLibrary::GenerateQueueSplineSlots(Spline, InvalidConfig).Num(), 0);
		}
	}

	// 有限负值继续沿用现有 Clamp 行为，不能被有限性检查拒绝。
	FXToolsQueueSplineConfig NegativeConfig = ValidConfig;
	NegativeConfig.StartDistance = -100.0;
	NegativeConfig.FillRatio = -1.0;
	NegativeConfig.SideOffset = -35.0;
	NegativeConfig.DistanceJitter = -15.0;
	NegativeConfig.SideJitter = -10.0;
	TestTrue(TEXT("有限负值配置保留既有校验语义"), UQueueSplineLibrary::IsQueueSplineConfigValid(Spline, NegativeConfig, Message));
	TestTrue(TEXT("有限负值应继续生成槽位"), UQueueSplineLibrary::CalculateQueueSplineSlot(Spline, 0, NegativeConfig, Slot));
	TestEqual(TEXT("负起始距离应钳制到零并计算队头"), Slot.Distance, 200.0);
	TestEqual(TEXT("负偏移和抖动应钳制到零"), Slot.RightOffset, 0.0);
	NegativeConfig.FillMode = EXToolsQueueSplineFillMode::PreFilledRatio;
	TestTrue(TEXT("负填充比例应继续生成槽位"), UQueueSplineLibrary::CalculateQueueSplineSlot(Spline, 0, NegativeConfig, Slot));
	TestEqual(TEXT("负填充比例应钳制到零"), Slot.Distance, 0.0);
	NegativeConfig.FillMode = EXToolsQueueSplineFillMode::FromStart;
	NegativeConfig.Spacing = -100.0;
	TestFalse(TEXT("负间距继续不通过显式校验"), UQueueSplineLibrary::IsQueueSplineConfigValid(Spline, NegativeConfig, Message));
	TestTrue(TEXT("生成时负间距继续钳制到一"), UQueueSplineLibrary::CalculateQueueSplineSlot(Spline, 0, NegativeConfig, Slot));
	TestEqual(TEXT("生成时使用钳制后的间距"), Slot.Distance, 2.0);
	TestEqual(TEXT("有限负值继续支持批量生成"), UQueueSplineLibrary::GenerateQueueSplineSlots(Spline, NegativeConfig).Num(), 3);
	return true;
}

#endif
