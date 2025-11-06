// Copyright fpwong. All Rights Reserved.

#pragma once

#include "BlueprintAssistGlobals.h"

#if BA_UE_VERSION_OR_LATER(5, 1)

#include "CoreMinimal.h"
#include "BAFilteredList.h"

class FBAGraphHandler;
struct FEdGraphSchemaAction;

struct FBAActionMenuItem final : IBAFilteredListItem
{
	FBAActionMenuItem(TSharedPtr<FEdGraphSchemaAction> InAction) : Action(InAction) { }

	virtual FString ToString() const override;

	TSharedPtr<FEdGraphSchemaAction> Action;
};

class BLUEPRINTASSIST_API SBABlueprintActionMenu final : public SCompoundWidget
{
	// TODO should allow for using any EdGraphPin as context instead of only the selected pin
	SLATE_BEGIN_ARGS(SBABlueprintActionMenu)
		: _bUseSelectedPin(true)
	{
	}
	SLATE_ARGUMENT(TSharedPtr<FBAGraphHandler>, GraphHandler)
	SLATE_ARGUMENT(bool, bUseSelectedPin)
	SLATE_END_ARGS()

	static FVector2D GetWidgetSize() { return FVector2D(480.0f, 300); }

	void Construct(const FArguments& InArgs);

	void InitListItems(TArray<TSharedPtr<FBAActionMenuItem>>& Items);

	TSharedRef<ITableRow> CreateItemWidget(TSharedPtr<FBAActionMenuItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	void SelectItem(TSharedPtr<FBAActionMenuItem> Item);

protected:
	TSharedPtr<FBAGraphHandler> GraphHandler;
	bool bUseSelectedPin = true;

	bool bContextSensitive = true;

	ECheckBoxState GetContextSensitiveTextboxState() const { return bContextSensitive ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }

	void OnContextSensitiveChanged(ECheckBoxState NewState);

	TSharedPtr<SBAFilteredList<TSharedPtr<FBAActionMenuItem>>> FilteredList;
};

#endif