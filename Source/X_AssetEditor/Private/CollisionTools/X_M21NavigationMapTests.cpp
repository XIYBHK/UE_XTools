/*
 * M21 地图级导航 fixture 自动化测试。
 * 使用项目中固定的样条移动测试地图，验证 NavMesh、投影和 AI MoveTo 请求链路。
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXM21NavigationMapFixtureTest,
	"XTools.SplineMovement.NavigationMapFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXM21NavigationMapFixtureTest::RunTest(const FString& Parameters)
{
	const FString MapPath = TEXT("/Game/样条移动/样条移动Untitled");
	const bool bLoaded = FEditorFileUtils::LoadMap(MapPath, false, false);
	UWorld* World = bLoaded && GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TestNotNull(TEXT("M21 测试地图应能加载"), World);
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	TestNotNull(TEXT("测试地图应创建 NavigationSystem"), NavSys);
	if (!NavSys)
	{
		return false;
	}

	ANavMeshBoundsVolume* BoundsVolume = nullptr;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		BoundsVolume = *It;
		break;
	}
	TestNotNull(TEXT("测试地图应包含 NavMeshBoundsVolume"), BoundsVolume);
	if (!BoundsVolume)
	{
		return false;
	}

	const FVector Center = BoundsVolume->GetActorLocation();
	FNavLocation StartLocation;
	FNavLocation GoalLocation;
	const bool bStartOnNav = NavSys->ProjectPointToNavigation(Center, StartLocation, FVector(1000.0f));
	const bool bGoalOnNav = NavSys->ProjectPointToNavigation(Center + FVector(200.0f, 0.0f, 0.0f), GoalLocation, FVector(1000.0f));
	TestTrue(TEXT("起点应投影到 NavMesh"), bStartOnNav);
	TestTrue(TEXT("目标点应投影到 NavMesh"), bGoalOnNav);
	if (!bStartOnNav || !bGoalOnNav)
	{
		return false;
	}

	FVector::FReal PathLength = 0.0;
	const ENavigationQueryResult::Type PathResult = NavSys->GetPathLength(StartLocation.Location, GoalLocation.Location, PathLength);
	TestEqual(TEXT("起点到目标应存在可计算路径"), PathResult, ENavigationQueryResult::Success);
	TestTrue(TEXT("路径长度应为正数"), PathLength > 0.0f);

	ACharacter* Pawn = World->SpawnActor<ACharacter>(StartLocation.Location, FRotator::ZeroRotator);
	AAIController* Controller = World->SpawnActor<AAIController>();
	TestNotNull(TEXT("地图测试应生成 Pawn"), Pawn);
	TestNotNull(TEXT("地图测试应生成 AIController"), Controller);
	if (Pawn && Controller)
	{
		Controller->Possess(Pawn);
		const EPathFollowingRequestResult::Type MoveResult = Controller->MoveToLocation(
			GoalLocation.Location, 20.0f, false, true, true);
		TestTrue(TEXT("NavMesh 上的目标应接受 MoveTo 请求"),
			MoveResult == EPathFollowingRequestResult::RequestSuccessful ||
			MoveResult == EPathFollowingRequestResult::AlreadyAtGoal);
		Controller->StopMovement();
	}

	return true;
}

#endif
