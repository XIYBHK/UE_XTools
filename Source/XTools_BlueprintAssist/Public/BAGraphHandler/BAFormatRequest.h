// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"
#include "UObject/WeakObjectPtr.h"

struct FBAScopedGraphAction;
class FBAGraphHandler;
class UEdGraphNode;

DECLARE_MULTICAST_DELEGATE(FOnPostFormatting);

class FBAFormatRequest
{
public:
	FBAFormatRequest(FBAGraphHandler& InGH);

	FOnPostFormatting OnPostFormatting;

	FEdGraphFormatterParameters FormatterParameters;
	FBAGraphHandler& GH;

	TSet<TWeakObjectPtr<UEdGraphNode>> PendingFormatting;
	TArray<TArray<TWeakObjectPtr<UEdGraphNode>>> FormatAllColumns;

	TSharedPtr<FBAScopedGraphAction> FormatAllTransaction;
	TSharedPtr<FBAScopedGraphAction> OngoingFormattingAction;

	TMap<FGuid, FIntPoint> PreFormatNodePositions;

	TWeakObjectPtr<UEdGraphNode> ZoomToTargetPostFormatting;

	bool bSaveAfterFormatting = false;

	void RequestFormatAll();
	void RequestFormatNode(
		UEdGraphNode* Node,
		TSharedPtr<FBAScopedGraphAction> PendingTransaction = {},
		FEdGraphFormatterParameters FormatterParameters = FEdGraphFormatterParameters());

	void UpdateNodesRequiringFormatting();

	bool IsBlueprintRootNode(UEdGraphNode* Node, bool bOnlyOutputRoots);
	void Reset();

	UEdGraphNode* FindRootNode(UEdGraphNode* InitialNode);

	TSharedPtr<FFormatterInterface> MakeFormatter();

	bool HasActiveTransaction() const;

	TSharedPtr<FFormatterInterface> FormatNodes(UEdGraphNode* Node, bool bUsingFormatAll = false);

	void PreFormatting();

	void PostFormatting(const TArray<TSharedPtr<FFormatterInterface>>& Formatters);
	void PostFormatComments(const TArray<TSharedPtr<FFormatterInterface>>& Formatters);

	void RunFormatting();
	void FormatPendingNodes();
	void FormatPendingColumns();

	void SimpleFormatAll();
	void SmartFormatAll();
	void FormatColumn(TArray<TSharedPtr<FFormatterInterface>>& CurrentColumn, float ColumnX);

	void ResetTransactions();
	void RunSavePostFormatting();

	bool RequestFormatAllAndSave();
};
