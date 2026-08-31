/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 * 
 * Based on AdvancedControlFlow by Colory Games (MIT License)
 * https://github.com/colory-games/UEPlugin-AdvancedControlFlow
 */

#include "K2Nodes/K2Node_CasePairedPinsNode.h"
#include "K2Nodes/K2NodeHelpers.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "BlueprintExtensions"

const FName DefaultExecPinName(TEXT("DefaultExec"));
const FName DefaultExecPinFriendlyName(TEXT("Default"));

namespace
{
	bool TryGetCasePinIndex(const UEdGraphPin* Pin, const FString& Prefix, int32& OutIndex)
	{
		if (!Pin)
		{
			return false;
		}

		const FString ExpectedPrefix = Prefix + TEXT("_");
		const FString PinName = Pin->GetFName().ToString();
		if (!PinName.StartsWith(ExpectedPrefix, ESearchCase::CaseSensitive))
		{
			return false;
		}

		const FString IndexString = PinName.RightChop(ExpectedPrefix.Len());
		if (IndexString.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : IndexString)
		{
			if (!FChar::IsDigit(Character))
			{
				return false;
			}
		}

		const int64 ParsedIndex = FCString::Atoi64(*IndexString);
		if (ParsedIndex > MAX_int32)
		{
			return false;
		}

		OutIndex = static_cast<int32>(ParsedIndex);
		return true;
	}
}

UK2Node_CasePairedPinsNode::UK2Node_CasePairedPinsNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UK2Node_CasePairedPinsNode::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	if (!Context->bIsDebugging)
	{
		FToolMenuSection& Section = Menu->AddSection(NodeContextMenuSectionName, NodeContextMenuSectionLabel);

		if (Context->Pin != nullptr && IsCasePin(Context->Pin))
		{
			Section.AddMenuEntry("AddCasePinBefore", LOCTEXT("AddCasePinBefore", "在此前添加条件"),
					LOCTEXT("AddCasePinBeforeTooltip", "在此引脚前添加一个条件分支"), FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_CasePairedPinsNode*>(this),
						&UK2Node_CasePairedPinsNode::AddCasePinBefore, const_cast<UEdGraphPin*>(Context->Pin))));
				Section.AddMenuEntry("AddCasePinAfter", LOCTEXT("AddCasePinAfter", "在此后添加条件"),
					LOCTEXT("AddCasePinAfterTooltip", "在此引脚后添加一个条件分支"), FSlateIcon(),
					FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_CasePairedPinsNode*>(this),
						&UK2Node_CasePairedPinsNode::AddCasePinAfter, const_cast<UEdGraphPin*>(Context->Pin))));
			Section.AddMenuEntry("RemoveThisCasePin", LOCTEXT("RemoveThisCasePin", "删除此条件"),
				LOCTEXT("RemoveThisCasePinTooltip", "删除此条件分支"), FSlateIcon(),
				FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_CasePairedPinsNode*>(this),
					&UK2Node_CasePairedPinsNode::RemoveCasePinAt, const_cast<UEdGraphPin*>(Context->Pin))));
		}

		if (GetCasePinCount() > 0)
		{
			Section.AddMenuEntry("RemoveFirstCasePin", LOCTEXT("RemoveFirstCasePin", "删除第一个条件"),
				LOCTEXT("RemoveFirstCasePinTooltip", "删除第一个条件分支"), FSlateIcon(),
				FUIAction(FExecuteAction::CreateUObject(
					const_cast<UK2Node_CasePairedPinsNode*>(this), &UK2Node_CasePairedPinsNode::RemoveFirstCasePin)));
			Section.AddMenuEntry("RemoveLastCasePin", LOCTEXT("RemoveLastCasePin", "删除最后一个条件"),
				LOCTEXT("RemoveLastCasePinTooltip", "删除最后一个条件分支"), FSlateIcon(),
				FUIAction(FExecuteAction::CreateUObject(
					const_cast<UK2Node_CasePairedPinsNode*>(this), &UK2Node_CasePairedPinsNode::RemoveLastCasePin)));
		}
	}
}

void UK2Node_CasePairedPinsNode::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::AllocateDefaultPins();

	int32 CasePinCount = 0;
	for (auto& Pin : OldPins)
	{
		if (IsCaseKeyPin(Pin))
		{
			++CasePinCount;
		}
	}

	for (int32 Index = 0; Index < CasePinCount; ++Index)
	{
		AddCasePinPair(Index);
	}
}

