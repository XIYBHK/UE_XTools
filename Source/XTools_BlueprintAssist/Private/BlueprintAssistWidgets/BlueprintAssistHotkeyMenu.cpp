// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BlueprintAssistHotkeyMenu.h"

#include "BlueprintAssistInputProcessor.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintEditorModule.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISinglePropertyView.h"
#include "LevelEditor.h"
#include "BlueprintAssistMisc/FBAScopedPropertySetter.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

static FName BABindingContextName("BlueprintAssistCommands");

FBAHotkeyItem::FBAHotkeyItem(TSharedPtr<FUICommandInfo> Command, TSharedPtr<FUICommandList> InCommandList)
{
	CommandInfo = Command;
	CommandName = Command->GetCommandName();
	CommandLabel = Command->GetLabel();
	CommandChord = Command->GetFirstValidChord()->GetInputText();
	BindingContext = CommandInfo->GetBindingContext();

	CommandList = InCommandList;

	if (CommandChord.IsEmptyOrWhitespace())
	{
		CommandChord = FText::FromString("Unbound");
	}

	CommandDesc = Command->GetDescription();

	SearchText = CommandLabel.ToString() + " " + CommandChord.ToString() + " " + BindingContext.ToString() + " " + CommandDesc.ToString();
}

FBAHotkeyItem::FBAHotkeyItem(UObject* SettingObj, FStructProperty* ChordProp, const FInputChord& Chord)
{
	CommandLabel = ChordProp->GetDisplayNameText();
	CommandDesc = ChordProp->GetToolTipText();
	CommandChord = Chord.GetInputText().IsEmptyOrWhitespace() ? INVTEXT("Unbound") : Chord.GetInputText(); 
	BindingContext = FName(SettingObj->GetClass()->GetDisplayNameText().ToString());

	SearchText = CommandLabel.ToString() + " " + CommandChord.ToString() + " " + BindingContext.ToString() + " " + CommandDesc.ToString();
}

FString FBAHotkeyItem::GetSearchText() const
{
	return SearchText;
}

bool FBAHotkeyItem::CanExecute() const
{
	if (CommandInfo && CommandList)
	{
		if (const FUIAction* Action = CommandList->GetActionForCommand(CommandInfo))
		{
			return Action->CanExecute();
		}
	}

	return false;
}

bool FBAHotkeyItem::Execute()
{
	if (CommandInfo && CommandList)
	{
		if (const FUIAction* Action = CommandList->GetActionForCommand(CommandInfo))
		{
			if (Action->CanExecute())
			{
				return Action->Execute();
			}
		}
	}

	return false;
}

void SBAHotkeyMenu::Construct(const FArguments& InArgs)
{
	FSinglePropertyParams Params;
	Params.NotifyHook = &SettingsPropertyHook;
	Params.Font = BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")); 
	Params.NamePlacement = EPropertyNamePlacement::Type::Inside;
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedPtr<ISinglePropertyView> PropCheckbox = EditModule.CreateSingleProperty(GetMutableDefault<UBASettings_EditorFeatures>(), GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bDisplayAllHotkeys), Params);
	FSimpleDelegate Delegate = FSimpleDelegate::CreateRaw(this, &SBAHotkeyMenu::RefreshList);
	PropCheckbox->SetOnPropertyValueChanged(Delegate);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.0)
		[
			SAssignNew(FilteredList, SBAFilteredList<TSharedPtr<FBAHotkeyItem>>)
			.InitListItems(this, &SBAHotkeyMenu::InitListItems)
			.OnGenerateRow(this, &SBAHotkeyMenu::CreateItemWidget)
			.OnSelectItem(this, &SBAHotkeyMenu::SelectItem)
			.WidgetSize(GetWidgetSize())
			.MenuTitle(FString("Blueprint Assist Hotkeys"))
			.CloseWhenSelecting(false)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SBorder).BorderImage(FBAStyle::GetBrush("BlueprintAssist.LightBorder")).HAlign(HAlign_Left)
			[
				PropCheckbox.ToSharedRef()
			]
		]
	];
}

