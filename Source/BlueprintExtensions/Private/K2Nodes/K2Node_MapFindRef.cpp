#include "K2Nodes/K2Node_MapFindRef.h"
#include "K2Nodes/K2NodeHelpers.h"

// 编辑器功能
#include "EdGraphSchema_K2.h"
#include "ToolMenus.h"

// 蓝图系统
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "SPinTypeSelector.h"
#include "Kismet2/BlueprintEditorUtils.h"

// 编译器-ExpandNode相关
#include "KismetCompiler.h"

// 节点
#include "K2Node_CallFunction.h"

// 功能库
#include "Kismet/BlueprintMapLibrary.h"

class SWidget;
struct FLinearColor;

#define LOCTEXT_NAMESPACE "MapFindRef"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_MapFindRef::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "查找值");
}

FText UK2Node_MapFindRef::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "查找值");
}

FText UK2Node_MapFindRef::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "根据键查找Map中的项并返回值副本\n修改输出不会回写Map");
}

FText UK2Node_MapFindRef::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Map");;
}

FSlateIcon UK2Node_MapFindRef::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.PureFunction_16x");
	return Icon;
}

TSharedPtr<SWidget> UK2Node_MapFindRef::CreateNodeImage() const
{
	return SPinTypeSelector::ConstructPinTypeImage(GetMapPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

// 使用ExpandNode方式实现，而不是自定义Handler
// 因为KCST_MapFindOutRef在标准UE中不存在
void UK2Node_MapFindRef::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// 先展开拆分引脚：结构体引脚被分割（Split Struct Pin）后链接挂在子引脚上，
	// 不展开则 MovePinLinksToIntermediate 迁移不到链接，下游会静默读到默认值
	Super::ExpandNode(CompilerContext, SourceGraph);

	// 直接构建中间节点，并在完成引脚迁移后显式断开原节点链接。

	// 【最佳实践 3.1】：在开头检查所有必要的输入连接
	UEdGraphPin* MapPin = GetMapPin();
	UEdGraphPin* KeyPin = GetKeyPin();
	UEdGraphPin* ValuePin = GetValuePin();
	UEdGraphPin* FoundPin = GetFoundResultPin();

	if (!K2NodeHelpers::BeginExpandNode(
		CompilerContext,
		this,
		{MapPin, KeyPin, ValuePin, FoundPin},
		LOCTEXT("InvalidPins", "MapFind node has invalid pins @@")))
	{
		K2NodeHelpers::EndExpandNode(this);
		return;
	}

	// 【最佳实践 3.1】：Map 必须连接；Key 允许使用默认字面量
	// 【UE 最佳实践】用户输入错误使用 Warning 而非 Error，避免触发 EdGraphNode.h:563 断言崩溃
	if (MapPin->LinkedTo.Num() == 0)
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("MapNotConnected", "MapFindRef requires a Map connection @@").ToString(), this);
		K2NodeHelpers::EndExpandNode(this);
		return;
	}

	// 【最佳实践 3.2】：使用SpawnIntermediateNode创建中间节点
	UK2Node_CallFunction* CallFindNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFindNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Find), UBlueprintMapLibrary::StaticClass());
	CallFindNode->AllocateDefaultPins();

	// 【最佳实践 3.3】：使用MovePinLinksToIntermediate转移引脚连接
	UEdGraphPin* CallMapPin = CallFindNode->FindPinChecked(TEXT("TargetMap"), EGPD_Input);
	CallMapPin->PinType = MapPin->PinType;
	CompilerContext.MovePinLinksToIntermediate(*MapPin, *CallMapPin);

	UEdGraphPin* CallKeyPin = CallFindNode->FindPinChecked(TEXT("Key"), EGPD_Input);
	CallKeyPin->PinType = KeyPin->PinType;
	CallKeyPin->PinType.ContainerType = EPinContainerType::None;
	CompilerContext.MovePinLinksToIntermediate(*KeyPin, *CallKeyPin);

	// UBlueprintMapLibrary::Map_Find 始终将值复制到输出，不能伪造引用语义。
	UEdGraphPin* CallValuePin = CallFindNode->FindPinChecked(TEXT("Value"), EGPD_Output);
	CallValuePin->PinType = ValuePin->PinType;
	CallValuePin->PinType.ContainerType = EPinContainerType::None;
	CallValuePin->PinType.bIsReference = false;
	CompilerContext.MovePinLinksToIntermediate(*ValuePin, *CallValuePin);

	// 连接Found返回值
	UEdGraphPin* CallFoundPin = CallFindNode->GetReturnValuePin();
	CompilerContext.MovePinLinksToIntermediate(*FoundPin, *CallFoundPin);

	// 【最佳实践 3.4】：在末尾必须调用BreakAllNodeLinks清理节点
	K2NodeHelpers::EndExpandNode(this);
}

	UK2Node_MapFindRef::UK2Node_MapFindRef(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
		, bReturnByRefDesired(false)
	{
	}

	// 不再需要自定义Handler，使用ExpandNode方式
	// class FNodeHandlingFunctor* UK2Node_MapFindRef::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
	// {
	// 	return new FKCHandler_MapFindRef(CompilerContext);
	// }

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

	void UK2Node_MapFindRef::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
	{
		K2NodeHelpers::RegisterNode<UK2Node_MapFindRef>(ActionRegistrar);
	}

	void UK2Node_MapFindRef::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
	{
		Super::GetNodeContextMenuActions(Menu, Context);
	}

	FBlueprintNodeSignature UK2Node_MapFindRef::GetSignature() const
	{
		FBlueprintNodeSignature NodeSignature = Super::GetSignature();

		static const FName NodeRetByRefKey(TEXT("ReturnByRef"));
		NodeSignature.AddNamedValue(NodeRetByRefKey, TEXT("false"));

		return NodeSignature;
	}

	bool UK2Node_MapFindRef::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
	{
		(void)Filter;
		return false;
	}

	void UK2Node_MapFindRef::PostReconstructNode()
	{
		Super::PostReconstructNode();

		bReturnByRefDesired = false;
		if (UEdGraphPin* ValuePin = GetValuePin())
		{
			ValuePin->PinType.bIsReference = false;
		}
		PropagatePinType();
	}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

	void UK2Node_MapFindRef::AllocateDefaultPins()
	{
		UEdGraphNode::FCreatePinParams MapPinParams;
		MapPinParams.ContainerType = EPinContainerType::Map;
		UEdGraphPin* MapPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, TEXT("Map"), MapPinParams);
		MapPin->PinToolTip = LOCTEXT("MapPin_Tooltip", "要搜索的Map").ToString();
		
		UEdGraphPin* KeyPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, TEXT("Key"));
		KeyPin->PinToolTip = LOCTEXT("KeyPin_Tooltip", "要查找的键").ToString();

		UEdGraphNode::FCreatePinParams OutputPinParams;
		bReturnByRefDesired = false;
		OutputPinParams.bIsReference = false;
		UEdGraphPin* ValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, TEXT("Value"), OutputPinParams);
		ValuePin->PinToolTip = LOCTEXT("ValuePin_Tooltip", "找到的值（副本）").ToString();

		UEdGraphPin* FoundPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, TEXT("Found"));
		FoundPin->PinToolTip = LOCTEXT("FoundPin_Tooltip", "是否找到指定键").ToString();
	}

	void UK2Node_MapFindRef::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
	{
		Super::NotifyPinConnectionListChanged(Pin);

		PropagatePinType();
		if (UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyNodeChanged(this);
		}
	}

	bool UK2Node_MapFindRef::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
	{
		if (MyPin != GetKeyPin())
		{
			if (OtherPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				OutReason = LOCTEXT("NoExecWarning", "Cannot have an map of execution pins.").ToString();
				return true;
			}
		}
		return false;
	}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ReferenceHandling 

