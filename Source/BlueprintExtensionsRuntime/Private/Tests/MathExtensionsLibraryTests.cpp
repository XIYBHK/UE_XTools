#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Libraries/MathExtensionsLibrary.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMathExtensionsLibrary_ApplyScaleCurveAmplitude_UsesNeutralScale,
	"XTools.BlueprintExtensionsRuntime.Math.ApplyScaleCurveAmplitude.UsesNeutralScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMathExtensionsLibrary_ApplyScaleCurveAmplitude_UsesNeutralScale::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("默认参数应表示无缩放变化"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude()
			.Equals(FVector::OneVector, UE_KINDA_SMALL_NUMBER));

	const FVector BaseScale(2.0, 3.0, 4.0);
	const FVector CurveScale(1.2, 0.8, 1.0);

	TestTrue(TEXT("强度为0时应保持基础缩放"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude(BaseScale, CurveScale, 0.0)
			.Equals(BaseScale, UE_KINDA_SMALL_NUMBER));

	TestTrue(TEXT("强度为0.5时应平滑减弱曲线效果"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude(BaseScale, CurveScale, 0.5)
			.Equals(FVector(2.211146, 2.683282, 4.0), UE_KINDA_SMALL_NUMBER));

	TestTrue(TEXT("强度为1时应精确保留曲线原始效果"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude(BaseScale, CurveScale, 1.0)
			.Equals(BaseScale * CurveScale, UE_KINDA_SMALL_NUMBER));

	TestTrue(TEXT("强度大于1时应增强但不越过默认倍率范围"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude(BaseScale, CurveScale, 10.0)
			.Equals(FVector(3.785252, 0.322123, 4.0), UE_KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMathExtensionsLibrary_ApplyScaleCurveAmplitude_BoundsStrengthWithoutNegativeScale,
	"XTools.BlueprintExtensionsRuntime.Math.ApplyScaleCurveAmplitude.BoundsStrengthWithoutNegativeScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMathExtensionsLibrary_ApplyScaleCurveAmplitude_BoundsStrengthWithoutNegativeScale::RunTest(const FString& Parameters)
{
	const FVector BaseScale(2.0, 3.0, 4.0);
	const FVector CurveScale = FVector::ZeroVector;

	TestTrue(TEXT("消失曲线应允许输出0缩放"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude(BaseScale, CurveScale, 1.0)
			.Equals(FVector::ZeroVector, UE_KINDA_SMALL_NUMBER));

	TestTrue(TEXT("强度大于1时应推向0但不产生负缩放"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude(BaseScale, CurveScale, 10.0)
			.Equals(FVector::ZeroVector, UE_KINDA_SMALL_NUMBER));

	TestTrue(TEXT("非法负最小倍率应按0处理，避免输出负缩放"),
		UMathExtensionsLibrary::ApplyScaleCurveAmplitude(BaseScale, CurveScale, 10.0, -1.0, 2.0)
			.Equals(FVector::ZeroVector, UE_KINDA_SMALL_NUMBER));

	return true;
}

#endif
