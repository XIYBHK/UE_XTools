// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BABlueprintActionMenu.h"

#include "BlueprintAssistUtils.h"

#if BA_UE_VERSION_OR_LATER(5, 1)

#include "BlueprintActionMenuBuilder.h"
#include "BlueprintActionMenuItem.h"
#include "BlueprintActionMenuUtils.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintDragDropMenuItem.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorSettings.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2_Actions.h"
#include "IDocumentation.h"
#include "K2Node_Variable.h"
#include "SGraphActionMenu.h"
#include "SPinTypeSelector.h"
#include "EdGraph/EdGraphSchema.h"
#include "Stats/StatsMisc.h"
#include "Widgets/Input/SCheckBox.h"

static FString GetVarType(UStruct* VarScope, FName VarName, bool bUseObjToolTip, bool bDetailed = false)
{
	FString VarDesc;

	if (VarScope)
	{
		if (FProperty* Property = FindFProperty<FProperty>(VarScope, VarName))
		{
			// If it is an object property, see if we can get a nice class description instead of just the name
			FObjectProperty* ObjProp = CastField<FObjectProperty>(Property);
			if (bUseObjToolTip && ObjProp && ObjProp->PropertyClass)
			{
				VarDesc = ObjProp->PropertyClass->GetToolTipText(GetDefault<UBlueprintEditorSettings>()->bShowShortTooltips).ToString();
			}

			// Name of type
			if (VarDesc.Len() == 0)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				FEdGraphPinType PinType;
				if (K2Schema->ConvertPropertyToPinType(Property, PinType)) // use schema to get the color
				{
					VarDesc = UEdGraphSchema_K2::TypeToText(PinType).ToString();
				}
			}
		}
	}

	return VarDesc;
}

