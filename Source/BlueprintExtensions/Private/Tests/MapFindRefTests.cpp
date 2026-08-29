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
#include "Kismet/KismetStringLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

namespace
{
	template <typename NodeType>
	NodeType* AddMapFindTestNode(UEdGraph* Graph)
	{
		NodeType* Node = NewObject<NodeType>(Graph);
		Graph->AddNode(Node);
		Node->CreateNewGuid();
		return Node;
	}
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
