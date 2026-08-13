#include "K2Node_SplineMoveAlong.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "KismetCompiler.h"
#include "SplineMoveAlongAction.h"

#define LOCTEXT_NAMESPACE "K2Node_SplineMoveAlong"

namespace SplineMoveAlongNodeNames
{
	const FName Interrupt(TEXT("Interrupt"));
	const FName TargetLocation(TEXT("TargetLocation"));
	const FName DistanceAlongSpline(TEXT("DistanceAlongSpline"));
}

UK2Node_SplineMoveAlong::UK2Node_SplineMoveAlong(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(USplineMoveAlongAction, SplineMoveAlong);
	ProxyFactoryClass = USplineMoveAlongAction::StaticClass();
	ProxyClass = USplineMoveAlongAction::StaticClass();
	ProxyActivateFunctionName = GET_FUNCTION_NAME_CHECKED(UBlueprintAsyncActionBase, Activate);
}

void UK2Node_SplineMoveAlong::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	UEdGraphPin* InterruptPin = CreatePin(
		EGPD_Input,
		UEdGraphSchema_K2::PC_Exec,
		SplineMoveAlongNodeNames::Interrupt);
	InterruptPin->PinFriendlyName = LOCTEXT("InterruptPin", "中断");

	const int32 ExecutePinIndex = Pins.IndexOfByPredicate([](const UEdGraphPin* Pin)
	{
		return Pin
			&& Pin->Direction == EGPD_Input
			&& Pin->PinName == UEdGraphSchema_K2::PN_Execute;
	});
	if (ExecutePinIndex != INDEX_NONE)
	{
		Pins.Remove(InterruptPin);
		Pins.Insert(InterruptPin, ExecutePinIndex + 1);
	}

	UEdGraphPin* ProxyPin = GetProxyPin();
	if (!ProxyPin)
	{
		ProxyPin = CreatePin(
			EGPD_Output,
			UEdGraphSchema_K2::PC_Object,
			USplineMoveAlongAction::StaticClass(),
			FBaseAsyncTaskHelper::GetAsyncTaskProxyName());
	}
	ProxyPin->bHidden = true;

	UEdGraphPin* TargetLocationPin = FindPin(SplineMoveAlongNodeNames::TargetLocation, EGPD_Output);
	if (!TargetLocationPin)
	{
		TargetLocationPin = CreatePin(
			EGPD_Output,
			UEdGraphSchema_K2::PC_Struct,
			TBaseStructure<FVector>::Get(),
			SplineMoveAlongNodeNames::TargetLocation);
	}
	TargetLocationPin->PinFriendlyName = LOCTEXT("TargetLocationPin", "当前前往位置");

	UEdGraphPin* DistanceAlongSplinePin = FindPin(SplineMoveAlongNodeNames::DistanceAlongSpline, EGPD_Output);
	if (!DistanceAlongSplinePin)
	{
		DistanceAlongSplinePin = CreatePin(
			EGPD_Output,
			UEdGraphSchema_K2::PC_Real,
			UEdGraphSchema_K2::PC_Float,
			SplineMoveAlongNodeNames::DistanceAlongSpline);
	}
	DistanceAlongSplinePin->PinFriendlyName = LOCTEXT("DistanceAlongSplinePin", "样条线距离");
}

void UK2Node_SplineMoveAlong::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	UEdGraphPin* InterruptPin = GetInterruptPin();
	UEdGraphPin* ProxyPin = GetProxyPin();

	if (!Schema || !InterruptPin || !ProxyPin)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("MissingInternalPins", "@@ 缺少中断展开所需的内部引脚。").ToString(),
			this);
		BreakAllNodeLinks();
		return;
	}

	UFunction* InterruptFunction = USplineMoveAlongAction::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(USplineMoveAlongAction, Interrupt));
	if (!InterruptFunction)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("MissingInterruptFunction", "@@ 找不到中断样条线移动函数。").ToString(),
			this);
		BreakAllNodeLinks();
		return;
	}

	UK2Node_CallFunction* CallInterruptNode =
		CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallInterruptNode->SetFromFunction(InterruptFunction);
	CallInterruptNode->AllocateDefaultPins();

	UEdGraphPin* InterruptExecPin = CallInterruptNode->GetExecPin();
	UEdGraphPin* InterruptSelfPin = Schema->FindSelfPin(*CallInterruptNode, EGPD_Input);
	const bool bMovedInterruptExec = InterruptExecPin
		&& CompilerContext.MovePinLinksToIntermediate(*InterruptPin, *InterruptExecPin).CanSafeConnect();
	const bool bConnectedProxy = InterruptSelfPin
		&& Schema->TryCreateConnection(ProxyPin, InterruptSelfPin);

	if (!bMovedInterruptExec || !bConnectedProxy)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("InterruptConnectionFailed", "@@ 无法生成中断样条线移动调用。").ToString(),
			this);
		BreakAllNodeLinks();
		return;
	}

	// 父类会把隐藏 Proxy 引脚的连接迁移到工厂返回值，并完成委托绑定与 Activate 调用。
	Super::ExpandNode(CompilerContext, SourceGraph);
}

void UK2Node_SplineMoveAlong::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_SplineMoveAlong::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "XTools|样条线|移动");
}

FText UK2Node_SplineMoveAlong::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "沿样条线移动");
}

FText UK2Node_SplineMoveAlong::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "驱动角色沿样条线移动，并可通过中断执行引脚停止当前移动。");
}

FText UK2Node_SplineMoveAlong::GetKeywords() const
{
	return LOCTEXT("Keywords", "样条 spline 移动 路径 follow 中断 interrupt");
}

UEdGraphPin* UK2Node_SplineMoveAlong::GetInterruptPin() const
{
	return FindPin(SplineMoveAlongNodeNames::Interrupt, EGPD_Input);
}

UEdGraphPin* UK2Node_SplineMoveAlong::GetProxyPin() const
{
	return FindPin(FBaseAsyncTaskHelper::GetAsyncTaskProxyName(), EGPD_Output);
}

#undef LOCTEXT_NAMESPACE
