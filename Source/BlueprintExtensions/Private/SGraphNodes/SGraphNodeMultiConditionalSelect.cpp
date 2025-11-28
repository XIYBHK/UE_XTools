/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 * 
 * Based on AdvancedControlFlow by Colory Games (MIT License)
 * https://github.com/colory-games/UEPlugin-AdvancedControlFlow
 */

#include "SGraphNodes/SGraphNodeMultiConditionalSelect.h"

#include "K2Nodes/K2Node_MultiConditionalSelect.h"
#include "NodeFactory.h"

void SGraphNodeMultiConditionalSelect::Construct(const FArguments& InArgs, UK2Node_MultiConditionalSelect* InNode)
{
	this->GraphNode = InNode;
	this->SetCursor(EMouseCursor::CardinalCross);
	this->UpdateGraphNode();
}

void SGraphNodeMultiConditionalSelect::CreatePinWidgets()
{
	UK2Node_MultiConditionalSelect* MultiConditionalSelect = CastChecked<UK2Node_MultiConditionalSelect>(GraphNode);

	for (auto It = GraphNode->Pins.CreateConstIterator(); It; ++It)
	{
		UEdGraphPin* Pin = *It;
		if (!Pin->bHidden)
		{
			TSharedPtr<SGraphPin> NewPin = FNodeFactory::CreatePinWidget(Pin);
			check(NewPin.IsValid());

			this->AddPin(NewPin.ToSharedRef());
		}
	}
}
