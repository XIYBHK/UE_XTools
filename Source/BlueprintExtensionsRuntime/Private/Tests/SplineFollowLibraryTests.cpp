/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Libraries/SplineFollowLibrary.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
	USplineComponent* CreateLinearSpline(double Length)
	{
		USplineComponent* Spline = NewObject<USplineComponent>(GetTransientPackage());
		Spline->ClearSplinePoints(false);
		Spline->AddSplinePoint(FVector::ZeroVector, ESplineCoordinateSpace::Local, false);
		Spline->AddSplinePoint(FVector(Length, 0.0, 0.0), ESplineCoordinateSpace::Local, true);
		return Spline;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSplineFollowLibrary_ResetsPathCacheWhenSplineChanges,
	"XTools.BlueprintExtensionsRuntime.SplineFollow.ResetsPathCacheWhenSplineChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSplineFollowLibrary_ResetsPathCacheWhenSplineChanges::RunTest(const FString& Parameters)
{
	AActor* Actor = NewObject<AActor>(GetTransientPackage());
	USplineComponent* LongSpline = CreateLinearSpline(1000.0);
	USplineComponent* ShortSpline = CreateLinearSpline(100.0);
	TestNotNull(TEXT("应创建瞬态Actor"), Actor);
	TestNotNull(TEXT("应创建长样条"), LongSpline);
	TestNotNull(TEXT("应创建短样条"), ShortSpline);
	if (!Actor || !LongSpline || !ShortSpline)
	{
		return false;
	}

	Actor->SetActorLocation(FVector::ZeroVector);
	FXToolsSplineFollowState State;
	FXToolsSplineFollowResult Result;
	USplineFollowLibrary::CalculateSplineFollowTarget(Actor, LongSpline, State, Result, 0.0, 500.0);
	TestTrue(TEXT("长样条应缓存500距离的前视目标"), FMath::IsNearlyEqual(Result.TargetDistance, 500.0, 0.1));

	USplineFollowLibrary::CalculateSplineFollowTarget(Actor, ShortSpline, State, Result, 0.0, 10.0);
	TestTrue(TEXT("切换样条后应按新路径重新计算前视目标"), FMath::IsNearlyEqual(Result.TargetDistance, 10.0, 0.1));
	TestTrue(TEXT("状态应记录当前缓存所属样条"), State.CachedSplineComponent.Get() == ShortSpline);

	return true;
}

#endif
