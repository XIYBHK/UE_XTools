#pragma once

#include "BlueprintAssistFormatters/GraphFormatterTypes.h"

struct FFormatterInterface;

struct FBAFormatterUtils
{
	static bool IsSameRow(const TMap<FPinLink, bool>& SameRowMapping, UEdGraphNode* NodeA, UEdGraphNode* NodeB);
	static void StraightenRow(TSharedPtr<FBAGraphHandler> GraphHandler, const TMap<FPinLink, bool>& SameRowMapping, UEdGraphNode* Node);
	static void StraightenRowWithFilter(TSharedPtr<FBAGraphHandler> GraphHandler, const TMap<FPinLink, bool>& SameRowMapping, UEdGraphNode* Node, TFunctionRef<bool(const FPinLink&)> Pred);
	static FSlateRect GetFormatterArrayBounds(TArray<TSharedPtr<FFormatterInterface>> FormatterArray, TSharedPtr<FBAGraphHandler> GraphHandler, bool bUseCommentPadding);
};