void UK2Node_CasePairedPinsNode::AddCasePinAfter(UEdGraphPin* Pin)
{
	if (Pin == nullptr)
	{
		return;
	}

	if (Pin->GetOwningNode() == this)
	{
		CasePinPair Pair = GetCasePinPair(Pin);
		UEdGraphPin* CaseKeyAfterPin = Pair.Key;
		UEdGraphPin* CaseValueAfterPin = Pair.Value;
		const int32 CaseIndexAfter = GetCaseIndexFromCaseKeyPin(CaseKeyAfterPin);
		if (!CaseKeyAfterPin || !CaseValueAfterPin || CaseIndexAfter == INDEX_NONE ||
			CaseIndexAfter != GetCaseIndexFromCaseValuePin(CaseValueAfterPin))
		{
			return;
		}

		Modify();

		// Get current key-value pin pair.
		TArray<CasePinPair> CasePairs = GetCasePinPairs();

		// Add new pin pair.
		AddCasePinPair(CaseIndexAfter + 1);

		// Restore key-value pin pair name.
		for (int32 Index = CaseIndexAfter + 1; Index < CasePairs.Num(); ++Index)
		{
			UEdGraphPin* CaseKeyPin = CasePairs[Index].Key;
			UEdGraphPin* CaseValuePin = CasePairs[Index].Value;
			if (!CaseKeyPin || !CaseValuePin)
			{
				continue;
			}

			CaseValuePin->PinName = *GetCasePinName(CaseValuePinNamePrefix.ToString(), Index + 1);
			CaseValuePin->PinFriendlyName =
				FText::AsCultureInvariant(GetCasePinFriendlyName(CaseValuePinFriendlyNamePrefix.ToString(), Index + 1));
			CaseKeyPin->PinName = *GetCasePinName(CaseKeyPinNamePrefix.ToString(), Index + 1);
			CaseKeyPin->PinFriendlyName =
				FText::AsCultureInvariant(GetCasePinFriendlyName(CaseKeyPinFriendlyNamePrefix.ToString(), Index + 1));
		}

		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}
}

void UK2Node_CasePairedPinsNode::AddCasePinBefore(UEdGraphPin* Pin)
{
	if (Pin == nullptr)
	{
		return;
	}

	if (Pin->GetOwningNode() == this)
	{
		CasePinPair Pair = GetCasePinPair(Pin);
		UEdGraphPin* CaseKeyBeforePin = Pair.Key;
		UEdGraphPin* CaseValueBeforePin = Pair.Value;
		const int32 CaseIndexBefore = GetCaseIndexFromCaseKeyPin(CaseKeyBeforePin);
		if (!CaseKeyBeforePin || !CaseValueBeforePin || CaseIndexBefore == INDEX_NONE ||
			CaseIndexBefore != GetCaseIndexFromCaseValuePin(CaseValueBeforePin))
		{
			return;
		}

		Modify();

		// Get current key-value pin pair.
		TArray<CasePinPair> CasePairs = GetCasePinPairs();

		// Add new pin pair.
		AddCasePinPair(CaseIndexBefore);

		// Restore key-value pin pair name.
		for (int32 Index = CaseIndexBefore; Index < CasePairs.Num(); ++Index)
		{
			UEdGraphPin* CaseKeyPin = CasePairs[Index].Key;
			UEdGraphPin* CaseValuePin = CasePairs[Index].Value;
			if (!CaseKeyPin || !CaseValuePin)
			{
				continue;
			}

			CaseValuePin->PinName = *GetCasePinName(CaseValuePinNamePrefix.ToString(), Index + 1);
			CaseValuePin->PinFriendlyName =
				FText::AsCultureInvariant(GetCasePinFriendlyName(CaseValuePinFriendlyNamePrefix.ToString(), Index + 1));
			CaseKeyPin->PinName = *GetCasePinName(CaseKeyPinNamePrefix.ToString(), Index + 1);
			CaseKeyPin->PinFriendlyName =
				FText::AsCultureInvariant(GetCasePinFriendlyName(CaseKeyPinFriendlyNamePrefix.ToString(), Index + 1));
		}

		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}
}

void UK2Node_CasePairedPinsNode::RemoveCasePinAt(UEdGraphPin* Pin)
{
	if (Pin == nullptr)
	{
		return;
	}

	if (Pin->GetOwningNode() == this)
	{
		const int32 CaseIndex = GetCaseIndexFromCasePin(Pin);
		if (CaseIndex == INDEX_NONE)
		{
			return;
		}

		Modify();
		RemoveCasePinAt(CaseIndex);
	}
}

