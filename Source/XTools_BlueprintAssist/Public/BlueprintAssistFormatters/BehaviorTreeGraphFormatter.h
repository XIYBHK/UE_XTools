// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BlueprintAssistSettings.h"
#include "FormatterInterface.h"

class FBAGraphHandler;

using FContour = TMap<int32, float>;
using FTidyNodePtr = TSharedPtr<struct FBATidyNode>;

/* 
 * Based on tidy tree algorithm
 * Doesn't do Principle 6 (The child nodes of a parent node should be evenly spaced)
 * https://llimllib.github.io/pymag-trees
 * https://www.zxch3n.com/tidy/tidy/#a-naive-non-layered-tree-visualization
 */
struct FBATidyNode : public TSharedFromThis<FBATidyNode>
{
	UEdGraphNode* GraphNode = nullptr;
	FTidyNodePtr Parent;
	TArray<FTidyNodePtr> Children;

	float RelativeX = 0.f; // relative x pos to parent
	int32 Depth = 0;
	float Mod = 0.f; // modifier to shift the subtree right
	FVector2D Size;

	FTidyNodePtr GetLeftMostChild() const { return Children.Num() > 0 ? Children[0] : nullptr; }
	FTidyNodePtr GetRightMostChild() const { return Children.Num() > 0 ? Children.Last() : nullptr; }
	FTidyNodePtr GetLeftSibling() const;

	bool IsLeaf() const { return Children.Num() == 0; }
};

struct FBATidyTree
{
	struct FBALayer
	{
		float Height;
		float FinalPos;
	};

	FBATidyTree(TSharedPtr<FBAGraphHandler> InGraphHandler, FVector2D InPadding)
		: GraphHandler(InGraphHandler)
		, Padding(InPadding)
	{
	}

	TSharedPtr<FBAGraphHandler> GraphHandler;
	FVector2D Padding;

	void FormatTidyTree(UEdGraphNode* RootNode);

	FTidyNodePtr BuildTidyTree(UEdGraphNode* CurrGraphNode, int32 Depth, TMap<UEdGraphNode*, FTidyNodePtr>& VisitedNodes);
	void FirstPass_Setup(FTidyNodePtr TidyNode) const;
	void SecondPass_ApplyMods(FTidyNodePtr TidyNode, float ModSum) const;

	void PushRight(FTidyNodePtr TidyNode) const;
	void GetLeftContour(FTidyNodePtr TidyNode, float ModSum, FContour& Contour) const;
	void GetRightContour(FTidyNodePtr TidyNode, float ModSum, FContour& Contour) const;

	TArray<FTidyNodePtr> AllNodes;

private:
	TArray<FBALayer> Layers;
};

class XTOOLS_BLUEPRINTASSIST_API FBehaviorTreeGraphFormatter final
	: public FFormatterInterface
{
public:
	TSharedPtr<FBAGraphHandler> GraphHandler;
	FBAFormatterSettings FormatterSettings;
	FEdGraphFormatterParameters FormatterParameters;

	TSet<UEdGraphNode*> FormattedNodes;
	UEdGraphNode* RootNode;

	TArray<float> LayerMaxSize;
	TArray<float> LayerPos;

	virtual TSet<UEdGraphNode*> GetFormattedNodes() override { return FormattedNodes; }
	virtual UEdGraphNode* GetRootNode() override { return RootNode; }
	virtual FBAFormatterSettings GetFormatterSettings() override;
	virtual FEdGraphFormatterParameters& GetFormatterParameters() override { return FormatterParameters; }
	virtual bool ShouldIgnoreComment(TSharedPtr<FBACommentContainsNode> ContainsNode) override { return true; }

	FBehaviorTreeGraphFormatter(TSharedPtr<FBAGraphHandler> InGraphHandler, const FEdGraphFormatterParameters& InFormatterParameters);

	virtual ~FBehaviorTreeGraphFormatter() override
	{
	}

	virtual void FormatNode(UEdGraphNode* Node) override;
};
