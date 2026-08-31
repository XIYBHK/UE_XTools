#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "K2Nodes/K2Node_MapFindRef.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeMap.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/Package.h"

namespace
{
	class FMapFindCompilerContext : public FKismetCompilerContext
	{
	public:
		FMapFindCompilerContext(
			UBlueprint* Blueprint,
			FCompilerResultsLog& Results,
			const FKismetCompilerOptions& Options)
			: FKismetCompilerContext(Blueprint, Results, Options)
		{
			Schema = GetMutableDefault<UEdGraphSchema_K2>();
		}
	};

	template <typename NodeType>
	NodeType* AddMapFindTestNode(UEdGraph* Graph)
	{
		NodeType* Node = NewObject<NodeType>(Graph);
		Graph->AddNode(Node);
		Node->CreateNewGuid();
		return Node;
	}

	UBlueprint* CreateMapFindBlueprint(const TCHAR* BaseName, UEdGraph*& OutEventGraph)
	{
		const FName BlueprintName = MakeUniqueObjectName(
			GetTransientPackage(), UBlueprint::StaticClass(), FName(BaseName));
		UPackage* BlueprintPackage = CreatePackage(
			*FString::Printf(TEXT("/Game/%s"), *BlueprintName.ToString()));
		BlueprintPackage->SetFlags(RF_Transient);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), BlueprintPackage, BlueprintName, BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
		OutEventGraph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
		return Blueprint;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMapFindRef_RejectsUnconnectedMap,
	"XTools.BlueprintExtensions.MapFindRef.RejectsUnconnectedMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapFindRef_RejectsUnconnectedMap::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("MapFindRef requires a Map connection"), EAutomationExpectedErrorFlags::Contains, 1);

