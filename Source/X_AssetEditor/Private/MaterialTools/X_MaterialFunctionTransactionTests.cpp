/*
 * 材质函数事务自动化测试
 */

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "MaterialTools/X_MaterialFunctionOperation.h"

#include "Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXMaterialFunctionAddIsUndoable,
	"XTools.AssetEditor.MaterialFunction.AddIsUndoable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXMaterialFunctionBatchAddIsSingleUndo,
	"XTools.AssetEditor.MaterialFunction.BatchAddIsSingleUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXMaterialFunctionAddIsUndoable::RunTest(const FString& Parameters)
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

	UPackage* TestPackage = CreatePackage(TEXT("/Game/__XToolsAutomation__/MaterialFunctionTransaction"));
	UMaterial* TemplateMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterial* Material = TemplateMaterial
		? DuplicateObject<UMaterial>(TemplateMaterial, TestPackage, TEXT("M_MaterialFunctionTransaction"))
		: nullptr;
	UMaterialFunction* MaterialFunction = LoadObject<UMaterialFunction>(
		nullptr,
		TEXT("/Engine/Functions/Engine_MaterialFunctions02/Fresnel_Function.Fresnel_Function"));
	TestNotNull(TEXT("应能加载引擎基础材质"), TemplateMaterial);
	TestNotNull(TEXT("应能创建临时材质"), Material);
	TestNotNull(TEXT("应能加载引擎材质函数"), MaterialFunction);
	if (!Material || !MaterialFunction)
	{
		return false;
	}

	const int32 InitialExpressionCount = Material->GetExpressions().Num();
	UMaterialExpressionMaterialFunctionCall* FunctionCall = FX_MaterialFunctionOperation::AddFunctionToMaterial(
		Material,
		MaterialFunction,
		NAME_None,
		100,
		100,
		false,
		false,
		EConnectionMode::None,
		nullptr);
	TestNotNull(TEXT("应成功创建材质函数调用节点"), FunctionCall);
	TestEqual(TEXT("添加后表达式数量应增加一"), Material->GetExpressions().Num(), InitialExpressionCount + 1);

	TestTrue(TEXT("材质函数添加应生成可撤销事务"), GEditor->UndoTransaction(false));
	TestEqual(TEXT("撤销后应恢复原始表达式数量"), Material->GetExpressions().Num(), InitialExpressionCount);
	return true;
}

bool FXMaterialFunctionBatchAddIsSingleUndo::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("编辑器事务系统应可用"), GEditor);
	if (!GEditor || GEditor->IsTransactionActive())
	{
		return false;
	}

	UPackage* TestPackage = CreatePackage(TEXT("/Game/__XToolsAutomation__/MaterialFunctionBatchTransaction"));
	UMaterial* TemplateMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialFunction* MaterialFunction = LoadObject<UMaterialFunction>(
		nullptr,
		TEXT("/Engine/Functions/Engine_MaterialFunctions02/Fresnel_Function.Fresnel_Function"));
	UMaterial* FirstMaterial = TemplateMaterial
		? DuplicateObject<UMaterial>(TemplateMaterial, TestPackage, TEXT("M_BatchTransactionA"))
		: nullptr;
	UMaterial* SecondMaterial = TemplateMaterial
		? DuplicateObject<UMaterial>(TemplateMaterial, TestPackage, TEXT("M_BatchTransactionB"))
		: nullptr;
	TestNotNull(TEXT("应能创建第一份测试材质"), FirstMaterial);
	TestNotNull(TEXT("应能创建第二份测试材质"), SecondMaterial);
	TestNotNull(TEXT("应能加载引擎材质函数"), MaterialFunction);
	if (!FirstMaterial || !SecondMaterial || !MaterialFunction)
	{
		return false;
	}

	const int32 FirstInitialCount = FirstMaterial->GetExpressions().Num();
	const int32 SecondInitialCount = SecondMaterial->GetExpressions().Num();
	const TArray<UObject*> SourceObjects{FirstMaterial, SecondMaterial};
	const FMaterialProcessResult Result = FX_MaterialFunctionOperation::AddFunctionToMultipleMaterials(
		SourceObjects,
		MaterialFunction,
		NAME_None,
		100,
		100,
		false,
		nullptr);
	TestEqual(TEXT("两份材质都应添加成功"), Result.SuccessCount, 2);
	TestEqual(TEXT("第一份材质表达式数量应增加一"), FirstMaterial->GetExpressions().Num(), FirstInitialCount + 1);
	TestEqual(TEXT("第二份材质表达式数量应增加一"), SecondMaterial->GetExpressions().Num(), SecondInitialCount + 1);

	TestTrue(TEXT("批处理应只需一次撤销"), GEditor->UndoTransaction(false));
	TestEqual(TEXT("撤销后第一份材质应恢复"), FirstMaterial->GetExpressions().Num(), FirstInitialCount);
	TestEqual(TEXT("撤销后第二份材质应恢复"), SecondMaterial->GetExpressions().Num(), SecondInitialCount);
	return true;
}

#endif
