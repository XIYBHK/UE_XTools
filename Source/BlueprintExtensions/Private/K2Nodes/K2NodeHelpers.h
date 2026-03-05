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
			return false;
		}

		const UEdGraphSchema* Schema = CompilerContext.GetSchema();
		return Schema && Schema->TryCreateConnection(SourcePin, TargetPin);
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
				CompilerContext.MessageLog.Error(*ErrorMessage.ToString(), Node);
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
			check(NodeSpawner != nullptr);
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
}
