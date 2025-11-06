// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/LinkPinMenu.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistUtils.h"
#include "ScopedTransaction.h"
#include "SGraphPanel.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

FPinLinkerStruct::FPinLinkerStruct(UEdGraphPin* InPin, const FString& InPinName)
	: Pin(InPin)
	, PinName(InPinName)
{
	const FString OwningNodeName = FBAUtils::GetNodeName(Pin->GetOwningNode());
	PinFullString = FString::Printf(TEXT("%s (%s)"), *PinName, *OwningNodeName);
}

FString FPinLinkerStruct::GetKeySearchText() const
{
	return PinName;
}

FString FPinLinkerStruct::ToString() const
{
	return PinFullString;
}

void SLinkPinMenu::Construct(const FArguments& InArgs)
{
	// read slate args
	GraphHandler = InArgs._GraphHandler;
	SourcePin = InArgs._SourcePin;

	float OutZoom;
	GraphHandler->GetGraphEditor()->GetViewLocation(SavedLocation, OutZoom);

	const FString MenuTitle = FString::Printf(
		TEXT("Link Pin to %s"),
		*SourcePin->GetName());

	ChildSlot
	[
		SAssignNew(FilteredList, SBAFilteredList<TSharedPtr<FPinLinkerStruct>>)
		.InitListItems(this, &SLinkPinMenu::InitListItems)
		.OnGenerateRow(this, &SLinkPinMenu::CreateItemWidget)
		.OnSelectItem(this, &SLinkPinMenu::SelectItem)
		.OnMarkActiveSuggestion(this, &SLinkPinMenu::MarkActiveSuggestion)
		.WidgetSize(GetWidgetSize())
		.MenuTitle(MenuTitle)
	];
}

SLinkPinMenu::~SLinkPinMenu()
{
	if (auto GraphOverlay = GraphHandler->GetGraphOverlay())
	{
		if (LastSelectedItem)
		{
			GraphOverlay->RemoveHighlightedPin(LastSelectedItem->Pin);
		}

		for (auto Item : FilteredList->AllItems)
		{
			TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphHandler->GetGraphPanel(), Item->Pin);
			GraphOverlay->RemoveTextOverWidget(GraphPin);
		}
	}

	GraphHandler->BeginLerpViewport(SavedLocation, false);
}

void SLinkPinMenu::InitListItems(TArray<TSharedPtr<FPinLinkerStruct>>& Items)
{
	// sort pins by output then input
	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();

	UEdGraphNode* SourceNode = SourcePin->GetOwningNode();

	TArray<UEdGraphNode*> AllNodes = Graph->Nodes;

	// sort nodes by distance to pin
	const auto Sorter = [MySourcePin = SourcePin, SourceNode](UEdGraphNode& NodeA, UEdGraphNode& NodeB)
	{
		int DistA = FBAUtils::DistanceSquaredBetweenNodes(SourceNode, &NodeA);
		int DistB = FBAUtils::DistanceSquaredBetweenNodes(SourceNode, &NodeB);

		bool bInDirectionA = false;
		bool bInDirectionB = false;
		if (MySourcePin->Direction == EGPD_Output)
		{
			bInDirectionA = SourceNode->NodePosX < NodeA.NodePosX;
			bInDirectionB = SourceNode->NodePosX < NodeB.NodePosX;
		}
		else
		{
			bInDirectionA = SourceNode->NodePosX > NodeA.NodePosX;
			bInDirectionB = SourceNode->NodePosX > NodeB.NodePosX;
		}

		if (bInDirectionA)
			DistA *= 0.1f;

		if (bInDirectionB)
			DistB *= 0.1f;

		return DistA < DistB;
	};

	AllNodes.Sort(Sorter);

	TMap<FName, int> SeenPinNames;

	// grab pins from the graph for the user to select
	for (UEdGraphNode* Node : AllNodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			// skip hidden | pins which we are already linked to | pins we can't connect to
			if (!FBAUtils::IsPinHidden(Pin) && !SourcePin->LinkedTo.Contains(Pin) && CanConnectSourceToPin(Pin))
			{
				FName PinName = Pin->GetFName();
				SeenPinNames.FindOrAdd(PinName, 0) += 1;

				FString PinUniqueName = FString::Printf(TEXT("%s_%d"), *PinName.ToString(), SeenPinNames[PinName]);

				auto Item = MakeShareable(new FPinLinkerStruct(Pin,PinUniqueName));
				Items.Add(Item);

				TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphHandler->GetGraphPanel(), Pin);
				FBAGraphOverlayTextParams Params;
				Params.Text = FText::FromString(PinUniqueName);
				Params.Widget = GraphPin;
				Params.WidgetBounds = FBAUtils::GetPinBounds(GraphPin);
				GraphHandler->GetGraphOverlay()->DrawTextOverWidget(Params);
			}
		}
	}
}

TSharedRef<ITableRow> SLinkPinMenu::CreateItemWidget(TSharedPtr<FPinLinkerStruct> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	UEdGraphPin* Pin = Item->Pin;

	const UEdGraphSchema* Schema = Pin->GetSchema();

	// Get the type color and icon
	const FSlateColor TypeColor = Schema->GetPinTypeColor(Pin->PinType);

	const FSlateBrush* ContextIcon = FBlueprintEditorUtils::GetIconFromPin(Item->Pin->PinType);

	const FString ItemName = Item->ToString();

	return
		SNew(STableRow< TSharedPtr<FString>>, OwnerTable).Padding(FMargin(2.0f, 4.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, (ContextIcon != nullptr) ? 5 : 0, 0)
			[
				SNew(SImage)
				.ColorAndOpacity(TypeColor)
				.Image(ContextIcon)
			]
			+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 2)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ItemName))
			]
		];
}

void SLinkPinMenu::SelectItem(TSharedPtr<FPinLinkerStruct> Item)
{
	if (!Item->Pin)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "LinkPinMenu", "Link Pin (Menu)"));

	FBAUtils::TryLinkPins(SourcePin, Item->Pin);

	GraphHandler->BeginLerpViewport(SavedLocation, false);
}

void SLinkPinMenu::MarkActiveSuggestion(TSharedPtr<FPinLinkerStruct> Item)
{
	if (auto GraphOverlay = GraphHandler->GetGraphOverlay())
	{
		if (LastSelectedItem)
		{
			GraphOverlay->RemoveHighlightedPin(LastSelectedItem->Pin);
		}

		GraphOverlay->AddHighlightedPin(Item->Pin, FLinearColor(0.75, 0.5, 0, 0.66));
	}

	GraphHandler->LerpNodeIntoView(Item->Pin->GetOwningNode(), true);

	LastSelectedItem = Item;
}

bool SLinkPinMenu::CanConnectSourceToPin(UEdGraphPin* Pin)
{
	const UEdGraphSchema* Schema = Pin->GetSchema();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, Pin);
	return Response.Response != CONNECT_RESPONSE_DISALLOW;
}

void SLinkPinMenu::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	// if (TSharedPtr<IBAFilteredListItem> Item = FilteredList->GetSuggestedItem())
	// {
	// 	TSharedPtr<FPinLinkerStruct> LinkPinItem = StaticCastSharedPtr<FPinLinkerStruct>(Item);
	// 	TSharedPtr<SGraphPin> GraphPin = GraphHandler->GetGraphPin(LinkPinItem->Pin);
	// 	if (GraphPin.IsValid())
	// 	{
	// 		GraphPin->SetPinColorModifier(UBASettings::Get().PinHighlightColor);
	// 	}
	// }
}
