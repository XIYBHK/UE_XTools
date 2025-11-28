/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "BlueprintExtensions.h"

#include "EdGraphUtilities.h"
#include "K2Nodes/K2Node_MultiBranch.h"
#include "K2Nodes/K2Node_ConditionalSequence.h"
#include "K2Nodes/K2Node_MultiConditionalSelect.h"
#include "K2Nodes/K2Node_SafeCastChain.h"
#include "SGraphNodes/SGraphNodeMultiBranch.h"
#include "SGraphNodes/SGraphNodeConditionalSequence.h"
#include "SGraphNodes/SGraphNodeMultiConditionalSelect.h"
#include "SGraphNodes/SGraphNodeSafeCastChain.h"

#define LOCTEXT_NAMESPACE "FBlueprintExtensionsModule"

DEFINE_LOG_CATEGORY(LogBlueprintExtensions);

/**
 * 自定义节点工厂，用于创建 AdvancedControlFlow 系列节点的可视化组件
 */
class FGraphPanelNodeFactory_BlueprintExtensions : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UK2Node_MultiBranch* MultiBranch = Cast<UK2Node_MultiBranch>(Node))
		{
			return SNew(SGraphNodeMultiBranch, MultiBranch);
		}
		else if (UK2Node_ConditionalSequence* ConditionalSequence = Cast<UK2Node_ConditionalSequence>(Node))
		{
			return SNew(SGraphNodeConditionalSequence, ConditionalSequence);
		}
		else if (UK2Node_MultiConditionalSelect* MultiConditionalSelect = Cast<UK2Node_MultiConditionalSelect>(Node))
		{
			return SNew(SGraphNodeMultiConditionalSelect, MultiConditionalSelect);
		}
		else if (UK2Node_SafeCastChain* SafeCastChain = Cast<UK2Node_SafeCastChain>(Node))
		{
			return SNew(SGraphNodeSafeCastChain, SafeCastChain);
		}

		return nullptr;
	}
};

void FBlueprintExtensionsModule::StartupModule()
{
	// 注册自定义节点工厂
	GraphPanelNodeFactory = MakeShareable(new FGraphPanelNodeFactory_BlueprintExtensions());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory);

	UE_LOG(LogBlueprintExtensions, Log, TEXT("BlueprintExtensions module started"));
}

void FBlueprintExtensionsModule::ShutdownModule()
{
	// 注销自定义节点工厂
	if (GraphPanelNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory);
		GraphPanelNodeFactory.Reset();
	}

	UE_LOG(LogBlueprintExtensions, Log, TEXT("BlueprintExtensions module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintExtensionsModule, BlueprintExtensions)