void SBAHotkeyMenu::InitListItems(TArray<TSharedPtr<FBAHotkeyItem>>& Items)
{
	const FInputBindingManager& InputBindingManager = FInputBindingManager::Get();

	TArray<TSharedPtr<FUICommandInfo>> CommandInfos;

	if (!UBASettings_EditorFeatures::Get().bDisplayAllHotkeys)
	{
		InputBindingManager.GetCommandInfosFromContext(BABindingContextName, CommandInfos);
	}
	else
	{
		// Get all command infos
		TArray<TSharedPtr<FBindingContext>> AllBindingContexts;
		InputBindingManager.GetKnownInputContexts(AllBindingContexts);

		for (TSharedPtr<FBindingContext> BindingContext : AllBindingContexts)
		{
			TArray<TSharedPtr<FUICommandInfo>> LocalCommandInfos;
			InputBindingManager.GetCommandInfosFromContext(BindingContext->GetContextName(), LocalCommandInfos);
			CommandInfos.Append(LocalCommandInfos);
		}
	}


	TArray<TSharedPtr<FUICommandList>> AvailableLists;

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	AvailableLists.Add(LevelEditorModule.GetGlobalLevelEditorActions());

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	AvailableLists.Add(BlueprintEditorModule.GetsSharedBlueprintEditorCommands());

	IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");
	AvailableLists.Add(MainFrame.GetMainFrameCommandBindings());

	AvailableLists.Append(FBAInputProcessor::Get().GetCommandLists());

	for (TSharedPtr<FUICommandInfo> Command : CommandInfos)
	{
		TSharedPtr<FUICommandList> CommandList = FindCommandListForCommand(Command, AvailableLists);
		Items.Add(MakeShareable(new FBAHotkeyItem(Command, CommandList)));
	}

	AddInputChords(Items);
}

void SBAHotkeyMenu::AddInputChords(TArray<TSharedPtr<FBAHotkeyItem>>& Items)
{
	TArray<UObject*> SettingsArray;

	if (UBASettings_EditorFeatures::Get().bDisplayAllHotkeys)
	{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("Settings")))
		{
			ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
			TArray<FName> AllContainers;
			SettingsModule.GetContainerNames(AllContainers);
			for (auto ContainerName : AllContainers)
			{
				TSharedPtr<ISettingsContainer> Container = SettingsModule.GetContainer(ContainerName);
				if (Container.IsValid())
				{
					TArray<TSharedPtr<ISettingsCategory>> AllCategories;
					Container->GetCategories(AllCategories);

					for (auto Category : AllCategories)
					{
						if (Category.IsValid())
						{
							TArray<TSharedPtr<ISettingsSection>> AllSections;
							Category->GetSections(AllSections);

							for (TSharedPtr<ISettingsSection> Section : AllSections)
							{
								TWeakObjectPtr<UObject> SettingsObject = Section->GetSettingsObject();
								if (SettingsObject.IsValid())
								{
									SettingsArray.Add(SettingsObject.Get());
								}
							}
						}
					}
				}
			}
		}
	}
	else // use only the blueprint assist settings objects
	{
		SettingsArray = {
			&UBASettings::GetMutable(),
			&UBASettings_EditorFeatures::GetMutable(),
			&UBASettings_Advanced::GetMutable()
		};
	}

	for (UObject* Setting : SettingsArray)
	{
		for (FStructProperty* StructProp : TFieldRange<FStructProperty>(Setting->GetClass(), EFieldIteratorFlags::IncludeSuper))
		{
			if (StructProp->Struct->IsChildOf(FInputChord::StaticStruct()))
			{
				if (const FInputChord* Chord = StructProp->ContainerPtrToValuePtr<FInputChord>(Setting))
				{
					TSharedPtr<FBAHotkeyItem> NewItem = MakeShared<FBAHotkeyItem>(Setting, StructProp, *Chord);
					Items.Add(NewItem);
				}
			}
		}
	}
}

TSharedPtr<FUICommandList> SBAHotkeyMenu::FindCommandListForCommand(TSharedPtr<FUICommandInfo> CommandInfo, TArray<TSharedPtr<FUICommandList>>& AvailableLists)
{
	for (TSharedPtr<FUICommandList> CommandList : AvailableLists)
	{
		if (CommandList->GetActionForCommand(CommandInfo))
		{
			return CommandList;
		}
	}

	return nullptr;
}

