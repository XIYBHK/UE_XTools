#include "K2Nodes/K2Node_MapFindRef.h"

// 编辑器功能
#include "EdGraphSchema_K2.h"

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
	return LOCTEXT("NodeTitle", "查找引用");
}

FText UK2Node_MapFindRef::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "查找引用");
}

FText UK2Node_MapFindRef::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "根据键查找Map中的项并返回引用\n可以直接操作该项，修改会反映到Map中");
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
	Super::ExpandNode(CompilerContext, SourceGraph);

	// 【最佳实践 3.1】：在开头检查所有必要的输入连接
	UEdGraphPin* MapPin = GetMapPin();
	UEdGraphPin* KeyPin = GetKeyPin();
	UEdGraphPin* ValuePin = GetValuePin();
	UEdGraphPin* FoundPin = GetFoundResultPin();

	// 【最佳实践 5.2】：空指针检查
	if (!MapPin || !KeyPin || !ValuePin || !FoundPin)
			{
		// 【最佳实践 4.3】：使用LOCTEXT本地化错误信息
		CompilerContext.MessageLog.Error(*LOCTEXT("InvalidPins", "MapFindRef node has invalid pins @@").ToString(), this);
		// 【最佳实践 3.1】：错误后必须调用BreakAllNodeLinks
		BreakAllNodeLinks();
					return;
				}

	// 【最佳实践 3.1】：验证必要输入
	if (MapPin->LinkedTo.Num() == 0)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MapNotConnected", "MapFindRef requires a Map connection @@").ToString(), this);
		BreakAllNodeLinks();
							return;
						}

	if (KeyPin->LinkedTo.Num() == 0)
			{
		CompilerContext.MessageLog.Error(*LOCTEXT("KeyNotConnected", "MapFindRef requires a Key connection @@").ToString(), this);
		BreakAllNodeLinks();
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

	// 处理Value输出引脚（支持引用返回）
	UEdGraphPin* CallValuePin = CallFindNode->FindPinChecked(TEXT("Value"), EGPD_Output);
	CallValuePin->PinType = ValuePin->PinType;
	CallValuePin->PinType.ContainerType = EPinContainerType::None;
	CallValuePin->PinType.bIsReference = IsSetToReturnRef();
	CompilerContext.MovePinLinksToIntermediate(*ValuePin, *CallValuePin);

	// 连接Found返回值
	UEdGraphPin* CallFoundPin = CallFindNode->GetReturnValuePin();
	CompilerContext.MovePinLinksToIntermediate(*FoundPin, *CallFoundPin);

	// 【最佳实践 3.4】：在末尾必须调用BreakAllNodeLinks清理节点
	BreakAllNodeLinks();
}

	namespace K2Node_MapFindRef_Impl
	{
		static bool SupportsReturnByRef(const FEdGraphPinType& PinType)
		{
			return !(PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
				PinType.PinCategory == UEdGraphSchema_K2::PC_Class ||
				PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject ||
				PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass ||
				PinType.PinCategory == UEdGraphSchema_K2::PC_Interface);
		}

		static bool SupportsReturnByRef(const UK2Node_MapFindRef* Node)
		{
			return (Node->Pins.Num() == 0) || SupportsReturnByRef(Node->GetMapPin()->PinType);
		}

		static FText GetToggleTooltip(bool bIsOutputRef)
		{
			return bIsOutputRef ? LOCTEXT("ConvToValTooltip", "Changing this node to return a copy will make it so it returns a temporary duplicate of the item in the map (changes to this item will NOT be propagated back to the map)") :
				LOCTEXT("ConvToRefTooltip", "Changing this node to return by reference will make it so it returns the same item that's in the map (meaning you can operate directly on that item, and changes will be reflected in the map)");
		}
	}

	UK2Node_MapFindRef::UK2Node_MapFindRef(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
		, bReturnByRefDesired(true)
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

		UClass* ActionKey = GetClass();

		if (ActionRegistrar.IsOpenForRegistration(ActionKey))
		{
			UBlueprintNodeSpawner* RetRefNodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			check(RetRefNodeSpawner != nullptr);
			ActionRegistrar.AddBlueprintAction(ActionKey, RetRefNodeSpawner);
		}
	}

	void UK2Node_MapFindRef::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
	{
		Super::GetNodeContextMenuActions(Menu, Context);

		const bool bReturnIsRef = IsSetToReturnRef();
		FText ToggleTooltip = K2Node_MapFindRef_Impl::GetToggleTooltip(bReturnIsRef);

		const bool bCannotReturnRef = !bReturnIsRef && bReturnByRefDesired;
		if (bCannotReturnRef)
		{
			UEdGraphPin* OutputPin = GetValuePin();
			ToggleTooltip = FText::Format(LOCTEXT("CannotToggleTooltip", "Cannot return by ref using '{0}' pins"), UEdGraphSchema_K2::TypeToText(OutputPin->PinType));
		}

		{
			FToolMenuSection& Section = Menu->AddSection("Map", LOCTEXT("MapHeader", "Map Find Out Ref Node"));
			Section.AddMenuEntry(
				"ToggleReturnPin",
				bReturnIsRef ? LOCTEXT("ChangeNodeToRef", "Change to return a copy") : LOCTEXT("ChangeNodeToVal", "Change to return a reference"),
				ToggleTooltip,
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_MapFindRef*>(this), &UK2Node_MapFindRef::ToggleReturnPin),
					FCanExecuteAction::CreateLambda([bCannotReturnRef]()->bool { return !bCannotReturnRef; }))
			);
		}
	}

	FBlueprintNodeSignature UK2Node_MapFindRef::GetSignature() const
	{
		FBlueprintNodeSignature NodeSignature = Super::GetSignature();

		static const FName NodeRetByRefKey(TEXT("ReturnByRef"));
		NodeSignature.AddNamedValue(NodeRetByRefKey, IsSetToReturnRef() ? TEXT("true") : TEXT("false"));

		return NodeSignature;
	}

	bool UK2Node_MapFindRef::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
	{
		bool bIsFilteredOut = false;
		for (UEdGraphPin* Pin : Filter.Context.Pins)
		{
			if (bReturnByRefDesired && !K2Node_MapFindRef_Impl::SupportsReturnByRef(Pin->PinType))
			{
				bIsFilteredOut = true;
				break;
			}
		}
		return bIsFilteredOut;
	}

	void UK2Node_MapFindRef::PostReconstructNode()
	{
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
		OutputPinParams.bIsReference = bReturnByRefDesired;
		UEdGraphPin* ValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, TEXT("Value"), OutputPinParams);
		ValuePin->PinToolTip = LOCTEXT("ValuePin_Tooltip", "找到的值（引用类型）").ToString();
	}

	void UK2Node_MapFindRef::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
	{
		Super::NotifyPinConnectionListChanged(Pin);

		PropagatePinType();
		GetGraph()->NotifyNodeChanged(this);
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
			else if (IsSetToReturnRef() && !K2Node_MapFindRef_Impl::SupportsReturnByRef(OtherPin->PinType))
			{
				OutReason = LOCTEXT("ConnectionWillChangeNodeToVal", "Change the Get node to return a copy").ToString();
				return false;
			}
		}
		return false;
	}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ReferenceHandling 

