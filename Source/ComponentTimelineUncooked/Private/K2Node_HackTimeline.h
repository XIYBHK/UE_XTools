// Copyright 2023 Tomasz Klin. All Rights Reserved.
#pragma once

#include "K2Node_Timeline.h"

#include "K2Node_HackTimeline.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class INameValidatorInterface;
class UEdGraph;
class UEdGraphPin;

// This class is a workaround for fix linking error. Not all UK2Node_Timeline functions are mark as DLLEXPORT

UCLASS(abstract)
class COMPONENTTIMELINEUNCOOKED_API UK2Node_HackTimeline : public UK2Node_Timeline
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void PreloadRequiredAssets() override;
	virtual void DestroyNode() override;
	virtual void PostPasteNode() override;
	virtual void PrepareForCopying() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void FindDiffs(class UEdGraphNode* OtherNode, struct FDiffResults& Results)  override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetDocumentationExcerptName() const override;
	virtual FName GetCornerIcon() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface.
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetNodeAttributes(TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface.

private:
	void ExpandForPin(UEdGraphPin* TimelinePin, const FName PropertyName, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);
};