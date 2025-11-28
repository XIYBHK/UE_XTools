/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 * 
 * Based on AdvancedControlFlow by Colory Games (MIT License)
 * https://github.com/colory-games/UEPlugin-AdvancedControlFlow
 */

#pragma once

#include "SGraphNodeCasePairedPinsNode.h"

class UK2Node_MultiBranch;

class SGraphNodeMultiBranch : public SGraphNodeCasePairedPinsNode
{
	SLATE_BEGIN_ARGS(SGraphNodeMultiBranch)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_MultiBranch* InNode);

	virtual void CreatePinWidgets() override;
};
