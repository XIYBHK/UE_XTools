#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "K2Node_SmartSort.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

namespace
{
	UBlueprint* CreateSmartSortTestBlueprint(UEdGraph*& OutEventGraph)
	{
		const FName BlueprintName = MakeUniqueObjectName(
			GetTransientPackage(),
			UBlueprint::StaticClass(),
			TEXT("SmartSortReconstructTest"));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			GetTransientPackage(),
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);
		OutEventGraph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
		return Blueprint;
	}

	UK2Node_TemporaryVariable* AddTemporaryVariable(UEdGraph* Graph, const FEdGraphPinType& VariableType)
	{
		UK2Node_TemporaryVariable* Node = NewObject<UK2Node_TemporaryVariable>(Graph);
		Graph->AddNode(Node);
		Node->CreateNewGuid();
		Node->VariableType = VariableType;
		Node->AllocateDefaultPins();
		return Node;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSmartSortNode_PreservesDynamicPinLinksAfterReconstruct,
	"XTools.SortEditor.SmartSort.PreservesDynamicPinLinksAfterReconstruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSmartSortNode_PreservesDynamicPinLinksAfterReconstruct::RunTest(const FString& Parameters)
{
	UEdGraph* EventGraph = nullptr;
	UBlueprint* Blueprint = CreateSmartSortTestBlueprint(EventGraph);
	if (!TestNotNull(TEXT("应创建测试蓝图"), Blueprint) ||
		!TestNotNull(TEXT("应找到事件图"), EventGraph))
	{
		return false;
	}

	FEdGraphPinType VectorArrayType;
	VectorArrayType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	VectorArrayType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	VectorArrayType.ContainerType = EPinContainerType::Array;
	UK2Node_TemporaryVariable* ArraySource = AddTemporaryVariable(EventGraph, VectorArrayType);

	UK2Node_SmartSort* SmartSort = NewObject<UK2Node_SmartSort>(EventGraph);
	EventGraph->AddNode(SmartSort);
	SmartSort->CreateNewGuid();
	SmartSort->AllocateDefaultPins();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!TestTrue(TEXT("应连接向量数组输入"),
		Schema->TryCreateConnection(ArraySource->GetVariablePin(), SmartSort->GetArrayInputPin())))
	{
		return false;
	}

	UEdGraphPin* ModePin = SmartSort->GetSortModePin();
	TestNotNull(TEXT("连接向量数组后应显示排序模式"), ModePin);
	if (!ModePin)
	{
		return false;
	}
	Schema->TrySetDefaultValue(*ModePin, StaticEnum<EVectorSortMode>()->GetNameStringByValue(
		static_cast<int64>(EVectorSortMode::ByProjection)));

	UEdGraphPin* DirectionPin = SmartSort->FindPin(FSmartSort_Helper::PN_Direction, EGPD_Input);
	TestNotNull(TEXT("投影排序应创建方向引脚"), DirectionPin);
	if (!DirectionPin)
	{
		return false;
	}

	FEdGraphPinType VectorType;
	VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	UK2Node_TemporaryVariable* DirectionSource = AddTemporaryVariable(EventGraph, VectorType);
	if (!TestTrue(TEXT("应连接投影方向"),
		Schema->TryCreateConnection(DirectionSource->GetVariablePin(), DirectionPin)))
	{
		return false;
	}

	SmartSort->ReconstructNode();

	UEdGraphPin* ReconstructedDirectionPin = SmartSort->FindPin(FSmartSort_Helper::PN_Direction, EGPD_Input);
	TestNotNull(TEXT("重建后应保留方向引脚"), ReconstructedDirectionPin);
	TestTrue(TEXT("重建后应保留方向连接"),
		ReconstructedDirectionPin &&
		ReconstructedDirectionPin->LinkedTo.Contains(DirectionSource->GetVariablePin()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSmartSortNode_PreservesRuntimeModePinsAfterReconstruct,
	"XTools.SortEditor.SmartSort.PreservesRuntimeModePinsAfterReconstruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSmartSortNode_PreservesRuntimeModePinsAfterReconstruct::RunTest(const FString& Parameters)
{
	UEdGraph* EventGraph = nullptr;
	UBlueprint* Blueprint = CreateSmartSortTestBlueprint(EventGraph);
	if (!TestNotNull(TEXT("应创建测试蓝图"), Blueprint) ||
		!TestNotNull(TEXT("应找到事件图"), EventGraph))
	{
		return false;
	}

	FEdGraphPinType VectorArrayType;
	VectorArrayType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	VectorArrayType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	VectorArrayType.ContainerType = EPinContainerType::Array;
	UK2Node_TemporaryVariable* ArraySource = AddTemporaryVariable(EventGraph, VectorArrayType);

	UK2Node_SmartSort* SmartSort = NewObject<UK2Node_SmartSort>(EventGraph);
	EventGraph->AddNode(SmartSort);
	SmartSort->CreateNewGuid();
	SmartSort->AllocateDefaultPins();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!TestTrue(TEXT("应连接向量数组输入"),
		Schema->TryCreateConnection(ArraySource->GetVariablePin(), SmartSort->GetArrayInputPin())))
	{
		return false;
	}

	FEdGraphPinType ModeType;
	ModeType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	ModeType.PinSubCategoryObject = StaticEnum<EVectorSortMode>();
	UK2Node_TemporaryVariable* ModeSource = AddTemporaryVariable(EventGraph, ModeType);
	if (!TestTrue(TEXT("应连接运行时排序模式"),
		Schema->TryCreateConnection(ModeSource->GetVariablePin(), SmartSort->GetSortModePin())))
	{
		return false;
	}

	UEdGraphPin* DirectionPin = SmartSort->FindPin(FSmartSort_Helper::PN_Direction, EGPD_Input);
	UEdGraphPin* AxisPin = SmartSort->FindPin(FSmartSort_Helper::PN_Axis, EGPD_Input);
	if (!TestNotNull(TEXT("运行时模式应创建方向引脚"), DirectionPin) ||
		!TestNotNull(TEXT("运行时模式应创建坐标轴引脚"), AxisPin))
	{
		return false;
	}

	FEdGraphPinType VectorType;
	VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	UK2Node_TemporaryVariable* DirectionSource = AddTemporaryVariable(EventGraph, VectorType);
	if (!TestTrue(TEXT("应连接运行时投影方向"),
		Schema->TryCreateConnection(DirectionSource->GetVariablePin(), DirectionPin)))
	{
		return false;
	}

	SmartSort->ReconstructNode();

	UEdGraphPin* ReconstructedDirectionPin = SmartSort->FindPin(FSmartSort_Helper::PN_Direction, EGPD_Input);
	TestTrue(TEXT("运行时模式重建后应保留方向连接"),
		ReconstructedDirectionPin &&
		ReconstructedDirectionPin->LinkedTo.Contains(DirectionSource->GetVariablePin()));
	TestNotNull(TEXT("运行时模式重建后应保留坐标轴引脚"),
		SmartSort->FindPin(FSmartSort_Helper::PN_Axis, EGPD_Input));

	return true;
}

#endif
