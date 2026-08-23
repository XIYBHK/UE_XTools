/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "KismetCompiler.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"

/**
 * K2Node 辅助函数命名空间
 * 提供自定义蓝图节点的共享辅助函数
 * 遵循 UE 最佳实践：仅在 Private 目录中使用，避免暴露给外部
 */
namespace K2NodeHelpers
{
	/**
	 * 统一连接入口：通过 Schema 校验连接合法性
	 */
	FORCEINLINE bool TryConnect(FKismetCompilerContext& CompilerContext, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
	{
		if (!SourcePin || !TargetPin)
		{
			// 【修复】使用 Warning 而非 Error：避免触发 EdGraphNode.h:563 断言崩溃
			CompilerContext.MessageLog.Warning(TEXT("K2Node internal connection failed: null pin."));
			return false;
		}

		const UEdGraphSchema* Schema = CompilerContext.GetSchema();
		const bool bConnected = Schema && Schema->TryCreateConnection(SourcePin, TargetPin);
		if (!bConnected)
		{
			UEdGraphNode* ErrorNode = SourcePin->GetOwningNode() ? SourcePin->GetOwningNode() : TargetPin->GetOwningNode();
			const FString Message = FString::Printf(
				TEXT("K2Node internal connection failed: %s.%s -> %s.%s @@"),
				SourcePin->GetOwningNode() ? *SourcePin->GetOwningNode()->GetName() : TEXT("<None>"),
				*SourcePin->PinName.ToString(),
				TargetPin->GetOwningNode() ? *TargetPin->GetOwningNode()->GetName() : TEXT("<None>"),
				*TargetPin->PinName.ToString());

			if (ErrorNode)
			{
				// 【修复】使用 Warning 而非 Error：避免触发 EdGraphNode.h:563 断言崩溃
				CompilerContext.MessageLog.Warning(*Message, ErrorNode);
			}
			else
			{
				CompilerContext.MessageLog.Warning(*Message);
			}
		}

		return bConnected;
	}

	/**
	 * 重建中间节点后安全地重新获取 Pin
	 */
	FORCEINLINE UEdGraphPin* ReconstructAndFindPin(UK2Node* Node, const FName& PinName, EEdGraphPinDirection Direction = EGPD_MAX)
	{
		if (!Node)
		{
			return nullptr;
		}

		Node->PostReconstructNode();
		return (Direction == EGPD_MAX) ? Node->FindPin(PinName) : Node->FindPin(PinName, Direction);
	}

	/**
	 * ExpandNode 标准入口：统一必要引脚校验
	 */
	FORCEINLINE bool BeginExpandNode(FKismetCompilerContext& CompilerContext, UK2Node* Node, std::initializer_list<UEdGraphPin*> RequiredPins, const FText& ErrorMessage)
	{
		for (UEdGraphPin* RequiredPin : RequiredPins)
		{
			if (!RequiredPin)
			{
				// 用 Warning 而非 Error：与项目 K2Node 规范一致，避免硬性阻断蓝图编译；调用方据返回值终止展开
				CompilerContext.MessageLog.Warning(*ErrorMessage.ToString(), Node);
				return false;
			}
		}

		return true;
	}

	/**
	 * ExpandNode 标准收尾：断开原节点所有链接
	 */
	FORCEINLINE void EndExpandNode(UK2Node* Node)
	{
		if (Node)
		{
			Node->BreakAllNodeLinks();
		}
	}

	/**
	 * CopyPinLinksToIntermediate 的增强版：同时搬运默认值。
	 * 引擎的 CopyPinLinks 只搬链接（与 MovePinLinks 不同，不搬 DefaultValue/DefaultObject/DefaultTextValue），
	 * 输入引脚未连接时中间节点会退回函数参数默认值，与源引脚上的默认值/用户手输值产生语义分叉
	 * （例如 Delay 引脚默认 0.1 未连接时，Delay<=0 判定引脚仍是 0.0，导致直通无延迟）。
	 */
	FORCEINLINE FPinConnectionResponse CopyPinLinksAndDefaultToIntermediate(
		FKismetCompilerContext& CompilerContext, UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin)
	{
		FPinConnectionResponse Response = CompilerContext.CopyPinLinksToIntermediate(SourcePin, IntermediatePin);
		IntermediatePin.DefaultValue = SourcePin.DefaultValue;
		IntermediatePin.DefaultObject = SourcePin.DefaultObject;
		IntermediatePin.DefaultTextValue = SourcePin.DefaultTextValue;
		return Response;
	}

	struct FSingleFlightExecutionGuard
	{
		UEdGraphPin* EntryExecPin = nullptr;
		UEdGraphPin* StartThenPin = nullptr;
		UEdGraphPin* FinishExecPin = nullptr;
		UEdGraphPin* FinishThenPin = nullptr;
	};

	/**
	 * 为事件图 Latent 节点创建单实例运行守卫。
	 * 同一节点运行期间的重复入口会被忽略，避免共享事件图临时变量与固定 Latent UUID 相互覆盖。
	 */
	FORCEINLINE FSingleFlightExecutionGuard CreateSingleFlightExecutionGuard(
		FKismetCompilerContext& CompilerContext,
		UK2Node* SourceNode,
		UEdGraph* SourceGraph)
	{
		UK2Node_TemporaryVariable* RunningFlag =
			CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(SourceNode, SourceGraph);
		RunningFlag->VariableType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		RunningFlag->AllocateDefaultPins();
		UEdGraphPin* RunningFlagPin = RunningFlag->GetVariablePin();

		UK2Node_IfThenElse* EntryBranch =
			CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(SourceNode, SourceGraph);
		EntryBranch->AllocateDefaultPins();
		TryConnect(CompilerContext, RunningFlagPin, EntryBranch->GetConditionPin());

		UK2Node_AssignmentStatement* StartRunning =
			CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(SourceNode, SourceGraph);
		StartRunning->AllocateDefaultPins();
		StartRunning->GetValuePin()->DefaultValue = TEXT("true");
		TryConnect(CompilerContext, EntryBranch->GetElsePin(), StartRunning->GetExecPin());
		TryConnect(CompilerContext, RunningFlagPin, StartRunning->GetVariablePin());

		UK2Node_AssignmentStatement* FinishRunning =
			CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(SourceNode, SourceGraph);
		FinishRunning->AllocateDefaultPins();
		FinishRunning->GetValuePin()->DefaultValue = TEXT("false");
		TryConnect(CompilerContext, RunningFlagPin, FinishRunning->GetVariablePin());

		return {
			EntryBranch->GetExecPin(),
			StartRunning->GetThenPin(),
			FinishRunning->GetExecPin(),
			FinishRunning->GetThenPin()
		};
	}

	/**
	 * 检查引脚是否为 Wildcard 类型
	 * @param Pin 要检查的引脚
	 * @return 如果引脚是 Wildcard 类型返回 true
	 */
	FORCEINLINE bool IsWildcardPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard;
	}