static void GetPaletteItemIcon(TSharedPtr<FEdGraphSchemaAction> ActionIn, UBlueprint const* BlueprintIn, FSlateBrush const*& BrushOut, FSlateColor& ColorOut, FText& ToolTipOut, FString& DocLinkOut, FString& DocExcerptOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut)
{
	// Default to tooltip based on action supplied
	ToolTipOut = ActionIn->GetTooltipDescription().IsEmpty() ? ActionIn->GetMenuDescription() : ActionIn->GetTooltipDescription();

	if (ActionIn->GetTypeId() == FBlueprintActionMenuItem::StaticGetTypeId())
	{
		FBlueprintActionMenuItem* NodeSpawnerAction = (FBlueprintActionMenuItem*)ActionIn.Get();
		BrushOut = NodeSpawnerAction->GetMenuIcon(ColorOut);

		TSubclassOf<UEdGraphNode> VarNodeClass = NodeSpawnerAction->GetRawAction()->NodeClass;
		// if the node is a variable getter or setter, use the variable icon instead, because maps need two brushes
		if (*VarNodeClass && VarNodeClass->IsChildOf(UK2Node_Variable::StaticClass()))
		{
			const UK2Node_Variable* TemplateNode = Cast<UK2Node_Variable>(NodeSpawnerAction->GetRawAction()->GetTemplateNode());
			FProperty* Property = TemplateNode->GetPropertyForVariable();
			BrushOut = FBlueprintEditor::GetVarIconAndColorFromProperty(Property, ColorOut, SecondaryBrushOut, SecondaryColorOut);
		}
	}
	else if (ActionIn->GetTypeId() == FBlueprintDragDropMenuItem::StaticGetTypeId())
	{
		FBlueprintDragDropMenuItem* DragDropAction = (FBlueprintDragDropMenuItem*)ActionIn.Get();
		BrushOut = DragDropAction->GetMenuIcon(ColorOut);
	}
	// for backwards compatibility:
	else if (UK2Node const* const NodeTemplate = FBlueprintActionMenuUtils::ExtractNodeTemplateFromAction(ActionIn))
	{
		// If the node wants to create tooltip text, use that instead, because its probably more detailed
		FText NodeToolTipText = NodeTemplate->GetTooltipText();
		if (!NodeToolTipText.IsEmpty())
		{
			ToolTipOut = NodeToolTipText;
		}

		// Ask node for a palette icon
		FLinearColor IconLinearColor = FLinearColor::White;
		BrushOut = NodeTemplate->GetIconAndTint(IconLinearColor).GetOptionalIcon();
		ColorOut = IconLinearColor;
	}
	// for MyBlueprint tab specific actions:
	else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
	{
		FEdGraphSchemaAction_K2Graph const* GraphAction = (FEdGraphSchemaAction_K2Graph const*)ActionIn.Get();
		UE_LOG(LogBlueprintAssist, Warning, TEXT("Graph ACTION!")); // TODO check if this is required!
		// GetSubGraphIcon(GraphAction, BlueprintIn, BrushOut, ColorOut, ToolTipOut);
	}
	else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
	{
		FEdGraphSchemaAction_K2Delegate* DelegateAction = (FEdGraphSchemaAction_K2Delegate*)ActionIn.Get();

		BrushOut = FAppStyle::GetBrush(TEXT("GraphEditor.Delegate_16x"));
		FFormatNamedArguments Args;
		Args.Add(TEXT("EventDispatcherName"), FText::FromName(DelegateAction->GetDelegateName()));
		ToolTipOut = FText::Format(INVTEXT("Event Dispatcher '{EventDispatcherName}'"), Args);
	}
	else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
	{
		FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)ActionIn.Get();

		UClass* VarClass = VarAction->GetVariableClass();
		BrushOut = FBlueprintEditor::GetVarIconAndColor(VarClass, VarAction->GetVariableName(), ColorOut, SecondaryBrushOut, SecondaryColorOut);
		ToolTipOut = FText::FromString(GetVarType(VarClass, VarAction->GetVariableName(), true, true));

		DocLinkOut = TEXT("Shared/Editor/Blueprint/VariableTypes");
		DocExcerptOut = GetVarType(VarClass, VarAction->GetVariableName(), false, false);
	}
	else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2LocalVar::StaticGetTypeId())
	{
		FEdGraphSchemaAction_K2LocalVar* LocalVarAction = (FEdGraphSchemaAction_K2LocalVar*)ActionIn.Get();

		UStruct* VarScope = CastChecked<UStruct>(LocalVarAction->GetVariableScope());
		BrushOut = FBlueprintEditor::GetVarIconAndColor(VarScope, LocalVarAction->GetVariableName(), ColorOut, SecondaryBrushOut, SecondaryColorOut);
		ToolTipOut = FText::FromString(GetVarType(VarScope, LocalVarAction->GetVariableName(), true));

		DocLinkOut = TEXT("Shared/Editor/Blueprint/VariableTypes");
		DocExcerptOut = GetVarType(VarScope, LocalVarAction->GetVariableName(), false);
	}
	else if (ActionIn->IsA(FEdGraphSchemaAction_BlueprintVariableBase::StaticGetTypeId()))
	{
		FEdGraphSchemaAction_BlueprintVariableBase* BPVarAction = (FEdGraphSchemaAction_BlueprintVariableBase*)(ActionIn.Get());
		const FEdGraphPinType PinType = BPVarAction->GetPinType();

		BrushOut = FBlueprintEditor::GetVarIconAndColorFromPinType(PinType, ColorOut, SecondaryBrushOut, SecondaryColorOut);
		ToolTipOut = FText::FromString(UEdGraphSchema_K2::TypeToText(PinType).ToString());

		DocLinkOut = TEXT("Shared/Editor/Blueprint/VariableTypes");
		DocExcerptOut = UEdGraphSchema_K2::TypeToText(PinType).ToString();
	}
	else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Enum::StaticGetTypeId())
	{
		BrushOut = FAppStyle::GetBrush(TEXT("GraphEditor.EnumGlyph"));
		ToolTipOut = INVTEXT("Enum Asset");
	}
	else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Struct::StaticGetTypeId())
	{
		BrushOut = FAppStyle::GetBrush(TEXT("GraphEditor.StructGlyph"));
		ToolTipOut = INVTEXT("Struct Asset");
	}
	else
	{
		BrushOut = ActionIn->GetPaletteIcon();
		const FText ActionToolTip = ActionIn->GetPaletteToolTip();
		if (!ActionToolTip.IsEmpty())
		{
			ToolTipOut = ActionToolTip;
		}
	}
}

FString FBAActionMenuItem::ToString() const
{
	return Action->GetMenuDescription().ToString();
}

void SBABlueprintActionMenu::Construct(const FArguments& InArgs)
{
	GraphHandler = InArgs._GraphHandler;
	bUseSelectedPin = InArgs._bUseSelectedPin;

	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SAssignNew(FilteredList, SBAFilteredList<TSharedPtr<FBAActionMenuItem>>)
				.InitListItems(this, &SBABlueprintActionMenu::InitListItems)
				.OnGenerateRow(this, &SBABlueprintActionMenu::CreateItemWidget)
				.OnSelectItem(this, &SBABlueprintActionMenu::SelectItem)
				.WidgetSize(GetWidgetSize())
				.MenuTitle(FString("Blueprint Action Menu"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SCheckBox)
					.IsChecked(this, &SBABlueprintActionMenu::GetContextSensitiveTextboxState)
					.OnCheckStateChanged(this, &SBABlueprintActionMenu::OnContextSensitiveChanged)
				]
			]
		];
	}
	UE_LOG(LogBlueprintAssist, Verbose, TEXT("Create BA action menu took %.2f"), ThisTime);
}

void SBABlueprintActionMenu::InitListItems(TArray<TSharedPtr<FBAActionMenuItem>>& Items)
{
	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);
		FBlueprintEditor* Editor = FBAUtils::GetBlueprintEditorForGraph(GraphHandler->GetFocusedEdGraph());
		if (!Editor)
		{
			return;
		}

		TWeakPtr<FBlueprintEditor> EditorWeakPtr = StaticCastWeakPtr<FBlueprintEditor>(Editor->AsWeak());
		if (!EditorWeakPtr.IsValid())
		{
			return;
		}

