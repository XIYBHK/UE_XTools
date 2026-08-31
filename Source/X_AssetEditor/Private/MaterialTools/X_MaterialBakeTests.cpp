/*
 * 材质烘焙自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "MaterialTools/X_MaterialBakeBlueprintLibrary.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXMaterialBakeFailureDoesNotMutateMeshDescription,
	"XTools.AssetEditor.MaterialBake.FailureDoesNotMutateMeshDescription",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXMaterialBakeFailureDoesNotMutateMeshDescription::RunTest(const FString& Parameters)
{
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage());
	StaticMesh->AddSourceModel();
	FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
	TestNotNull(TEXT("测试网格应创建LOD0 MeshDescription"), MeshDescription);
	if (!MeshDescription)
	{
		return false;
	}

	FStaticMeshAttributes Attributes(*MeshDescription);
	Attributes.Register();
	MeshDescription->VertexInstanceAttributes().UnregisterAttribute(MeshAttribute::VertexInstance::Color);
	TestFalse(TEXT("测试前不应存在顶点色属性"), Attributes.GetVertexInstanceColors().IsValid());

	const FX_MaterialBakeVertexColorResult Result =
		UX_MaterialBakeBlueprintLibrary::BakeStaticMeshBaseColorToVertexColors(StaticMesh, 0, 64, 0);

	TestFalse(TEXT("无材质槽时烘焙应失败"), Result.bSuccess);
	TestTrue(TEXT("失败原因应明确指出缺少材质槽"), Result.Message.Contains(TEXT("没有材质槽")));
	TestFalse(TEXT("失败路径不应注册顶点色属性"), Attributes.GetVertexInstanceColors().IsValid());
	return true;
}

#endif
