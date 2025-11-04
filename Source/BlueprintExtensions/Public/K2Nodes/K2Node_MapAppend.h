#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"

#include "K2Node_MapAppend.generated.h"

UCLASS()
class BLUEPRINTEXTENSIONS_API UK2Node_MapAppend : public UK2Node
{
    GENERATED_BODY()

public:
    
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    
#pragma region NodeProperties

    virtual bool IsNodePure() const override { return false; }
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

private:
    UEdGraphPin* GetTargetMapPin() const;
    UEdGraphPin* GetSourceMapPin() const;
    UEdGraphPin* GetThenPin() const;

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

};