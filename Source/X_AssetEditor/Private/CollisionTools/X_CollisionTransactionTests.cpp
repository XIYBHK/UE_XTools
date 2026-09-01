/*
 * 碰撞工具事务自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CollisionTools/X_CollisionManager.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXCollisionDirectOperationIsUndoable,
	"XTools.AssetEditor.Collision.DirectOperationIsUndoable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXCollisionDirectOperationIsUndoable::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("编辑器事务系统应可用"), GEditor);
	if (!GEditor)
	{
		return false;
	}
	if (GEditor->IsTransactionActive())
	{
		AddError(TEXT("测试开始前不应存在活动事务"));
		return false;
	}

	UStaticMesh* TemplateMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("应能加载引擎基础立方体网格"), TemplateMesh);
	if (!TemplateMesh)
	{
		return false;
	}

	UStaticMesh* StaticMesh = DuplicateObject<UStaticMesh>(TemplateMesh, GetTransientPackage());
	TestNotNull(TEXT("应能创建有效的临时静态网格"), StaticMesh);
	if (!StaticMesh)
	{
		return false;
	}

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	TestNotNull(TEXT("测试网格应创建BodySetup"), BodySetup);
	if (!BodySetup)
	{
		return false;
	}

	BodySetup->RemoveSimpleCollision();
	BodySetup->AggGeom.BoxElems.AddDefaulted();
	TestEqual(TEXT("测试前应有一个简单碰撞体"), BodySetup->AggGeom.GetElementCount(), 1);
	TestTrue(TEXT("单网格碰撞移除应成功"), FX_CollisionManager::RemoveCollisionFromMesh(StaticMesh));
	TestEqual(TEXT("操作后简单碰撞应被移除"), BodySetup->AggGeom.GetElementCount(), 0);

	TestTrue(TEXT("单网格入口应生成可撤销事务"), GEditor->UndoTransaction(false));
	BodySetup = StaticMesh->GetBodySetup();
	TestNotNull(TEXT("撤销后BodySetup仍应有效"), BodySetup);
	if (BodySetup)
	{
		TestEqual(TEXT("撤销后应恢复简单碰撞"), BodySetup->AggGeom.GetElementCount(), 1);
	}
	return true;
}

#endif
