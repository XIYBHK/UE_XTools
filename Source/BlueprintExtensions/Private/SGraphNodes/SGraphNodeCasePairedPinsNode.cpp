/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 * 
 * Based on AdvancedControlFlow by Colory Games (MIT License)
 * https://github.com/colory-games/UEPlugin-AdvancedControlFlow
 */

#include "SGraphNodes/SGraphNodeCasePairedPinsNode.h"

#include "DetailLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "GraphEditorSettings.h"
#include "K2Nodes/K2Node_CasePairedPinsNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

void SGraphNodeCasePairedPinsNode::Construct(const FArguments& InArgs, UK2Node_CasePairedPinsNode* InNode)
{
	this->GraphNode = InNode;
	this->SetCursor(EMouseCursor::CardinalCross);
	this->UpdateGraphNode();
}

void SGraphNodeCasePairedPinsNode::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	UK2Node_CasePairedPinsNode* CasePairedPinsNode = CastChecked<UK2Node_CasePairedPinsNode>(GraphNode);

// Add pin button
	TSharedRef<SWidget> AddPinButton =
		AddPinButtonContent(FText::AsCultureInvariant("Add pin"), FText::AsCultureInvariant("Add new pin"));

	FMargin AddPinPadding = Settings->GetOutputPinPadding();
	AddPinPadding.Top += 6.0f;
	AddPinPadding.Right += 3.0f;

	OutputBox->AddSlot().AutoHeight().VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(AddPinPadding)[AddPinButton];
}

EVisibility SGraphNodeCasePairedPinsNode::IsAddPinButtonVisible() const
{
	return SGraphNode::IsAddPinButtonVisible();
}

FReply SGraphNodeCasePairedPinsNode::OnAddPin()
{
	UK2Node_CasePairedPinsNode* CasePairedPinsNode = CastChecked<UK2Node_CasePairedPinsNode>(GraphNode);

	const FScopedTransaction Transaction(NSLOCTEXT("BlueprintExtensions", "AddExecutionPin", "添加执行引脚"));
	CasePairedPinsNode->Modify();

	CasePairedPinsNode->AddCasePinLast();
	FBlueprintEditorUtils::MarkBlueprintAsModified(CasePairedPinsNode->GetBlueprint());

	UpdateGraphNode();
	GraphNode->GetGraph()->NotifyGraphChanged();

	return FReply::Handled();
}