	const FName BlueprintName = MakeUniqueObjectName(
		GetTransientPackage(), UBlueprint::StaticClass(), TEXT("MapFindRefValidationTest"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), GetTransientPackage(), BlueprintName, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	UEdGraph* Graph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
	if (!TestNotNull(TEXT("MapFindRef校验图可用"), Graph))
	{
		return false;
	}

	UK2Node_MapFindRef* MapFind = AddMapFindTestNode<UK2Node_MapFindRef>(Graph);
	MapFind->AllocateDefaultPins();
	FCompilerResultsLog Results;
	FKismetCompilerOptions Options;
	FMapFindCompilerContext CompilerContext(Blueprint, Results, Options);
	MapFind->ExpandNode(CompilerContext, Graph);

	return TestEqual(TEXT("Map未连接产生一次编译错误"), Results.NumErrors, 1)
		&& TestEqual(TEXT("Map未连接不降级为警告"), Results.NumWarnings, 0)
		&& !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMapFindRef_CompilesExpansion,
	"XTools.BlueprintExtensions.MapFindRef.CompilesExpansion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapFindRef_CompilesExpansion::RunTest(const FString& Parameters)
{
	AddExpectedError(
		TEXT("ScanPathsSynchronous: Package /Game/XToolsMapFindRefCompileTest_"),
		EAutomationExpectedErrorFlags::Contains, 1);

	UEdGraph* Graph = nullptr;
	UBlueprint* Blueprint = CreateMapFindBlueprint(TEXT("XToolsMapFindRefCompileTest"), Graph);
	if (!TestNotNull(TEXT("创建MapFindRef编译蓝图"), Blueprint)
		|| !TestNotNull(TEXT("获取MapFindRef编译事件图"), Graph))
	{
		return false;
	}

	UK2Node_TemporaryVariable* MapSource = AddMapFindTestNode<UK2Node_TemporaryVariable>(Graph);
	MapSource->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
	MapSource->VariableType.ContainerType = EPinContainerType::Map;
	MapSource->VariableType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_String;
	MapSource->AllocateDefaultPins();

	UK2Node_MapFindRef* MapFind = AddMapFindTestNode<UK2Node_MapFindRef>(Graph);
	MapFind->AllocateDefaultPins();
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!TestTrue(TEXT("连接MapFindRef的Map输入"),
		Schema && Schema->TryCreateConnection(MapSource->GetVariablePin(), MapFind->GetMapPin())))
	{
		return false;
	}

	UK2Node_CallFunction* ValueConsumer = AddMapFindTestNode<UK2Node_CallFunction>(Graph);
	ValueConsumer->SetFromFunction(UKismetStringLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr)));
	ValueConsumer->AllocateDefaultPins();
	UK2Node_CallFunction* FoundConsumer = AddMapFindTestNode<UK2Node_CallFunction>(Graph);
	FoundConsumer->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool)));
	FoundConsumer->AllocateDefaultPins();
	if (!TestTrue(TEXT("连接MapFindRef值输出"), Schema->TryCreateConnection(
		MapFind->GetValuePin(), ValueConsumer->FindPin(TEXT("A"), EGPD_Input)))
		|| !TestTrue(TEXT("连接MapFindRef查找结果"), Schema->TryCreateConnection(
			MapFind->GetFoundResultPin(), FoundConsumer->FindPin(TEXT("A"), EGPD_Input))))
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return TestEqual(TEXT("MapFindRef展开后蓝图无警告编译成功"), Blueprint->Status, BS_UpToDate)
		&& !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMapFindRef_RemovesAllIncompatibleLinks,
	"XTools.BlueprintExtensions.MapFindRef.RemovesAllIncompatibleLinks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapFindRef_RemovesAllIncompatibleLinks::RunTest(const FString& Parameters)
{
	const FName BlueprintName = MakeUniqueObjectName(
		GetTransientPackage(),
		UBlueprint::StaticClass(),
		TEXT("MapFindRefLinkTest"));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		GetTransientPackage(),
		BlueprintName,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None);
	UEdGraph* Graph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
	if (!TestNotNull(TEXT("Test graph must be available"), Graph))
	{
		return false;
	}

	UK2Node_MakeMap* MakeMap = AddMapFindTestNode<UK2Node_MakeMap>(Graph);
	MakeMap->AllocateDefaultPins();
	UEdGraphPin* MapOutput = MakeMap->GetOutputPin();
	MapOutput->PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	MapOutput->PinType.ContainerType = EPinContainerType::Map;
	MapOutput->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int;

	UK2Node_MapFindRef* MapFind = AddMapFindTestNode<UK2Node_MapFindRef>(Graph);
	MapFind->AllocateDefaultPins();
	UEdGraphPin* MapPin = MapFind->GetMapPin();
	MapOutput->MakeLinkTo(MapPin);
	MapFind->NotifyPinConnectionListChanged(MapPin);

	UK2Node_CallFunction* StringCall = AddMapFindTestNode<UK2Node_CallFunction>(Graph);
	StringCall->SetFromFunction(UKismetStringLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr)));
	StringCall->AllocateDefaultPins();
	UEdGraphPin* StringA = StringCall->FindPinChecked(TEXT("A"), EGPD_Input);
	UEdGraphPin* StringB = StringCall->FindPinChecked(TEXT("B"), EGPD_Input);
	UEdGraphPin* ValuePin = MapFind->GetValuePin();

	// Build a stale graph state that the propagation repair path is responsible for cleaning.
	ValuePin->LinkedTo.Add(StringA);
	StringA->LinkedTo.Add(ValuePin);
	ValuePin->LinkedTo.Add(StringB);
	StringB->LinkedTo.Add(ValuePin);

	MapFind->NotifyPinConnectionListChanged(ValuePin);

	TestEqual(TEXT("All incompatible value links must be removed"), ValuePin->LinkedTo.Num(), 0);
	TestEqual(TEXT("First incompatible sink must be unlinked"), StringA->LinkedTo.Num(), 0);
	TestEqual(TEXT("Second incompatible sink must be unlinked"), StringB->LinkedTo.Num(), 0);
	return true;
}

#endif
