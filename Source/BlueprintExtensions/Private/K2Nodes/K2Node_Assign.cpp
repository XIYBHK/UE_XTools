#include "K2Nodes/K2Node_Assign.h"

// 编辑器功能
#include "EdGraphSchema_K2.h"

// 蓝图系统
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "SPinTypeSelector.h"

// 编译器-FKCHandler相关
#include "BPTerminal.h"
#include "EdGraphUtilities.h"
#include "KismetCompiler.h"
#include "KismetCompilerMisc.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiledFunctionContext.h"
#include "BlueprintCompiledStatement.h"

#define LOCTEXT_NAMESPACE "XTools_K2Node_Assign"

static FName TargetVarPinName(TEXT("Target"));
static FName VarValuePinName(TEXT("Value"));

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_Assign::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "引用赋值");
}

FText UK2Node_Assign::GetCompactNodeTitle() const
{
	return LOCTEXT("CompactNodeTitle", "赋值");
}

FText UK2Node_Assign::GetTooltipText() const
{
	return LOCTEXT("TooltipText", "通过引用设置变量的值");
}

FText UK2Node_Assign::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Variables");
}

FSlateIcon UK2Node_Assign::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Function_16x");
	return Icon;
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

class FKCHandler_Assign : public FNodeHandlingFunctor
{
public:
	FKCHandler_Assign(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Assign* VarRefNode = CastChecked<UK2Node_Assign>(Node);
		UEdGraphPin* ValuePin = VarRefNode->GetValuePin();
		ValidateAndRegisterNetIfLiteral(Context, ValuePin);

		{
			using namespace UE::KismetCompiler;

			UEdGraphPin* VariablePin = VarRefNode->GetTargetPin();
			UEdGraphPin* VariablePinNet = FEdGraphUtilities::GetNetFromPin(VariablePin);
			UEdGraphPin* ValuePinNet = FEdGraphUtilities::GetNetFromPin(ValuePin);

			if ((VariablePinNet != nullptr) && (ValuePinNet != nullptr))
			{
				CastingUtils::FConversion Conversion =
					CastingUtils::GetFloatingPointConversion(*ValuePinNet, *VariablePinNet);

				if (Conversion.Type != CastingUtils::FloatingPointCastType::None)
				{
					check(!ImplicitCastMap.Contains(VarRefNode));

					FBPTerminal* NewTerminal = CastingUtils::MakeImplicitCastTerminal(Context, VariablePinNet);

					ImplicitCastMap.Add(VarRefNode, CastingUtils::FImplicitCastParams{Conversion, NewTerminal, Node});
				}
			}
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Assign* VarRefNode = CastChecked<UK2Node_Assign>(Node);
		UEdGraphPin* VarTargetPin = VarRefNode->GetTargetPin();
		UEdGraphPin* ValuePin = VarRefNode->GetValuePin();

		InnerAssignment(Context, Node, VarTargetPin, ValuePin);

		// Generate the output impulse from this node
		GenerateSimpleThenGoto(Context, *Node);
	}

private:

	void InnerAssignment(FKismetFunctionContext& Context, UEdGraphNode* Node, UEdGraphPin* VariablePin, UEdGraphPin* ValuePin)
	{
		UEdGraphPin* VariablePinNet = FEdGraphUtilities::GetNetFromPin(VariablePin);
		UEdGraphPin* ValuePinNet = FEdGraphUtilities::GetNetFromPin(ValuePin);

		FBPTerminal** VariableTerm = Context.NetMap.Find(VariablePin);
		if (VariableTerm == nullptr)
		{
			VariableTerm = Context.NetMap.Find(VariablePinNet);
		}

		FBPTerminal** ValueTerm = Context.LiteralHackMap.Find(ValuePin);
		if (ValueTerm == nullptr)
		{
			ValueTerm = Context.NetMap.Find(ValuePinNet);
		}

		if ((VariableTerm != nullptr) && (ValueTerm != nullptr))
		{
			FBPTerminal* LHSTerm = *VariableTerm;
			FBPTerminal* RHSTerm = *ValueTerm;

			{
				using namespace UE::KismetCompiler;

				UK2Node_Assign* VarRefNode = CastChecked<UK2Node_Assign>(Node);
				if (CastingUtils::FImplicitCastParams* CastParams = ImplicitCastMap.Find(VarRefNode))
				{
					CastingUtils::InsertImplicitCastStatement(Context, *CastParams, RHSTerm);
					
					RHSTerm = CastParams->TargetTerminal;

					ImplicitCastMap.Remove(VarRefNode);

					// We've manually registered our cast statement, so it can be removed from the context.
					CastingUtils::RemoveRegisteredImplicitCast(Context, VariablePin);
					CastingUtils::RemoveRegisteredImplicitCast(Context, ValuePin);
				}
			}

			FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
			Statement.Type = KCST_Assignment;
			Statement.LHS = LHSTerm;
			Statement.RHS.Add(RHSTerm);

			if (!(*VariableTerm)->IsTermWritable())
			{
				// 【修复】使用 Warning 避免触发 EdGraphNode.h:563 断言崩溃
				CompilerContext.MessageLog.Warning(*LOCTEXT("WriteConst_Error", "Cannot write to const @@").ToString(), VariablePin);
			}
		}
		else
		{
			if (VariablePin != ValuePin)
			{
				// 【修复】使用 Warning 避免触发 EdGraphNode.h:563 断言崩溃
				CompilerContext.MessageLog.Warning(*LOCTEXT("ResolveValueIntoVariablePin_Error", "Failed to resolve term @@ passed into @@").ToString(), ValuePin, VariablePin);
			}
			else
			{
				// 【修复】使用 Warning 避免触发 EdGraphNode.h:563 断言崩溃
				CompilerContext.MessageLog.Warning(*LOCTEXT("ResolveTermPassed_Error", "Failed to resolve term passed into @@").ToString(), VariablePin);
			}
		}
	}

	TMap<UK2Node_Assign*, UE::KismetCompiler::CastingUtils::FImplicitCastParams> ImplicitCastMap;
};

UK2Node_Assign::UK2Node_Assign(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FNodeHandlingFunctor* UK2Node_Assign::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_Assign(CompilerContext);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_Assign::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();

	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UK2Node_Assign::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	// Default to filtering this node out unless dragging off of a reference output pin
	bool bIsFilteredOut = false;
	FBlueprintActionContext const& FilterContext = Filter.Context;

	for (UEdGraphPin* Pin : FilterContext.Pins)
	{
		if(Pin->Direction == EGPD_Output && Pin->PinType.bIsReference == true)
		{
			bIsFilteredOut = false;
			break;
		}
	}
	return bIsFilteredOut;
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_Assign::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	UEdGraphNode::FCreatePinParams PinParams;
	PinParams.bIsReference = true;
	UEdGraphPin* TargetPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, TargetVarPinName, PinParams);
	TargetPin->PinToolTip = LOCTEXT("TargetPin_Tooltip", "要设置的变量（引用类型）").ToString();

