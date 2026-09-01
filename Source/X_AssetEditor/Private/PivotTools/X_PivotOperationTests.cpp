/*
 * Pivot 源数据修改自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "PivotTools/X_PivotOperation.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "StaticMeshAttributes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXPivotOperationTransformsSourceDataWithoutRenderData,
	"XTools.AssetEditor.Pivot.TransformsSourceDataWithoutRenderData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXPivotOperationTransformsSourceDataWithoutRenderData::RunTest(const FString& Parameters)
{
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage());
	StaticMesh->AddSourceModel();
	FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
	TestNotNull(TEXT("测试网格应创建LOD0源数据"), MeshDescription);
	if (!MeshDescription)
	{
		return false;
	}

	FStaticMeshAttributes Attributes(*MeshDescription);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	const FVertexID VertexID = MeshDescription->CreateVertex();
	Positions[VertexID] = FVector3f(10.0f, 20.0f, 30.0f);

	StaticMesh->CreateBodySetup();
	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	TestNotNull(TEXT("测试网格应创建BodySetup"), BodySetup);
	if (!BodySetup)
	{
		return false;
	}
	FKBoxElem& Box = BodySetup->AggGeom.BoxElems.AddDefaulted_GetRef();
	Box.Center = FVector(1.0, 2.0, 3.0);

	UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(StaticMesh);
	Socket->RelativeLocation = FVector(4.0, 5.0, 6.0);
	StaticMesh->Sockets.Add(Socket);

	const FVector Offset(7.0, 8.0, 9.0);
	FString ErrorMessage;
	FX_PivotOperation Operation(StaticMesh);
	TestTrue(TEXT("仅有源LOD且尚无RenderData时Pivot操作仍应成功"), Operation.Execute(Offset, ErrorMessage));
	TestTrue(TEXT("成功时不应返回错误信息"), ErrorMessage.IsEmpty());

	FMeshDescription* UpdatedMeshDescription = StaticMesh->GetMeshDescription(0);
	TestNotNull(TEXT("操作后LOD0源数据仍应有效"), UpdatedMeshDescription);
	if (UpdatedMeshDescription)
	{
		const FStaticMeshConstAttributes UpdatedAttributes(*UpdatedMeshDescription);
		const TVertexAttributesConstRef<FVector3f> UpdatedPositions = UpdatedAttributes.GetVertexPositions();
		TestTrue(TEXT("源顶点应应用Pivot偏移"),
			UpdatedPositions.IsValid() && FVector(UpdatedPositions[VertexID]).Equals(FVector(17.0, 28.0, 39.0)));
	}

	TestTrue(TEXT("简单碰撞应应用Pivot偏移"),
		BodySetup->AggGeom.BoxElems[0].Center.Equals(FVector(8.0, 10.0, 12.0)));
	TestTrue(TEXT("Socket应应用Pivot偏移"),
		Socket->RelativeLocation.Equals(FVector(11.0, 13.0, 15.0)));
	return true;
}

#endif