void UK2Node_MapFindRef::SetDesiredReturnType(bool bAsReference)
{
	if (bReturnByRefDesired != bAsReference)
	{
		bReturnByRefDesired = bAsReference;

		const bool bReconstruct = (Pins.Num() > 0) && (IsSetToReturnRef() == bAsReference);
		if (bReconstruct)
		{
			ReconstructNode();
			FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
		}
	}
}

void UK2Node_MapFindRef::ToggleReturnPin()
{
	FText TransactionTitle;
	if (bReturnByRefDesired)
	{
		TransactionTitle = LOCTEXT("ToggleToVal", "Change to return a copy");
	}
	else
	{
		TransactionTitle = LOCTEXT("ToggleToRef", "Change to return a reference");
	}
	const FScopedTransaction Transaction(TransactionTitle);
	Modify();

	SetDesiredReturnType(!bReturnByRefDesired);
}

void UK2Node_MapFindRef::PropagatePinType()
{
	UEdGraphPin* MapPin = GetMapPin();
	UEdGraphPin* KeyPin = GetKeyPin();
	UEdGraphPin* ValuePin = GetValuePin();

	const bool MapPinConnected = MapPin->LinkedTo.Num() > 0;
	const bool KeyPinConnected = KeyPin->LinkedTo.Num() > 0;
	const bool ValuePinConnected = ValuePin->LinkedTo.Num() > 0;

	if (MapPin && KeyPin && ValuePin)
	{
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
			for (TArray<UEdGraphPin*>::TIterator ConnectionIt(Pin->LinkedTo); ConnectionIt; ++ConnectionIt)
			{
				UEdGraphPin* ConnectedPin = *ConnectionIt;
				if (!Schema->ArePinsCompatible(Pin, ConnectedPin, CallingContext))
				{
					Pin->BreakLinkTo(ConnectedPin);
				}
				else if (ConnectedPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
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
	return bReturnByRefDesired && K2Node_MapFindRef_Impl::SupportsReturnByRef(this);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE

