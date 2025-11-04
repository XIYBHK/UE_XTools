#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"

#include "K2Node_ForEachArray.generated.h"

UCLASS()
class UK2Node_ForEachArray : public UK2Node
{
	GENERATED_BODY()

public:

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region NodeProperties
    
	virtual bool IsNodePure() const override { return false; }
	virtual bool ShouldDrawCompact() const { return true; }

#pragma endregion
    
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region NodeAppearance
    
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetCompactNodeTitle() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetKeywords() const override;
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
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;

	UEdGraphPin* GetLoopBodyPin() const;
	UEdGraphPin* GetBreakPin() const;
	UEdGraphPin* GetCompletedPin() const;

	UEdGraphPin* GetArrayPin() const;
	UEdGraphPin* GetValuePin() const;
	UEdGraphPin* GetIndexPin() const;
	
	void PropagatePinType() const;
    
#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
	
};