// Copyright 2023 Tomasz Klin. All Rights Reserved.

/**
 * 时间轴节点修复类
 * 用于解决链接错误的变通方案，因为并非所有 UK2Node_Timeline 函数都标记为 DLLEXPORT
 */

#pragma once

#include "K2Node_Timeline.h"
#include "K2Node_HackTimeline.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class INameValidatorInterface;
class UEdGraph;
class UEdGraphPin;

UCLASS(abstract)
class COMPONENTTIMELINEUNCOOKED_API UK2Node_HackTimeline : public UK2Node_Timeline
{
	GENERATED_UCLASS_BODY()

public:
	//~ UEdGraphNode 接口实现
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
	//~ UEdGraphNode 接口结束

	//~ UK2Node 接口实现
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetNodeAttributes(TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ UK2Node 接口结束

private:
	/** 为指定引脚展开节点 */
	void ExpandForPin(UEdGraphPin* TimelinePin, const FName PropertyName, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);
};