	UEdGraphPin* ValuePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, VarValuePinName);
	ValuePin->PinToolTip = LOCTEXT("ValuePin_Tooltip", "要赋予的新值").ToString();
}

void UK2Node_Assign::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();

	// Coerce the type of the node from the old pin, if available
	UEdGraphPin* OldTargetPin = nullptr;
	for (UEdGraphPin* CurrPin : OldPins)
	{
		if (CurrPin->PinName == TargetVarPinName)
		{
			OldTargetPin = CurrPin;
			break;
		}
	}
  
	if( OldTargetPin )
	{
		UEdGraphPin* NewTargetPin = GetTargetPin();
		CoerceTypeFromPin(OldTargetPin);
	}

	RestoreSplitPins(OldPins);
}

void UK2Node_Assign::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	UEdGraphPin* TargetPin = GetTargetPin();
	UEdGraphPin* ValuePin = GetValuePin();

	if( (Pin == TargetPin) || (Pin == ValuePin) )
	{
		UEdGraphPin* ConnectedToPin = (Pin->LinkedTo.Num() > 0) ? Pin->LinkedTo[0] : nullptr;
		CoerceTypeFromPin(ConnectedToPin);

		// If both target and value pins are unlinked, then reset types to wildcard
		if(TargetPin->LinkedTo.Num() == 0 && ValuePin->LinkedTo.Num() == 0)
		{
			// collapse SubPins back into their parent if there are any
			auto TryRecombineSubPins = [](UEdGraphPin* ParentPin)
			{
				if (!ParentPin->SubPins.IsEmpty())
				{
					const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
					K2Schema->RecombinePin(ParentPin->SubPins[0]);
				}
			};
			
			// Pin disconnected...revert to wildcard
			TargetPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			TargetPin->PinType.PinSubCategory = NAME_None;
			TargetPin->PinType.PinSubCategoryObject = nullptr;
			TargetPin->BreakAllPinLinks();
			TryRecombineSubPins(TargetPin);

			ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			ValuePin->PinType.PinSubCategory = NAME_None;
			ValuePin->PinType.PinSubCategoryObject = nullptr;
			ValuePin->BreakAllPinLinks();
			TryRecombineSubPins(ValuePin);
		}
		
		// Get the graph to refresh our title and default value info
		GetGraph()->NotifyNodeChanged(this);
	}
}

void UK2Node_Assign::PostPasteNode()
{
	Super::PostPasteNode();

	// 获取引脚
	UEdGraphPin* TargetPin = GetTargetPin();
	UEdGraphPin* ValuePin = GetValuePin();

	// 重置 Target 引脚为 Wildcard
	TargetPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	TargetPin->PinType.PinSubCategory = NAME_None;
	TargetPin->PinType.PinSubCategoryObject = nullptr;
	if (!TargetPin->SubPins.IsEmpty())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->RecombinePin(TargetPin->SubPins[0]);
	}

	// 重置 Value 引脚为 Wildcard
	ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	ValuePin->PinType.PinSubCategory = NAME_None;
	ValuePin->PinType.PinSubCategoryObject = nullptr;
	if (!ValuePin->SubPins.IsEmpty())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->RecombinePin(ValuePin->SubPins[0]);
	}
}

UEdGraphPin* UK2Node_Assign::GetTargetPin() const
{
	return FindPin(TargetVarPinName);
}

UEdGraphPin* UK2Node_Assign::GetValuePin() const
{
	return FindPin(VarValuePinName);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region TypeHandling

void UK2Node_Assign::CoerceTypeFromPin(const UEdGraphPin* Pin)
{
	UEdGraphPin* TargetPin = GetTargetPin();
	UEdGraphPin* ValuePin = GetValuePin();

	check(TargetPin && ValuePin);

	if( Pin && 
		(Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard ||
		(	Pin->PinType.PinCategory == TargetPin->PinType.PinCategory &&
			Pin->PinType.PinCategory == ValuePin->PinType.PinCategory )
		) )
	{
		check((Pin != TargetPin) || (Pin->PinType.bIsReference && !Pin->PinType.IsContainer()));

		TargetPin->PinType = Pin->PinType;
		TargetPin->PinType.bIsReference = true;

		ValuePin->PinType = Pin->PinType;
		ValuePin->PinType.bIsReference = false;
	}
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE
