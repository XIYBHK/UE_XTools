#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"

#include "K2Node_MapIdentical.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONS_API UK2Node_MapIdentical : public UK2Node
{
    GENERATED_BODY()

public:

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeProperties

    virtual bool IsNodePure() const override { return true; }
    virtual bool ShouldDrawCompact() const override { return true; }

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetCompactNodeTitle() const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
    virtual TSharedPtr<SWidget> CreateNodeImage() const override;

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

    virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual void PostReconstructNode() override;

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

    virtual void AllocateDefaultPins() override;
    virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
    
    UEdGraphPin* GetMapAPin() const;
    UEdGraphPin* GetMapBPin() const;
    UEdGraphPin* GetReturnValuePin() const;
    UEdGraphPin* GetTargetMapPin(const UK2Node_CallFunction* Function) const;
    UEdGraphPin* GetKeysPin(const UK2Node_CallFunction* Function) const;
    UEdGraphPin* GetValuesPin(const UK2Node_CallFunction* Function) const;
    UEdGraphPin* GetArrayAPin(const UK2Node_CallFunction* Function) const;
    UEdGraphPin* GetArrayBPin(const UK2Node_CallFunction* Function) const;

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

};