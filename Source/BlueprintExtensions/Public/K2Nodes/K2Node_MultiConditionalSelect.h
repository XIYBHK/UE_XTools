/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 * 
 * Based on AdvancedControlFlow by Colory Games (MIT License)
 * https://github.com/colory-games/UEPlugin-AdvancedControlFlow
 */

#pragma once

#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_CasePairedPinsNode.h"

#include "K2Node_MultiConditionalSelect.generated.h"

UCLASS(MinimalAPI, meta = (Keywords = "Select MultiConditionalSelect 多条件选择 选择 条件 返回值 三元运算 纯函数"))
class UK2Node_MultiConditionalSelect : public UK2Node_CasePairedPinsNode
{
	GENERATED_BODY()

	// Override from UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// Override from UK2Node
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool IsNodePure() const override
	{
		return true;
	}
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const;

	// Internal functions.
	void CreateDefaultOptionPin();
	void CreateReturnValuePin();
	UEdGraphPin* GetDefaultOptionPin() const;
	UEdGraphPin* GetReturnValuePin() const;
	virtual CasePinPair AddCasePinPair(int32 CaseIndex) override;

public:
	UK2Node_MultiConditionalSelect(const FObjectInitializer& ObjectInitializer);
};
