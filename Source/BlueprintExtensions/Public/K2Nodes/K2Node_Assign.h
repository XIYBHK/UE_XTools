#pragma once

#include "BlueprintActionFilter.h"
#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "KismetCompilerMisc.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_Assign.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraphPin;
class UObject;

UCLASS(MinimalAPI, Category = "XTools|K2Node", meta=(Keywords = "XTools, Assign"))
class UK2Node_Assign : public UK2Node
{
	GENERATED_UCLASS_BODY()

public:

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeProperties

	virtual bool ShouldDrawCompact() const { return true; }
	virtual int32 GetNodeRefreshPriority() const override { return EBaseNodeRefreshPriority::Low_UsesDependentWildcard; }
    
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance
    
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetCompactNodeTitle() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile
    
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
    
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem
    
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
    
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement
    
	virtual void AllocateDefaultPins() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostPasteNode() override;

	BLUEPRINTEXTENSIONS_API UEdGraphPin* GetTargetPin() const;
	BLUEPRINTEXTENSIONS_API UEdGraphPin* GetValuePin() const;

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region TypeHandling
    
	void CoerceTypeFromPin(const UEdGraphPin* Pin);
    
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

};