#if BA_UE_VERSION_OR_LATER(5, 2)
		FBlueprintActionMenuBuilder MenuBuilder;
#else
		FBlueprintActionMenuBuilder MenuBuilder(EditorWeakPtr);
#endif

		FBlueprintActionContext FilterContext;
		FilterContext.Graphs.Add(GraphHandler->GetFocusedEdGraph());
		FilterContext.Blueprints.Add(GraphHandler->GetBlueprint());

		if (bUseSelectedPin)
		{
			if (UEdGraphPin* SelectedPin = GraphHandler->GetSelectedPin())
			{
				FilterContext.Pins.Add(SelectedPin);
			}
		}

		constexpr uint32 OriginalFlagsMask = EContextTargetFlags::TARGET_Blueprint
			| EContextTargetFlags::TARGET_SubComponents
			| EContextTargetFlags::TARGET_NodeTarget
			| EContextTargetFlags::TARGET_PinObject
			| EContextTargetFlags::TARGET_SiblingPinObjects
			| EContextTargetFlags::TARGET_BlueprintLibraries
			| EContextTargetFlags::TARGET_NonImportedTypes;

		// NOTE: cannot call GetGraphContextActions() during serialization and GC due to its use of FindObject()
		if (!GIsSavingPackage && !IsGarbageCollecting() && FilterContext.Blueprints.Num() > 0)
		{
			FBlueprintActionMenuUtils::MakeContextMenu(FilterContext, bContextSensitive, OriginalFlagsMask, MenuBuilder);
		}

		for (int i = 0; i < MenuBuilder.GetNumActions(); ++i)
		{
#if BA_UE_VERSION_OR_LATER(5, 5)
			TSharedPtr<FEdGraphSchemaAction> EdGraphSchemaAction = MenuBuilder.GetSchemaAction(i);
			Items.Add(MakeShared<FBAActionMenuItem>(EdGraphSchemaAction));
#else
			FGraphActionListBuilderBase::ActionGroup& ActionGroup = MenuBuilder.GetAction(i);
			for (TSharedPtr<FEdGraphSchemaAction> EdGraphSchemaAction : ActionGroup.Actions)
			{
				Items.Add(MakeShared<FBAActionMenuItem>(EdGraphSchemaAction));
			}
#endif
		}
	}
	UE_LOG(LogBlueprintAssist, Verbose, TEXT("Get all actions took %.2f"), ThisTime);
}

TSharedRef<ITableRow> SBABlueprintActionMenu::CreateItemWidget(TSharedPtr<FBAActionMenuItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	// construct the icon widget
	FSlateBrush const* IconBrush = FAppStyle::GetBrush(TEXT("NoBrush"));
	FSlateBrush const* SecondaryBrush = FAppStyle::GetBrush(TEXT("NoBrush"));
	FSlateColor IconColor = FSlateColor::UseForeground();
	FSlateColor SecondaryIconColor = FSlateColor::UseForeground();
	FText IconToolTip = Item->Action->GetTooltipDescription();
	FString IconDocLink, IconDocExcerpt;

	GetPaletteItemIcon(Item->Action, GraphHandler->GetBlueprint(), IconBrush, IconColor, IconToolTip, IconDocLink, IconDocExcerpt, SecondaryBrush, SecondaryIconColor);

	TSharedRef<SWidget> IconWidget = SPinTypeSelector::ConstructPinTypeImage(
		IconBrush,
		IconColor,
		SecondaryBrush,
		SecondaryIconColor,
		IDocumentation::Get()->CreateToolTip(IconToolTip, NULL, IconDocLink, IconDocExcerpt));

	IconWidget->SetEnabled(false);

	return
		SNew(STableRow<TSharedPtr<FString>>, OwnerTable).Padding(FMargin(6.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0, 0.0)
			[
				IconWidget
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Text(Item->Action->GetMenuDescription())
			]
		];
}

void SBABlueprintActionMenu::SelectItem(TSharedPtr<FBAActionMenuItem> Item)
{
	TSharedPtr<SGraphEditor> GraphEditor = GraphHandler->GetGraphEditor();
	if (!GraphEditor.IsValid())
	{
		return;
	}

#if BA_UE_VERSION_OR_LATER(5, 6)
	const FBAVector2 SpawnLocation = GraphEditor->GetPasteLocation2f();
#else
	const FBAVector2 SpawnLocation = GraphEditor->GetPasteLocation();
#endif
	UEdGraphPin* Pin = bUseSelectedPin ? GraphHandler->GetSelectedPin() : nullptr;
	Item->Action->PerformAction(GraphHandler->GetFocusedEdGraph(), Pin, SpawnLocation);
}


void SBABlueprintActionMenu::OnContextSensitiveChanged(ECheckBoxState NewState)
{
	if (NewState == ECheckBoxState::Checked)
	{
		FilteredList->GenerateItems();
		bContextSensitive = true;
	}
	else
	{
		bContextSensitive = false;
	}
}

#endif