TSharedRef<ITableRow> SBAHotkeyMenu::CreateItemWidget(TSharedPtr<FBAHotkeyItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const bool bIsBlueprintAssistCommand = Item->BindingContext == BABindingContextName;// && Item->CommandName.IsValid();
	const bool bHasDescription = !Item->CommandDesc.IsEmptyOrWhitespace();

	TSharedRef<SVerticalBox> RightBlock = SNew(SVerticalBox);

	// can't use Item->CanExecute cause some commands will cause crashes checking this?
	if (Item->CommandInfo && Item->CommandList)
	{
		RightBlock->AddSlot().HAlign(HAlign_Right)
		.AttachWidget(SNew(SButton)
		.ToolTipText(INVTEXT("Try to run the command"))
		.Text(INVTEXT("Run")).OnClicked_Lambda([Item]()
		{
			Item->Execute();
			return FReply::Handled();
		}));
	}

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable).Padding(FMargin(8.0, 4.0))
	.ToolTipText(Item->CommandDesc)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right)
		[
			SNew(SCheckBox)
			.ToolTipText(bIsBlueprintAssistCommand ? INVTEXT("If unchecked, the command will be disabled. See 'Blueprint Assist Advanced > Disabled Commands'") : INVTEXT(""))
			.Padding(4.0f)
			.IsChecked_Lambda([CommandName = Item->CommandName]()
			{
				return !GetDefault<UBASettings_Advanced>()->DisabledCommands.Contains(CommandName)
					? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.IsEnabled(bIsBlueprintAssistCommand)
			.OnCheckStateChanged_Lambda([CommandName = Item->CommandName](ECheckBoxState InCheckBoxState)
			{
				if (InCheckBoxState == ECheckBoxState::Checked)
				{
					UBASettings_Advanced* Settings = GetMutableDefault<UBASettings_Advanced>();
					if (FProperty* Prop = Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBASettings_Advanced, DisabledCommands)))
					{
						const FText TransactionDesc = FText::Format(INVTEXT("Disable command \"{0}\""), FText::FromName(CommandName));
						FBAScopedPropertySetter PropertySetter(Settings, Prop, TransactionDesc, EPropertyChangeType::ArrayRemove);
						Settings->DisabledCommands.Remove(CommandName);
					}
				}
				else if (InCheckBoxState == ECheckBoxState::Unchecked)
				{
					UBASettings_Advanced* Settings = GetMutableDefault<UBASettings_Advanced>();
					if (FProperty* Prop = Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBASettings_Advanced, DisabledCommands)))
					{
						const FText TransactionDesc = FText::Format(INVTEXT("Enable command \"{0}\""), FText::FromName(CommandName));
						FBAScopedPropertySetter PropertySetter(Settings, Prop, TransactionDesc, EPropertyChangeType::ArrayAdd);
						Settings->DisabledCommands.Add(CommandName);
					}
				}
			})
		]
		+ SHorizontalBox::Slot().FillWidth(1.0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("[%s] "), *Item->BindingContext.ToString())))
					.Visibility(UBASettings_EditorFeatures::Get().bDisplayAllHotkeys ? EVisibility::Visible : EVisibility::Collapsed)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					SNew(STextBlock).Text(FText::FromString(Item->ToString()))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SSpacer).Size(FVector2D(4.0f, 0.0f))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					SNew(STextBlock)
					.Text(Item->CommandChord)
					.Font(BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.BoldFont")))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().VAlign(VAlign_Bottom)
			[
				SNew(STextBlock).Text(Item->CommandDesc)
				.ToolTipText(Item->CommandDesc)
				.Font(BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ColorAndOpacity(FLinearColor::Gray)
				.Visibility(bHasDescription ? EVisibility::Visible : EVisibility::Collapsed)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			RightBlock
		]
	];
}

void SBAHotkeyMenu::SelectItem(TSharedPtr<FBAHotkeyItem> Item)
{
	// Item->Execute();
}

void SBAHotkeyMenu::RefreshList()
{
	FilteredList->GenerateItems();
}