	/**
	 * 重置引脚为 Wildcard 类型（保留原有链接）
	 * @param Pin 要重置的引脚
	 */
	FORCEINLINE void ResetPinToWildcard(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return;
		}

		Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		Pin->PinType.PinSubCategory = NAME_None;
		Pin->PinType.PinSubCategoryObject = nullptr;
		Pin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
		Pin->PinType.PinValueType.TerminalSubCategory = NAME_None;
		Pin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;
		Pin->BreakAllPinLinks(true);
	}

	/**
	 * 从源引脚复制类型信息到目标引脚
	 * @param SourcePin 源引脚
	 * @param TargetPin 目标引脚
	 * @param bCopyContainerType 是否复制容器类型
	 */
	FORCEINLINE void CopyPinType(const UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, bool bCopyContainerType = true)
	{
		if (!SourcePin || !TargetPin)
		{
			return;
		}

		TargetPin->PinType.PinCategory = SourcePin->PinType.PinCategory;
		TargetPin->PinType.PinSubCategory = SourcePin->PinType.PinSubCategory;
		TargetPin->PinType.PinSubCategoryObject = SourcePin->PinType.PinSubCategoryObject;

		if (bCopyContainerType)
		{
			TargetPin->PinType.ContainerType = SourcePin->PinType.ContainerType;
		}

		TargetPin->PinType.PinValueType.TerminalCategory = SourcePin->PinType.PinValueType.TerminalCategory;
		TargetPin->PinType.PinValueType.TerminalSubCategory = SourcePin->PinType.PinValueType.TerminalSubCategory;
		TargetPin->PinType.PinValueType.TerminalSubCategoryObject = SourcePin->PinType.PinValueType.TerminalSubCategoryObject;
	}

	/**
	 * 检查图是否支持事件图（宏图/蓝图编辑器）
	 * @param TargetGraph 目标图
	 * @return 如果支持返回 true
	 */
	FORCEINLINE bool IsEventGraphCompatible(const UEdGraph* TargetGraph)
	{
		if (!TargetGraph)
		{
			return false;
		}

		const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		return Blueprint && FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
	}

	/**
	 * 检查图是否允许 Latent 节点。
	 * 当前自定义 Loop 节点会在 ExpandNode 时才生成 Delay 调用，UE 的宏图 Latent 扫描无法提前识别。
	 * 因此只允许事件图，避免宏图再被函数图间接调用时绕过官方 Latent 过滤。
	 */
	FORCEINLINE bool IsLatentGraphCompatible(const UEdGraph* TargetGraph)
	{
		if (!TargetGraph || !TargetGraph->GetSchema())
		{
			return false;
		}

		const EGraphType GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
		return GraphType == EGraphType::GT_Ubergraph;
	}

	/**
	 * 编译阶段按原始节点所属图检查 Latent 兼容性。
	 * 事件图会先被克隆到临时 ConsolidatedEventGraph，直接判断编译图会被误识别为函数图。
	 */
	FORCEINLINE bool IsLatentNodeGraphCompatible(
		const FKismetCompilerContext& CompilerContext,
		const UK2Node* Node,
		const UEdGraph* CompiledGraph)
	{
		const UK2Node* SourceNode = Node
			? Cast<UK2Node>(CompilerContext.MessageLog.FindSourceObject(Node))
			: nullptr;
		const UEdGraph* AuthoredGraph = SourceNode ? SourceNode->GetGraph() : CompiledGraph;
		return IsLatentGraphCompatible(AuthoredGraph);
	}

	/**
	 * 注册自定义节点到蓝图菜单
	 * @param ActionRegistrar 动作注册器
	 */
	template<typename NodeType>
	void RegisterNode(FBlueprintActionDatabaseRegistrar& ActionRegistrar)
	{
		UClass* ActionKey = NodeType::StaticClass();
		if (ActionRegistrar.IsOpenForRegistration(ActionKey))
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(NodeType::StaticClass());
			if (ensureMsgf(NodeSpawner != nullptr, TEXT("RegisterNode: UBlueprintNodeSpawner::Create 返回空，节点注册跳过")))
			{
				ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
			}
		}
	}
}
