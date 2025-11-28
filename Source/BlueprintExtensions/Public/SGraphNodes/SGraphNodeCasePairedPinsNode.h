/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 * 
 * Based on AdvancedControlFlow by Colory Games (MIT License)
 * https://github.com/colory-games/UEPlugin-AdvancedControlFlow
 */

#pragma once

#include "KismetNodes/SGraphNodeK2Base.h"

class UK2Node_CasePairedPinsNode;

class SGraphNodeCasePairedPinsNode : public SGraphNodeK2Base
{
	SLATE_BEGIN_ARGS(SGraphNodeCasePairedPinsNode)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_CasePairedPinsNode* InNode);

protected:
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
	virtual EVisibility IsAddPinButtonVisible() const override;
	virtual FReply OnAddPin() override;
};
