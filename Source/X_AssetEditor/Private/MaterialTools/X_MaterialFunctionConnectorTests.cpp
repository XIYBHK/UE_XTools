/*
 * 材质函数连接器自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "MaterialTools/X_MaterialFunctionConnector.h"

#include "Editor.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	UMaterial* DuplicateBasicShapeMaterial(UPackage* Package, const FName AssetName)
	{
		UMaterial* TemplateMaterial = LoadObject<UMaterial>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		return TemplateMaterial ? DuplicateObject<UMaterial>(TemplateMaterial, Package, AssetName) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXMaterialConnectorDirectConnectionIsUndoable,
	"XTools.AssetEditor.MaterialConnector.DirectConnectionIsUndoable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXMaterialConnectorAddConnectionIsAtomicAndUndoable,
	"XTools.AssetEditor.MaterialConnector.AddConnectionIsAtomicAndUndoable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXMaterialConnectorDirectConnectionIsUndoable::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("编辑器事务系统应可用"), GEditor);
	if (!GEditor || GEditor->IsTransactionActive())
	{
		return false;
	}

	UPackage* TestPackage = CreatePackage(TEXT("/Game/__XToolsAutomation__/MaterialConnectorDirect"));
	UMaterial* Material = DuplicateBasicShapeMaterial(TestPackage, TEXT("M_ConnectorDirect"));
	UMaterial* OtherMaterial = DuplicateBasicShapeMaterial(TestPackage, TEXT("M_ConnectorOther"));
	TestNotNull(TEXT("应能创建目标材质副本"), Material);
	TestNotNull(TEXT("应能创建另一份材质副本"), OtherMaterial);
	if (!Material || !OtherMaterial)
	{
		return false;
	}

	UMaterialExpressionConstant3Vector* SourceExpression = Cast<UMaterialExpressionConstant3Vector>(
		UMaterialEditingLibrary::CreateMaterialExpression(
			Material,
			UMaterialExpressionConstant3Vector::StaticClass(),
			-400,
			0));
	TestNotNull(TEXT("应能创建连接源表达式"), SourceExpression);
	if (!SourceExpression)
	{
		return false;
	}

	UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
	UMaterialEditorOnlyData* OtherEditorOnlyData = OtherMaterial->GetEditorOnlyData();
	TestNotNull(TEXT("目标材质应有编辑器数据"), EditorOnlyData);
	TestNotNull(TEXT("另一份材质应有编辑器数据"), OtherEditorOnlyData);
	if (!EditorOnlyData || !OtherEditorOnlyData)
	{
		return false;
	}

	UMaterialExpression* OriginalExpression = EditorOnlyData->BaseColor.Expression;
	const int32 OriginalOutputIndex = EditorOnlyData->BaseColor.OutputIndex;
	UMaterialExpression* OtherOriginalExpression = OtherEditorOnlyData->BaseColor.Expression;

	AddExpectedError(TEXT("表达式输出索引无效"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("非法输出索引应被拒绝"),
		FX_MaterialFunctionConnector::ConnectExpressionToMaterialProperty(
			Material, SourceExpression, MP_BaseColor, 99));
	TestEqual(TEXT("非法输出索引不应改写连接"), EditorOnlyData->BaseColor.Expression, OriginalExpression);

	AddExpectedError(TEXT("表达式不属于目标材质"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("跨材质表达式应被拒绝"),
		FX_MaterialFunctionConnector::ConnectExpressionToMaterialProperty(
			OtherMaterial, SourceExpression, MP_BaseColor, 0));
	TestEqual(TEXT("跨材质失败不应改写目标"), OtherEditorOnlyData->BaseColor.Expression, OtherOriginalExpression);

	TestTrue(TEXT("有效表达式应连接到BaseColor"),
		FX_MaterialFunctionConnector::ConnectExpressionToMaterialProperty(
			Material, SourceExpression, MP_BaseColor, 0));
	TestTrue(TEXT("连接后BaseColor应指向新表达式"), EditorOnlyData->BaseColor.Expression == SourceExpression);

	TestTrue(TEXT("直连操作应生成可撤销事务"), GEditor->UndoTransaction(false));
	TestEqual(TEXT("撤销后应恢复原表达式"), EditorOnlyData->BaseColor.Expression, OriginalExpression);
	TestEqual(TEXT("撤销后应恢复原输出索引"), EditorOnlyData->BaseColor.OutputIndex, OriginalOutputIndex);
	return true;
}

bool FXMaterialConnectorAddConnectionIsAtomicAndUndoable::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("编辑器事务系统应可用"), GEditor);
	if (!GEditor || GEditor->IsTransactionActive())
	{
		return false;
	}

	UPackage* TestPackage = CreatePackage(TEXT("/Game/__XToolsAutomation__/MaterialConnectorAdd"));
	UMaterial* Material = DuplicateBasicShapeMaterial(TestPackage, TEXT("M_ConnectorAdd"));
	UMaterialFunction* MaterialFunction = LoadObject<UMaterialFunction>(
		nullptr,
		TEXT("/Engine/Functions/Engine_MaterialFunctions02/Fresnel_Function.Fresnel_Function"));
	TestNotNull(TEXT("应能创建目标材质副本"), Material);
	TestNotNull(TEXT("应能加载测试材质函数"), MaterialFunction);
	if (!Material || !MaterialFunction)
	{
		return false;
	}

	UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(
		UMaterialEditingLibrary::CreateMaterialExpression(
			Material,
			UMaterialExpressionMaterialFunctionCall::StaticClass(),
			-400,
			0));
	TestNotNull(TEXT("应能创建函数调用表达式"), FunctionCall);
	if (!FunctionCall || !FunctionCall->SetMaterialFunction(MaterialFunction))
	{
		AddError(TEXT("测试函数调用应接受材质函数"));
		return false;
	}

	UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
	TestNotNull(TEXT("目标材质应有编辑器数据"), EditorOnlyData);
	if (!EditorOnlyData)
	{
		return false;
	}

	const int32 InitialExpressionCount = Material->GetExpressions().Num();
	TestNull(TEXT("Add不应接受MaterialAttributes目标"),
		FX_MaterialFunctionConnector::CreateAddConnectionToProperty(
			Material, FunctionCall, 0, MP_MaterialAttributes));
	TestEqual(TEXT("无效目标不应遗留孤立节点"), Material->GetExpressions().Num(), InitialExpressionCount);

	UMaterialExpression* OriginalExpression = EditorOnlyData->BaseColor.Expression;
	const int32 OriginalOutputIndex = EditorOnlyData->BaseColor.OutputIndex;
	UMaterialExpressionAdd* AddExpression = FX_MaterialFunctionConnector::CreateAddConnectionToProperty(
		Material,
		FunctionCall,
		0,
		MP_BaseColor);
	TestNotNull(TEXT("有效目标应创建Add节点"), AddExpression);
	TestEqual(TEXT("创建后表达式数量应增加一"), Material->GetExpressions().Num(), InitialExpressionCount + 1);
	if (!AddExpression)
	{
		return false;
	}
	TestTrue(TEXT("Add节点应成为BaseColor来源"), EditorOnlyData->BaseColor.Expression == AddExpression);
	TestEqual(TEXT("Add节点应保留原BaseColor连接"), AddExpression->B.Expression, OriginalExpression);

	TestTrue(TEXT("Add连接应生成可撤销事务"), GEditor->UndoTransaction(false));
	TestEqual(TEXT("撤销后应移除Add节点"), Material->GetExpressions().Num(), InitialExpressionCount);
	TestEqual(TEXT("撤销后应恢复原BaseColor来源"), EditorOnlyData->BaseColor.Expression, OriginalExpression);
	TestEqual(TEXT("撤销后应恢复原输出索引"), EditorOnlyData->BaseColor.OutputIndex, OriginalOutputIndex);
	return true;
}

#endif
