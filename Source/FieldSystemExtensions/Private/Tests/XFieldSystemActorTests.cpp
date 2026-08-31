/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "XFieldSystemActor.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FXFieldSystemActor_EndPlayClearsRuntimeState,
	"XTools.FieldSystem.Actor.EndPlayClearsRuntimeState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXFieldSystemActor_EndPlayClearsRuntimeState::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("XFieldSystemActorEndPlayTest"));
	TestNotNull(TEXT("应创建生命周期测试世界"), World);
	if (!World)
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);

	AXFieldSystemActor* FieldActor = World->SpawnActor<AXFieldSystemActor>();
	TestNotNull(TEXT("应创建Field Actor"), FieldActor);
	if (FieldActor)
	{
		FieldActor->bEnableFiltering = true;
		FieldActor->bEnableActorClassFilter = true;
		FieldActor->bListenToActorSpawn = true;
		FieldActor->DispatchBeginPlay();

		TestTrue(TEXT("BeginPlay后应注册Spawn监听"), FieldActor->SpawnListenerHandle.IsValid());
		TestNotNull(TEXT("BeginPlay后应创建筛选器"), FieldActor->CachedFilter.Get());
		TestTrue(TEXT("BeginPlay后应记录筛选器已应用"), FieldActor->bFilterApplied);

		UGeometryCollectionComponent* GeometryCollection =
			NewObject<UGeometryCollectionComponent>(FieldActor);
		GeometryCollection->InitializationFields.Add(FieldActor);
		FieldActor->CachedGeometryCollections.Add(GeometryCollection);

		// 模拟运行时修改配置；EndPlay必须仍按真实句柄和缓存状态完成清理。
		FieldActor->bListenToActorSpawn = false;
		FieldActor->RouteEndPlay(EEndPlayReason::RemovedFromWorld);

		TestFalse(TEXT("EndPlay后Spawn监听句柄应失效"), FieldActor->SpawnListenerHandle.IsValid());
		TestTrue(TEXT("EndPlay后GC缓存应清空"), FieldActor->CachedGeometryCollections.IsEmpty());
		TestNull(TEXT("EndPlay后筛选器缓存应释放"), FieldActor->CachedFilter.Get());
		TestFalse(TEXT("EndPlay后筛选器应用状态应重置"), FieldActor->bFilterApplied);
		TestFalse(TEXT("EndPlay后GC不应保留Field初始化引用"),
			GeometryCollection->InitializationFields.Contains(FieldActor));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FXFieldSystemActor_FiltersActorsByClassAndTag,
	"XTools.FieldSystem.Actor.FiltersActorsByClassAndTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXFieldSystemActor_FiltersActorsByClassAndTag::RunTest(const FString& Parameters)
{
	AXFieldSystemActor* FieldActor = NewObject<AXFieldSystemActor>(GetTransientPackage());
	AActor* TargetActor = NewObject<AActor>(GetTransientPackage());
	TestNotNull(TEXT("测试用Field Actor应创建成功"), FieldActor);
	TestNotNull(TEXT("测试用目标Actor应创建成功"), TargetActor);
	if (!FieldActor || !TargetActor)
	{
		return false;
	}

	TestTrue(TEXT("未启用筛选时应影响所有Actor"), FieldActor->ShouldAffectActor(TargetActor));
	TestTrue(TEXT("空Actor输入在未配置筛选时应安全通过"), FieldActor->ShouldAffectActor(nullptr));

	FieldActor->bEnableFiltering = true;
	FieldActor->bEnableActorClassFilter = true;
	FieldActor->ExcludeActorClasses = { AActor::StaticClass() };
	TestFalse(TEXT("排除类应拒绝其子类Actor"), FieldActor->ShouldAffectActor(TargetActor));

	FieldActor->ExcludeActorClasses.Reset();
	FieldActor->IncludeActorClasses = { AActor::StaticClass() };
	TestTrue(TEXT("包含类应接受匹配Actor"), FieldActor->ShouldAffectActor(TargetActor));

	FieldActor->bEnableActorClassFilter = false;
	FieldActor->bEnableActorTagFilter = true;
	FieldActor->IncludeActorTags = { TEXT("Affected") };
	TestFalse(TEXT("缺少包含标签时应拒绝Actor"), FieldActor->ShouldAffectActor(TargetActor));
	TargetActor->Tags.Add(TEXT("Affected"));
	TestTrue(TEXT("匹配包含标签时应接受Actor"), FieldActor->ShouldAffectActor(TargetActor));
	FieldActor->ExcludeActorTags = { TEXT("Affected") };
	TestFalse(TEXT("排除标签应优先拒绝Actor"), FieldActor->ShouldAffectActor(TargetActor));

	FieldActor->ApplyFilter();
	TestNotNull(TEXT("启用筛选后应创建筛选器缓存"), FieldActor->GetCachedFilter());
	FieldActor->bEnableFiltering = false;
	FieldActor->ApplyFilter();
	TestNull(TEXT("关闭筛选后重新应用应释放旧筛选器缓存"), FieldActor->GetCachedFilter());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FXFieldSystemActor_RefreshRemovesStaleInitializationField,
	"XTools.FieldSystem.Actor.RefreshRemovesStaleInitializationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXFieldSystemActor_RefreshRemovesStaleInitializationField::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("XFieldSystemActorRefreshTest"));
	TestNotNull(TEXT("应创建缓存刷新测试世界"), World);
	if (!World)
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);

	AActor* TargetActor = World->SpawnActor<AActor>();
	UGeometryCollectionComponent* GeometryCollection = TargetActor
		? NewObject<UGeometryCollectionComponent>(TargetActor)
		: nullptr;
	if (TargetActor && GeometryCollection)
	{
		TargetActor->AddInstanceComponent(GeometryCollection);
		TargetActor->Tags.Add(TEXT("Affected"));
	}

	AXFieldSystemActor* FieldActor = World->SpawnActor<AXFieldSystemActor>();
	TestNotNull(TEXT("应创建缓存刷新测试Field Actor"), FieldActor);
	TestNotNull(TEXT("应创建缓存刷新测试GeometryCollection"), GeometryCollection);
	if (FieldActor && GeometryCollection)
	{
		FieldActor->bEnableFiltering = true;
		FieldActor->bEnableActorTagFilter = true;
		FieldActor->IncludeActorTags = { TEXT("Affected") };
		FieldActor->bAutoRegisterToGCs = true;
		FieldActor->DispatchBeginPlay();

		TestTrue(TEXT("匹配筛选的GC应注册Field初始化引用"),
			GeometryCollection->InitializationFields.Contains(FieldActor));

		TargetActor->Tags.Remove(TEXT("Affected"));
		FieldActor->RefreshGeometryCollectionCache();
		TestFalse(TEXT("刷新后不再匹配的GC应解除Field初始化引用"),
			GeometryCollection->InitializationFields.Contains(FieldActor));

		FieldActor->RouteEndPlay(EEndPlayReason::RemovedFromWorld);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