void UK2Node_CasePairedPinsNode::RemoveFirstCasePin()
{
	if (GetCasePinCount() <= 0)
	{
		return;
	}

	Modify();

	RemoveCasePinAt(0);
}

void UK2Node_CasePairedPinsNode::RemoveLastCasePin()
{
	if (GetCasePinCount() <= 0)
	{
		return;
	}

	Modify();

	RemoveCasePinAt(GetCasePinCount() - 1);
}

UEdGraphPin* UK2Node_CasePairedPinsNode::GetCaseKeyPinFromCaseIndex(int32 CaseIndex) const
{
	for (auto& P : Pins)
	{
		if (IsCaseKeyPin(P))
		{
			int32 ActualIndex = GetCaseIndexFromCaseKeyPin(P);

			if ((ActualIndex != -1) && (ActualIndex == CaseIndex))
			{
				return P;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* UK2Node_CasePairedPinsNode::GetCaseValuePinFromCaseIndex(int32 CaseIndex) const
{
	for (auto& P : Pins)
	{
		if (IsCaseValuePin(P))
		{
			int32 ActualIndex = GetCaseIndexFromCaseValuePin(P);

			if ((ActualIndex != -1) && (ActualIndex == CaseIndex))
			{
				return P;
			}
		}
	}

	return nullptr;
}

CasePinPair UK2Node_CasePairedPinsNode::GetCasePinPair(UEdGraphPin* Pin) const
{
	CasePinPair Pair;

	if (IsCaseValuePin(Pin))
	{
		Pair.Key = GetCaseKeyPinFromCaseValuePin(Pin);
		Pair.Value = Pin;
	}
	else if (IsCaseKeyPin(Pin))
	{
		Pair.Key = Pin;
		Pair.Value = GetCaseValuePinFromCaseKeyPin(Pin);
	}

	return Pair;
}

int32 UK2Node_CasePairedPinsNode::GetCaseIndexFromCasePin(UEdGraphPin* Pin) const
{
	if (IsCaseValuePin(Pin))
	{
		return GetCaseIndexFromCaseValuePin(Pin);
	}
	return IsCaseKeyPin(Pin) ? GetCaseIndexFromCaseKeyPin(Pin) : INDEX_NONE;
}

int32 UK2Node_CasePairedPinsNode::GetCaseIndexFromCasePin(const FString& Prefix, UEdGraphPin* Pin) const
{
	int32 CaseIndex = INDEX_NONE;
	return TryGetCasePinIndex(Pin, Prefix, CaseIndex) ? CaseIndex : INDEX_NONE;
}

int32 UK2Node_CasePairedPinsNode::GetCaseIndexFromCaseValuePin(UEdGraphPin* Pin) const
{
	return GetCaseIndexFromCasePin(CaseValuePinNamePrefix.ToString(), Pin);
}

int32 UK2Node_CasePairedPinsNode::GetCaseIndexFromCaseKeyPin(UEdGraphPin* Pin) const
{
	return GetCaseIndexFromCasePin(CaseKeyPinNamePrefix.ToString(), Pin);
}

void UK2Node_CasePairedPinsNode::RemoveCasePinAt(int32 CaseIndex)
{
	UEdGraphPin* CaseValuePinToRemove = GetCaseValuePinFromCaseIndex(CaseIndex);
	UEdGraphPin* CaseKeyPinToRemove = GetCaseKeyPinFromCaseIndex(CaseIndex);
	if (!CaseValuePinToRemove && !CaseKeyPinToRemove)
	{
		return;
	}

	if (CaseValuePinToRemove)
	{
		Pins.Remove(CaseValuePinToRemove);
		CaseValuePinToRemove->MarkAsGarbage();
	}
	if (CaseKeyPinToRemove)
	{
		Pins.Remove(CaseKeyPinToRemove);
		CaseKeyPinToRemove->MarkAsGarbage();
	}

	const TArray<CasePinPair> RemainingPairs = GetCasePinPairs();
	for (int32 Index = 0; Index < RemainingPairs.Num(); ++Index)
	{
		UEdGraphPin* CaseKeyPin = RemainingPairs[Index].Key;
		UEdGraphPin* CaseValuePin = RemainingPairs[Index].Value;
		if (CaseKeyPin && CaseValuePin)
		{
			CaseValuePin->PinName = *GetCasePinName(CaseValuePinNamePrefix.ToString(), Index);
			CaseValuePin->PinFriendlyName =
				FText::AsCultureInvariant(GetCasePinFriendlyName(CaseValuePinFriendlyNamePrefix.ToString(), Index));
			CaseKeyPin->PinName = *GetCasePinName(CaseKeyPinNamePrefix.ToString(), Index);
			CaseKeyPin->PinFriendlyName =
				FText::AsCultureInvariant(GetCasePinFriendlyName(CaseKeyPinFriendlyNamePrefix.ToString(), Index));
		}
	}

	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

int32 UK2Node_CasePairedPinsNode::GetCasePinCount() const
{
	// 优化：直接遍历 Pins 统计 CaseValuePin 数量，O(n) 复杂度
	int32 Count = 0;
	for (const UEdGraphPin* Pin : Pins)
	{
		if (IsCaseValuePin(Pin))
		{
			++Count;
		}
	}
	return Count;
}

TArray<CasePinPair> UK2Node_CasePairedPinsNode::GetCasePinPairs() const
{
	TArray<TPair<int32, CasePinPair>> IndexedPairs;
	IndexedPairs.Reserve(GetCasePinCount());

	for (UEdGraphPin* Pin : Pins)
	{
		const int32 Index = GetCaseIndexFromCaseValuePin(Pin);
		if (Index != INDEX_NONE)
		{
			IndexedPairs.Emplace(Index, CasePinPair(GetCaseKeyPinFromCaseValuePin(Pin), Pin));
		}
	}

	IndexedPairs.Sort([](const TPair<int32, CasePinPair>& Left, const TPair<int32, CasePinPair>& Right)
	{
		return Left.Key < Right.Key;
	});

	TArray<CasePinPair> CasePairs;
	CasePairs.Reserve(IndexedPairs.Num());
	for (const TPair<int32, CasePinPair>& IndexedPair : IndexedPairs)
	{
		CasePairs.Add(IndexedPair.Value);
	}

	return CasePairs;
}

bool UK2Node_CasePairedPinsNode::IsCasePin(const UEdGraphPin* Pin) const
{
	return IsCaseKeyPin(Pin) || IsCaseValuePin(Pin);
}

bool UK2Node_CasePairedPinsNode::IsCaseKeyPin(const UEdGraphPin* Pin) const
{
	int32 CaseIndex = INDEX_NONE;
	return TryGetCasePinIndex(Pin, CaseKeyPinNamePrefix.ToString(), CaseIndex);
}

bool UK2Node_CasePairedPinsNode::IsCaseValuePin(const UEdGraphPin* Pin) const
{
	int32 CaseIndex = INDEX_NONE;
	return TryGetCasePinIndex(Pin, CaseValuePinNamePrefix.ToString(), CaseIndex);
}

FString UK2Node_CasePairedPinsNode::GetCasePinName(const FString& Prefix, int32 CaseIndex) const
{
	return FString::Printf(TEXT("%s_%d"), *Prefix, CaseIndex);
}

FString UK2Node_CasePairedPinsNode::GetCasePinFriendlyName(const FString& Prefix, int32 CaseIndex) const
{
	return FString::Printf(TEXT("%s%d"), *Prefix, CaseIndex);
}

UEdGraphPin* UK2Node_CasePairedPinsNode::GetCaseKeyPinFromCaseValuePin(const UEdGraphPin* ValuePin) const
{
	int32 CaseIndex = INDEX_NONE;
	if (!TryGetCasePinIndex(ValuePin, CaseValuePinNamePrefix.ToString(), CaseIndex))
	{
		return nullptr;
	}

	return FindPin(*GetCasePinName(CaseKeyPinNamePrefix.ToString(), CaseIndex));
}

UEdGraphPin* UK2Node_CasePairedPinsNode::GetCaseValuePinFromCaseKeyPin(const UEdGraphPin* KeyPin) const
{
	int32 CaseIndex = INDEX_NONE;
	if (!TryGetCasePinIndex(KeyPin, CaseKeyPinNamePrefix.ToString(), CaseIndex))
	{
		return nullptr;
	}

	return FindPin(*GetCasePinName(CaseValuePinNamePrefix.ToString(), CaseIndex));
}

void UK2Node_CasePairedPinsNode::AddCasePinLast()
{
	Modify();

	int32 N = GetCasePinCount();

	AddCasePinPair(N);
}

#undef LOCTEXT_NAMESPACE