void UK2Node_MapFindRef::SetDesiredReturnType(bool bAsReference)
{
	(void)bAsReference;
	const bool bWasReference = IsSetToReturnRef();
	bReturnByRefDesired = false;
	if (bWasReference != bReturnByRefDesired && Pins.Num() > 0)
	{
		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
	}
}

void UK2Node_MapFindRef::PropagatePinType()
{
	UEdGraphPin* MapPin = GetMapPin();
	UEdGraphPin* KeyPin = GetKeyPin();
	UEdGraphPin* ValuePin = GetValuePin();
	if (MapPin && KeyPin && ValuePin)
	{
		ValuePin->PinType.bIsReference = false;

		const bool MapPinConnected = MapPin->LinkedTo.Num() > 0;
		const bool KeyPinConnected = KeyPin->LinkedTo.Num() > 0;
		const bool ValuePinConnected = ValuePin->LinkedTo.Num() > 0;

		UClass const* CallingContext = nullptr;
		if (UBlueprint const* Blueprint = GetBlueprint())
		{
			CallingContext = Blueprint->GeneratedClass;
			if (CallingContext == nullptr)
			{
				CallingContext = Blueprint->ParentClass;
			}
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		auto PropagatePinTypeToDestination = [Schema](UEdGraphPin* DestinationPin, const FEdGraphPinType& PinType)
			{

				if (DestinationPin->SubPins.Num() != 0 &&
					(
					DestinationPin->PinType.PinCategory != PinType.PinCategory ||
					DestinationPin->PinType.PinSubCategory != PinType.PinSubCategory ||
					DestinationPin->PinType.PinSubCategoryObject != PinType.PinSubCategoryObject)
					)
				{
					Schema->RecombinePin(DestinationPin->SubPins[0]);
				}

				DestinationPin->PinType.PinCategory = PinType.PinCategory;
				DestinationPin->PinType.PinSubCategory = PinType.PinSubCategory;
				DestinationPin->PinType.PinSubCategoryObject = PinType.PinSubCategoryObject;
			};

		auto PropagatePinTypeToDestinationTerminal = [Schema](UEdGraphPin* DestinationPin, const FEdGraphPinType& PinType)
			{
				if (DestinationPin->SubPins.Num() != 0 &&
					(
					DestinationPin->PinType.PinValueType.TerminalCategory != PinType.PinCategory ||
					DestinationPin->PinType.PinValueType.TerminalSubCategory != PinType.PinSubCategory ||
					DestinationPin->PinType.PinValueType.TerminalSubCategoryObject != PinType.PinSubCategoryObject)
					)
				{
					Schema->RecombinePin(DestinationPin->SubPins[0]);
				}

				DestinationPin->PinType.PinValueType.TerminalCategory = PinType.PinCategory;
				DestinationPin->PinType.PinValueType.TerminalSubCategory = PinType.PinSubCategory;
				DestinationPin->PinType.PinValueType.TerminalSubCategoryObject = PinType.PinSubCategoryObject;
			};

		auto PropagateTerminalPinTypeToDestination = [Schema](UEdGraphPin* DestinationPin, const FEdGraphTerminalType& PinType)
			{
				if (DestinationPin->SubPins.Num() != 0 &&
					(
					DestinationPin->PinType.PinCategory != PinType.TerminalCategory ||
					DestinationPin->PinType.PinSubCategory != PinType.TerminalSubCategory ||
					DestinationPin->PinType.PinSubCategoryObject != PinType.TerminalSubCategoryObject)
					)
				{
					Schema->RecombinePin(DestinationPin->SubPins[0]);
				}

				DestinationPin->PinType.PinCategory = PinType.TerminalCategory;
				DestinationPin->PinType.PinSubCategory = PinType.TerminalSubCategory;
				DestinationPin->PinType.PinSubCategoryObject = PinType.TerminalSubCategoryObject;
			};

		auto ResetPinToWildcardAndBreakAllLinks = [](UEdGraphPin* Pin)
			{
				if (Pin != nullptr)
				{
					Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
					Pin->PinType.PinSubCategory = NAME_None;
					Pin->PinType.PinSubCategoryObject = nullptr;

					Pin->BreakAllPinLinks();
				}
			};

		auto ResetPinTerminalType = [](UEdGraphPin* Pin)
			{
				if (Pin != nullptr)
				{
					Pin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
					Pin->PinType.PinValueType.TerminalSubCategory = NAME_None;
					Pin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
				}
			};


		if (MapPinConnected)
		{
			MapPin->PinType = MapPin->LinkedTo[0]->PinType;
		}
		else
		{
			ResetPinTerminalType(MapPin);
			ResetPinToWildcardAndBreakAllLinks(MapPin);
		}


		PropagatePinTypeToDestination(KeyPin, MapPin->PinType);

		PropagateTerminalPinTypeToDestination(ValuePin, MapPin->PinType.PinValueType);

		if (!MapPinConnected)
		{
			if (KeyPinConnected)
			{
				KeyPin->PinType = KeyPin->LinkedTo[0]->PinType;
				PropagatePinTypeToDestination(MapPin, KeyPin->PinType);
			}

			if (ValuePinConnected)
			{
				PropagatePinTypeToDestination(ValuePin, ValuePin->LinkedTo[0]->PinType);
				PropagatePinTypeToDestinationTerminal(MapPin, ValuePin->PinType);
			}
		}



		for (UEdGraphPin* Pin : Pins)
		{
			const TArray<UEdGraphPin*> LinkedPins = Pin->LinkedTo;
			for (UEdGraphPin* ConnectedPin : LinkedPins)
			{
				if (ConnectedPin && !Schema->ArePinsCompatible(Pin, ConnectedPin, CallingContext))
				{
					Pin->BreakLinkTo(ConnectedPin);
				}
				else if (ConnectedPin && ConnectedPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
				{
					if (UK2Node* ConnectedNode = Cast<UK2Node>(ConnectedPin->GetOwningNode()))
					{
						ConnectedNode->PinConnectionListChanged(ConnectedPin);
					}
				}
			}
		}

	}
}

bool UK2Node_MapFindRef::IsSetToReturnRef() const
{
	return false;
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE

