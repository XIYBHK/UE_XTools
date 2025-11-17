// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BAOpenWindowMenu.h"

#include "BlueprintAssistCommands.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistInputProcessor.h"
#include "BlueprintAssistModule.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorTabs.h"
#include "EditorUtilityBlueprint.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "WidgetBlueprint.h"
#include "WorkspaceMenuStructure.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "WorkspaceMenuStructureModule.h"
#include "BlueprintAssistMisc/BAPrivate.h"
#include "Interfaces/IMainFrameModule.h"

#if !(BA_UE_VERSION_OR_LATER(5, 1))
#include "AssetRegistry/AssetRegistryState.h"
#endif

typedef TMap<FName, TSharedRef<FTabSpawnerEntry>> FTabSpawner;

#if BA_UE_VERSION_OR_LATER(5, 0)
BA_DEFINE_PRIVATE_MEMBER_PTR(TSharedRef<FTabSpawner>, GNomadTabSpawner, FTabManager, NomadTabSpawner);
BA_DEFINE_PRIVATE_MEMBER_PTR(FTabSpawner, GTabSpawner, FTabManager, TabSpawner);
#endif

const FSlateBrush* FOpenWindowItem_Base::GetIcon()
{
	static const FSlateBrush* IconHelp = BA_STYLE_CLASS::Get().GetBrush("Icons.Help");
	return IconHelp;
}

/****************/
/* FOpenTabItem */
/****************/

FOpenTabItem::FOpenTabItem(FName InTabName, FName InTabIcon, TSharedPtr<FTabManager> InAlternateTabManager)
{
	TabName = InTabName;
	TabIconStyle = "EditorStyle";
	TabDisplayName = InTabName;
	AlternateTabManager = InAlternateTabManager;
	Icon = FSlateIcon("EditorStyle", (!InTabIcon.IsNone() ? InTabIcon : "Icons.Help"));
	if (!Icon.GetOptionalIcon())
	{
		Icon = FSlateIcon("EditorStyle", "Icons.Help");
	}
}

FOpenTabItem::FOpenTabItem(FName InTabName, FName InTabIcon, FName InTabDisplayName, TSharedPtr<FTabManager> InAlternateTabManager)
{
	TabName = InTabName;
	TabIconStyle = "EditorStyle";
	TabDisplayName = InTabDisplayName;
	AlternateTabManager = InAlternateTabManager;
	Icon = FSlateIcon("EditorStyle", (!InTabIcon.IsNone() ? InTabIcon : "Icons.Help"));
	if (!Icon.GetOptionalIcon())
	{
		Icon = FSlateIcon("EditorStyle", "Icons.Help");
	}
}

FOpenTabItem::FOpenTabItem(FName InTabName, const FSlateIcon& InTabIcon, FName InTabDisplayName, TSharedPtr<FTabManager> InAlternateTabManager)
{
	TabName = InTabName;
	Icon = (InTabIcon.GetOptionalIcon() ? InTabIcon : FSlateIcon("EditorStyle", "Icons.Help"));
	TabDisplayName = InTabDisplayName;
	AlternateTabManager = InAlternateTabManager;
}

FString FOpenTabItem::ToString() const
{
	return TabDisplayName.IsNone() ? TabName.ToString() : TabDisplayName.ToString();
}

void FOpenTabItem::SelectItem()
{
	if (AlternateTabManager.IsValid())
	{
#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
		AlternateTabManager->TryInvokeTab(TabName);
#else
		AlternateTabManager->InvokeTab(TabInfo.TabName);
#endif
	}
	else
	{
#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
		FGlobalTabmanager::Get()->TryInvokeTab(TabName);
#else
		FGlobalTabmanager::Get()->InvokeTab(TabName);
#endif
	}
}

/***********************/
/* FOpenTabSpawnerItem */
/***********************/

#if BA_UE_VERSION_OR_LATER(5, 0)
FString FOpenTabSpawnerItem::ToString() const
{
	return TabSpawnerEntry->GetDisplayName().ToString();
}

void FOpenTabSpawnerItem::SelectItem()
{
	TSharedPtr<FTabManager> TabManager = AlternateTabManager.IsValid() ? AlternateTabManager : FGlobalTabmanager::Get();
	if (TabManager.IsValid())
	{
		TabManager->TryInvokeTab(GetTabName());
	}
}

const FSlateBrush* FOpenTabSpawnerItem::GetIcon()
{
	return TabSpawnerEntry->GetIcon().GetIcon();
}

FName FOpenTabSpawnerItem::GetTabName() const
{
	return TabSpawnerEntry->GetTabType();
}

const FString* FOpenTabSpawnerItem::GetDetailsString()
{
	if (TabSpawnerEntry->GetTooltipText().IsEmptyOrWhitespace())
	{
		return nullptr;
	}

	return &TabSpawnerEntry->GetTooltipText().ToString();
}
#endif

/********************/
/* FOpenSettingItem */
/********************/

FString FOpenSettingItem::ToString() const
{
	return SectionDisplayName;
}

FString FOpenSettingItem::GetSearchText() const
{
	return ToString() + " " + GetCategoryString();
}

FString FOpenSettingItem::GetCategoryString() const
{
	return FString::Printf(TEXT("%s | %s"),
		*ContainerName.ToString(),
		*CategoryName.ToString());
}

const FSlateBrush* FOpenSettingItem::GetIcon()
{
	if (ContainerName == "Project")
	{
		return BA_STYLE_CLASS::Get().GetBrush("ProjectSettings.TabIcon");
	}

	if (ContainerName == "Editor")
	{
		return BA_STYLE_CLASS::Get().GetBrush("EditorPreferences.TabIcon");
	}

	return nullptr;
}

void FOpenSettingItem::SelectItem()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("Settings")))
	{
		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.ShowViewer(ContainerName, CategoryName, SectionName);
	}
}

bool FOpenSettingItem::operator==(const FOpenSettingItem& Other)
{
	return SectionName == Other.SectionName && CategoryName == Other.CategoryName && ContainerName == Other.ContainerName;
}

/******************/
/* FToolsMenuItem */
/******************/

FString FToolsMenuItem::ToString() const
{
	return Entry->Label.Get().ToString();
}

void FToolsMenuItem::SelectItem()
{
	// Entry->TryExecuteToolUIAction(Menu->Context);

	// TODO: Currently the context has empty CommandLists array. It seems like the context is passed through but never set at this point when building the menu
	FToolMenuContext& Context = Section->Context;

	TSharedPtr<const FUICommandList> CommandListForAction;
	if (const FUIAction* Action = Entry->GetActionForCommand(Context, CommandListForAction))
	{
		Action->Execute();
	}
}

/***********************/
/* FExecuteCommandItem */
/***********************/

FString FExecuteCommandItem::ToString() const { return Command->GetLabel().ToString(); }

void FExecuteCommandItem::SelectItem()
{
	if (Command.IsValid())
	{
		for (TSharedPtr<FUICommandList> CommandList : AvailableLists)
		{
			TSharedRef<FUICommandInfo> SharedRef = Command.ToSharedRef();
			CommandList->TryExecuteAction(SharedRef);
		}
	}
}

const FSlateBrush* FExecuteCommandItem::GetIcon()
{
	if (const FSlateBrush* Brush = Command->GetIcon().GetIcon())
	{
		return Brush;
	}

	return FOpenWindowItem_Base::GetIcon();
}

TOptional<FString> FExecuteCommandItem::GetKeyboardShortcut()
{
	return TOptional<FString>(Command->GetFirstValidChord()->GetInputText().ToString());
}

const FString* FExecuteCommandItem::GetDetailsString()
{
	return &Command->GetDescription().ToString();
}

/***************/
/* FWidgetItem */
/***************/

FWidgetItem::FWidgetItem(TSharedPtr<SWidget> MenuEntryBlock)
{
	Widget = MenuEntryBlock;
	if (TSharedPtr<STextBlock> TextBlock = FIND_CHILD_WIDGET(MenuEntryBlock, STextBlock))
	{
		Name = TextBlock->GetText().ToString();
	}
	else
	{
		Name = "Nullptr";
	}
}

FString FWidgetItem::ToString() const
{
	return Name;
}

void FWidgetItem::SelectItem()
{
	FBAUtils::InteractWithWidget(Widget);
}

void FOpenWindowItem_Action::SelectItem()
{
	Action.ExecuteIfBound();
}

FString FEditorUtilityItem::ToString() const
{
	return AssetData.AssetName.ToString();
}

void FEditorUtilityItem::SelectItem()
{
	if (UEditorUtilityWidgetBlueprint* UtilityWidget = Cast<UEditorUtilityWidgetBlueprint>(AssetData.GetAsset()))
	{
		if (UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
		{
			EditorUtilitySubsystem->SpawnAndRegisterTab(UtilityWidget);
		}
	}
	else
	{
		if (auto Asset = AssetData.GetAsset())
		{
			if (UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>())
			{
				EditorUtilitySubsystem->TryRun(Asset);
			}
		}
	}
}

void SBAOpenWindowMenu::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBAFilteredList<TSharedPtr<FOpenWindowItem_Base>>)
		.InitListItems(this, &SBAOpenWindowMenu::InitListItems)
		.OnGenerateRow(this, &SBAOpenWindowMenu::CreateItemWidget)
		.OnSelectItem(this, &SBAOpenWindowMenu::SelectItem)
		.WidgetSize(GetWidgetSize())
		.MenuTitle(FString("Open Window"))
	];
}

void SBAOpenWindowMenu::InitListItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	AddOpenTabItems(Items);
	AddOpenSettingsItems(Items);
	AddCommandItems(Items);
	AddActionItems(Items);
	AddEditorUtilityWidgets(Items);

	// TODO: this nearly works need to figure out how to actually run the action though...
	// AddToolItems(Items);

	// AddMenuEntryBlocks(Items);
}

void SBAOpenWindowMenu::AddOpenTabItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	TSet<FName> ExistingTabNames;

#if BA_UE_VERSION_OR_LATER(5, 0)
	// read tabs dynamically
	{
		UE_LOG(LogBlueprintAssist, Verbose, TEXT("Adding tabs for global tab manager"));
		AddTabManagerItems(FGlobalTabmanager::Get(), ExistingTabNames, Items);

		if (IAssetEditorInstance* Editor = FBAUtils::GetEditorFromActiveTab())
		{
			UE_LOG(LogBlueprintAssist, Verbose, TEXT("Adding tabs for editor %s"), *Editor->GetEditorName().ToString());
			TSharedPtr<FTabManager> EditorTabManagerPtr = Editor->GetAssociatedTabManager();
			AddTabManagerItems(EditorTabManagerPtr, ExistingTabNames, Items);
		}

		UE_LOG(LogBlueprintAssist, Verbose, TEXT("Adding tabs for level editor"));
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		AddTabManagerItems(LevelEditorTabManager, ExistingTabNames, Items);

		// also try adding via workspace menu structure (this probably won't do anything since the previous 3 tab managers should cover for these)
		// AddWorkspaceMenuStructure(Items, ExistingTabNames);
	}
#endif

	{
		// legacy method to manually add tabs, most tabs should be dynamically loaded in the previous logic but
		// some tabs are not registered with the tab manager until requested, so this is still needed
		// currently only known case is - Undo History
		TArray<TSharedPtr<FOpenWindowItem_Base>> TempTabItems;
		AddHardCodedTabItems(TempTabItems);

		for (TSharedPtr<FOpenWindowItem_Base> Item : TempTabItems)
		{
			if (TSharedPtr<FOpenTabItem> TabItem = StaticCastSharedPtr<FOpenTabItem>(Item))
			{
				if (ExistingTabNames.Contains(TabItem->TabName))
				{
					continue;
				}

				UE_LOG(LogBlueprintAssist, Verbose, TEXT("ADD HARDCODED TAB %s"), *TabItem->TabName.ToString());
				Items.Add(TabItem);
			}
		}
	}
}

void SBAOpenWindowMenu::AddHardCodedTabItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	TArray<FOpenTabItem> TabInfos;

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

	// Category General
	TabInfos.Add(FOpenTabItem("GameplayCueApp", "Profiler.EventGraph.ExpandHotPath16", "GameplayCue Editor"));

	// Category Find in Blueprints
	TabInfos.Add(FOpenTabItem("GlobalFindResults_01", "Kismet.Tabs.FindResults", "Find in Blueprints"));
	TabInfos.Add(FOpenTabItem("GlobalFindResults_02", "Kismet.Tabs.FindResults", "Find in Blueprints 2"));
	TabInfos.Add(FOpenTabItem("GlobalFindResults_03", "Kismet.Tabs.FindResults", "Find in Blueprints 3"));
	TabInfos.Add(FOpenTabItem("GlobalFindResults_04", "Kismet.Tabs.FindResults", "Find in Blueprints 4"));

	// Category Miscellaneous
	TabInfos.Add(FOpenTabItem("AssetAudit", "GenericEditor.Tabs.Properties", "Asset Audit"));
	TabInfos.Add(FOpenTabItem("ClassViewerApp", "ClassViewer.TabIcon", "Class Viewer"));
	TabInfos.Add(FOpenTabItem("DeviceManager", "DeviceDetails.TabIcon", "Device Manager"));
	TabInfos.Add(FOpenTabItem("DeviceProfileEditor", "DeviceDetails.Tabs.ProfileEditor", "Device Profiles"));
	TabInfos.Add(FOpenTabItem("FontAtlasVisualizer", FName(), "Font Atlas Visualizer"));
	TabInfos.Add(FOpenTabItem("HierarchicalProfiler", FSlateIcon("ControlRigEditorStyle", "ControlRig.RigUnit"), "Hierarchical Profiler"));
	TabInfos.Add(FOpenTabItem("MaterialAnalyzer", "MaterialEditor.ToggleMaterialStats.Tab", "Material Analyzer"));
	TabInfos.Add(FOpenTabItem("MergeActors", "MergeActors.TabIcon", "Merge Actors"));
	TabInfos.Add(FOpenTabItem("LevelEditorPixelInspector", FSlateIcon("PixelInspectorStyle", "PixelInspector.TabIcon"), "Pixel Inspector"));
	TabInfos.Add(FOpenTabItem("SessionFrontend", "SessionFrontEnd.TabIcon", "Session Frontend"));
	TabInfos.Add(FOpenTabItem("StructViewerApp", "ClassViewer.TabIcon", "Struct Viewer"));
	TabInfos.Add(FOpenTabItem("TextureAtlasVisualizer", FName(), "Texture Atlas Visualizer"));
	TabInfos.Add(FOpenTabItem("TimecodeProviderTab", "TimecodeProvider.TabIcon", "Timecode Provider", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem("VisualLogger", FSlateIcon("LogVisualizerStyle", "LogVisualizerApp.TabIcon"), "Visual Logger"));
	TabInfos.Add(FOpenTabItem("WidgetReflector", FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "WidgetReflector.TabIcon"), "Widget Reflector"));

	// Category Log
	TabInfos.Add(FOpenTabItem("DeviceOutputLog", "Log.TabIcon", "Device Output Log"));
	TabInfos.Add(FOpenTabItem("OutputLog", "Log.TabIcon", "Output Log"));
	TabInfos.Add(FOpenTabItem("MessageLog", "MessageLog.TabIcon", "Message Log"));

	// Category Debug
	TabInfos.Add(FOpenTabItem("DebuggerApp", "BlueprintDebugger.TabIcon", "Blueprint Debugger"));
	TabInfos.Add(FOpenTabItem("CollisionAnalyzerApp", "CollisionAnalyzer.TabIcon", "Collision Analyzer"));
	TabInfos.Add(FOpenTabItem("DebugTools", "DebugTools.TabIcon", "Debug Tools"));
	TabInfos.Add(FOpenTabItem("VisualizerSpawnPoint", FSlateIcon(BA_STYLE_CLASS::Get().GetStyleSetName(), "Profiler.Tab"), "Profile Data Visualizer"));

	// Category Content Browser
	TabInfos.Add(FOpenTabItem("ContentBrowserTab1", "ContentBrowser.TabIcon", "Content Browser 1"));
	TabInfos.Add(FOpenTabItem("ContentBrowserTab2", "ContentBrowser.TabIcon", "Content Browser 2"));
	TabInfos.Add(FOpenTabItem("ContentBrowserTab3", "ContentBrowser.TabIcon", "Content Browser 3"));
	TabInfos.Add(FOpenTabItem("ContentBrowserTab4", "ContentBrowser.TabIcon", "Content Browser 4"));

	// copied from LevelEditor.cpp
	static const FName LevelEditorViewport(TEXT("LevelEditorViewport"));
	static const FName LevelEditorViewport_Clone1(TEXT("LevelEditorViewport_Clone1"));
	static const FName LevelEditorViewport_Clone2(TEXT("LevelEditorViewport_Clone2"));
	static const FName LevelEditorViewport_Clone3(TEXT("LevelEditorViewport_Clone3"));
	static const FName LevelEditorViewport_Clone4(TEXT("LevelEditorViewport_Clone4"));
	static const FName LevelEditorToolBox(TEXT("LevelEditorToolBox"));
	static const FName LevelEditorSelectionDetails(TEXT("LevelEditorSelectionDetails"));
	static const FName LevelEditorSelectionDetails2(TEXT("LevelEditorSelectionDetails2"));
	static const FName LevelEditorSelectionDetails3(TEXT("LevelEditorSelectionDetails3"));
	static const FName LevelEditorSelectionDetails4(TEXT("LevelEditorSelectionDetails4"));
	static const FName PlacementBrowser(TEXT("PlacementBrowser"));
	static const FName LevelEditorBuildAndSubmit(TEXT("LevelEditorBuildAndSubmit"));
	static const FName LevelEditorSceneOutliner(TEXT("LevelEditorSceneOutliner"));
	static const FName LevelEditorStatsViewer(TEXT("LevelEditorStatsViewer"));
	static const FName LevelEditorLayerBrowser(TEXT("LevelEditorLayerBrowser"));
	static const FName LevelEditorDataLayerBrowser(TEXT("LevelEditorDataLayerBrowser"));
	static const FName Sequencer(TEXT("Sequencer"));
	static const FName SequencerGraphEditor(TEXT("SequencerGraphEditor"));
	static const FName WorldSettings(TEXT("WorldSettingsTab"));
	static const FName WorldBrowserComposition(TEXT("WorldBrowserComposition"));
	static const FName WorldBrowserPartitionEditor(TEXT("WorldBrowserPartitionEditor"));
	static const FName WorldBrowserHierarchy(TEXT("WorldBrowserHierarchy"));
	static const FName WorldBrowserDetails(TEXT("WorldBrowserDetails"));
	static const FName LevelEditorHierarchicalLODOutliner(TEXT("LevelEditorHierarchicalLODOutliner"));
	static const FName OutputLog(TEXT("OutputLog"));
	static const FName LevelEditorEnvironmentLightingViewer(TEXT("LevelEditorEnvironmentLightingViewer"));

	// Category Level Editor
	TabInfos.Add(FOpenTabItem(LevelEditorViewport, "LevelEditor.Tabs.Viewports", "Level Editor Viewport 1", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorViewport_Clone1, "LevelEditor.Tabs.Viewports", "Level Editor Viewport 2", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorViewport_Clone2, "LevelEditor.Tabs.Viewports", "Level Editor Viewport 3", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorViewport_Clone3, "LevelEditor.Tabs.Viewports", "Level Editor Viewport 4", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorViewport_Clone4, "LevelEditor.Tabs.Viewports", "Level Editor Viewport 5", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorToolBox, "LevelEditor.Tabs.Modes", "Active Mode Toolbox", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorSelectionDetails, "LevelEditor.Tabs.Details", "Details 1", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorSelectionDetails2, "LevelEditor.Tabs.Details", "Details 2", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorSelectionDetails3, "LevelEditor.Tabs.Details", "Details 3", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorSelectionDetails4, "LevelEditor.Tabs.Details", "Details 4", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(PlacementBrowser, "LevelEditor.Tabs.PlacementBrowser", "Active Mode Toolbar", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorBuildAndSubmit, "LevelEditor.Tabs.PlacementBrowser", "Build and Submit", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorSceneOutliner, "LevelEditor.Tabs.Outliner", "World Outliner", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorStatsViewer, "LevelEditor.Tabs.StatsViewer", "Statistics", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorLayerBrowser, "LevelEditor.Tabs.Layers", "Layers", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(Sequencer, FSlateIcon("LevelSequenceEditorStyle", "LevelSequenceEditor.Tabs.Sequencer"), "Sequencer", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(SequencerGraphEditor, "GenericCurveEditor.TabIcon", "Sequencer Curves", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(WorldSettings, "LevelEditor.WorldProperties.Tab", "World Settings", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(WorldBrowserComposition, "LevelEditor.Tabs.WorldBrowserComposition", "World Composition", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(WorldBrowserHierarchy, "LevelEditor.Tabs.WorldBrowser", "Levels", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(WorldBrowserDetails, "LevelEditor.Tabs.WorldBrowserDetails", "Level Details", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorHierarchicalLODOutliner, "LevelEditor.Tabs.HLOD", "Hierarchical LOD Outliner", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(LevelEditorEnvironmentLightingViewer, "EditorViewport.ReflectionOverrideMode", "Env. Light Mixer", LevelEditorTabManager));
#if BA_UE_VERSION_OR_LATER(5, 0) // why is this private in UE5 but not in UE4...
	TabInfos.Add(FOpenTabItem(LevelEditorDataLayerBrowser, "LevelEditor.Tabs.DataLayers", "Data Layers Outliner", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem(WorldBrowserPartitionEditor, "LevelEditor.Tabs.WorldPartition", "World Partition", LevelEditorTabManager));
#endif

	TabInfos.Add(FOpenTabItem("LevelEditorToolBar", "LevelEditor.Tabs.Toolbar", "Level Editor Toolbar", LevelEditorTabManager));

	TabInfos.Add(FOpenTabItem("EditorModeToolbar", "LevelEditor.Tabs.PlacementBrowser", "Active Mode Toolbar", LevelEditorTabManager));

	// Category Details
	TabInfos.Add(FOpenTabItem("CameraShakePreviewer", "LevelViewport.ToggleActorPilotCameraView", "Camera Shake Previewer", LevelEditorTabManager));
	TabInfos.Add(FOpenTabItem("VariantManager_VariantManagerMain", "LevelEditor.GameSettings.Small", "Variant Manager", LevelEditorTabManager));

	// Category Viewports
	TabInfos.Add(FOpenTabItem("LevelEditor", "LevelEditor.Tabs.Viewports", "Level Editor"));

	// Category Cinematics
	TabInfos.Add(FOpenTabItem("SequenceRecorder", "SequenceRecorder.TabIcon", "Sequence Recorder"));

	{
		// try to load the undo history for the undo history tab below so that it registers the nomad tab
		FModuleManager::Get().LoadModule("UndoHistoryEditor");
	}

	// Nomad unlisted tabs - search for '->RegisterNomadTabSpawner('
	TabInfos.Add(FOpenTabItem("ReferenceViewer", "Kismet.Tabs.FindResults", "Reference Viewer"));
	TabInfos.Add(FOpenTabItem("UndoHistory", "UndoHistory.TabIcon", "Undo History"));
	TabInfos.Add(FOpenTabItem("PluginsEditor", FSlateIcon("PluginStyle", "Plugins.TabIcon"), "Plugins"));
	TabInfos.Add(FOpenTabItem("PluginCreator", FSlateIcon("PluginStyle", "Plugins.TabIcon"), "New Plugin"));
	TabInfos.Add(FOpenTabItem("ProjectLauncher", "Launcher.TabIcon", "Project Launcher"));
	TabInfos.Add(FOpenTabItem("MaterialAnalyzer", "MaterialEditor.ToggleMaterialStats.Tab", "Material Analyzer"));
	TabInfos.Add(FOpenTabItem("ActorPaletteTab1", FSlateIcon("ActorPaletteStyle", "ActorPalette.TabIcon"), "Actor Palette 1"));
	TabInfos.Add(FOpenTabItem("ActorPaletteTab2", FSlateIcon("ActorPaletteStyle", "ActorPalette.TabIcon"), "Actor Palette 2"));
	TabInfos.Add(FOpenTabItem("ActorPaletteTab3", FSlateIcon("ActorPaletteStyle", "ActorPalette.TabIcon"), "Actor Palette 3"));
	TabInfos.Add(FOpenTabItem("ActorPaletteTab4", FSlateIcon("ActorPaletteStyle", "ActorPalette.TabIcon"), "Actor Palette 4"));
	TabInfos.Add(FOpenTabItem("VisualLogger", FSlateIcon("LogVisualizerStyle", "LogVisualizerApp.TabIcon"), "VisualLogger"));
	TabInfos.Add(FOpenTabItem("ConfigEditor", "ConfigEditor.TabIcon", "Config Editor"));
	TabInfos.Add(FOpenTabItem("DebuggerApp", "BlueprintDebugger.TabIcon", "Blueprint Debugger"));
	TabInfos.Add(FOpenTabItem("Search", "Symbols.SearchGlass", "Search"));
	TabInfos.Add(FOpenTabItem("LocalizationDashboard", "LocalizationDashboard.MenuIcon", "Localization Dashboard"));

	// InsightsManager.cpp 
	TabInfos.Add(FOpenTabItem("StartPage", FSlateIcon("InsightsStyle", "StartPage.Icon.Small"), "Unreal Insights"));
	TabInfos.Add(FOpenTabItem("SessionInfo", FSlateIcon("InsightsStyle", "SessionInfo.Icon.Small"), "Session"));
	TabInfos.Add(FOpenTabItem("TimingProfiler", FSlateIcon("InsightsStyle", "TimingProfiler.Icon.Small"), "Timing Insights"));
	TabInfos.Add(FOpenTabItem("LoadingProfiler", FSlateIcon("InsightsStyle", "LoadingProfiler.Icon.Small"), "Asset Loading Insights"));
	TabInfos.Add(FOpenTabItem("NetworkingProfiler", FSlateIcon("InsightsStyle", "NetworkingProfiler.Icon.Small"), "Networking Insights"));
	TabInfos.Add(FOpenTabItem("MemoryProfiler", FSlateIcon("InsightsStyle", "MemoryProfiler.Icon.Small"), "Memory Insights"));
	// TabInfos.Add(FOpenTabItem("MessageLog", FSlateIcon("InsightsStyle", "MessageLog.Icon.Small"), "Message Log"));
	TabInfos.Add(FOpenTabItem("AutomationWindow", FSlateIcon("InsightsStyle", "AutomationWindow.Icon.Small"), "Automation"));

	// Add editor specific tabs
	if (IAssetEditorInstance* Editor = FBAUtils::GetEditorFromActiveTab())
	{
		TSharedPtr<FTabManager> EditorTabManager = Editor->GetAssociatedTabManager();

		FName EditorName = Editor->GetEditorName();
		if (EditorName == "BlueprintEditor")
		{
			// BlueprintEditorTabs.cpp
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::DetailsID, "Kismet.Tabs.BlueprintDefaults", "Details", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::DefaultEditorID, "Kismet.Tabs.BlueprintDefaults", "Class Defaults", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::DebugID, "DebugTools.TabIcon", "Debug", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::PaletteID, "Kismet.Tabs.Palette", "Palette", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::BookmarksID, "Kismet.Tabs.Bookmarks", "Bookmarks", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::CompilerResultsID, "Kismet.Tabs.CompilerResults", "Compiler Results", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::FindResultsID, "Kismet.Tabs.FindResults", "Find Results", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::ConstructionScriptEditorID, "Kismet.Tabs.Components", "Components", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::SCSViewportID, "LevelEditor.Tabs.Viewports", "Viewport", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::MyBlueprintID, "ClassIcon.BlueprintCore", "My Blueprint", EditorTabManager));
			TabInfos.Add(FOpenTabItem(FBlueprintEditorTabs::ReplaceNodeReferencesID, "ClassIcon.BlueprintCore", "Replace References", EditorTabManager));
		}
		// TODO: Finish the tabs for the other types of graphs
		else if (EditorName == "Behavior Tree")
		{
			// BehaviorTreeEditorTabs.cpp
			TabInfos.Add(FOpenTabItem("BehaviorTreeEditor_Properties", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("BehaviorTreeEditor_Search", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("BehaviorTreeEditor_Blackboard", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("BehaviorTreeEditor_BlackboardEditor", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("BehaviorTreeEditor_BlackboardProperties", FName(), EditorTabManager));
		}
		else if (EditorName == "AnimationBlueprintEditor")
		{
			// AnimationBlueprintEditor.cpp
			// TabInfos.Add(FOpenTabItem("DetailsTab", FName(), EditorTabManager)); <- unused
			TabInfos.Add(FOpenTabItem("SkeletonTreeView", "Persona.Tabs.SkeletonTree", EditorTabManager));
			TabInfos.Add(FOpenTabItem("Viewport", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("AdvancedPreviewTab", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("SequenceBrowser", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("AnimBlueprintPreviewEditor", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("AnimBlueprintParentPlayerEditor", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("SkeletonSlotNames", "Persona.Tabs.AnimSlotManager", EditorTabManager));
			TabInfos.Add(FOpenTabItem("AnimCurveViewerTab", "Persona.Tabs.SkeletonCurves", EditorTabManager));

			// PersonaModule - See TabFactories
		}
		else if (EditorName == "MaterialEditor")
		{
			// MaterialEditor.cpp
			TabInfos.Add(FOpenTabItem("MaterialEditor_Preview", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialEditor_GraphCanvas", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialEditor_MaterialProperties", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialEditor_Palette", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialEditor_Find", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialEditor_PreviewSettings", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialEditor_ParameterDefaults", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialEditor_CustomPrimitiveData", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("MaterialInstanceEditor_MaterialLayerProperties", FName(), EditorTabManager));
		}
		else if (EditorName == "Niagara")
		{
			// NiagaraScriptToolkit.cpp
			TabInfos.Add(FOpenTabItem("NiagaraEditor_NodeGraph", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("NiagaraEditor_ScriptDetails", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("NiagaraEditor_SelectedDetails", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("NiagaraEditor_Parameters", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("NiagaraEditor_Paramters2", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("NiagaraEditor_Stats", FName(), EditorTabManager));
			TabInfos.Add(FOpenTabItem("NiagaraEditor_MessageLog", FName(), EditorTabManager));
		}
	}

	// Skip items which are invalid
	for (const FOpenTabItem& Item : TabInfos)
	{
		if (Item.AlternateTabManager.IsValid())
		{
			if (!Item.AlternateTabManager->HasTabSpawner(Item.TabName))
			{
				if (UBASettings_Advanced::HasDebugSetting("OpenWindowMenu"))
				{
					UE_LOG(LogBlueprintAssist, Warning, TEXT("Missing open tab alternate %s"), *Item.TabName.ToString());
				}

				continue;
			}
		}
		else
		{
			if (!FGlobalTabmanager::Get()->HasTabSpawner(Item.TabName))
			{
				if (UBASettings_Advanced::HasDebugSetting("OpenWindowMenu"))
				{
					UE_LOG(LogBlueprintAssist, Warning, TEXT("Missing open tab %s"), *Item.TabName.ToString());
				}

				continue;
			}
		}
		// if (TabInfo.TabDisplayName.IsNone())
		// {
		// 	UE_LOG(LogBA, Warning, TEXT("No display name for tab %s"), *TabInfo.TabName.ToString());
		// }
		// if (BA_STYLE_CLASS::Get().GetBrush(TabInfo.TabIcon) == FEditorStyle::GetDefaultBrush())
		// {
		// 	UE_LOG(LogBlueprintAssist, Warning, TEXT("Invalid icon for tab %s"), *TabInfo.TabName.ToString());
		// }

		Items.Add(MakeShareable(new FOpenTabItem(Item)));
	}
}

void SBAOpenWindowMenu::AddOpenSettingsItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	TSet<TSharedPtr<FOpenSettingItem>> OpenSettingsItems;

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
							OpenSettingsItems.Add(MakeShared<FOpenSettingItem>(
								Container->GetName(),
								Category->GetName(),
								Section->GetName(),
								Section->GetDisplayName().ToString()
							));
						}
					}
				}
			}
		}
	}

	for (TObjectIterator<UDeveloperSettings> SettingsIt(RF_NoFlags); SettingsIt; ++SettingsIt)
	{
		if (UDeveloperSettings* Settings = *SettingsIt)
		{
			// Only Add the CDO of any UDeveloperSettings objects.
			if (Settings->HasAnyFlags(RF_ClassDefaultObject) && !Settings->GetClass()->HasAnyClassFlags(CLASS_Deprecated | CLASS_Abstract))
			{
				// Ignore the setting if it's specifically the UDeveloperSettings or other abstract settings classes
				if (Settings->GetClass()->HasAnyClassFlags(CLASS_Abstract) || !Settings->SupportsAutoRegistration())
				{
					continue;
				}

				FName ContainerName = Settings->GetContainerName();
				FName CategoryName = Settings->GetCategoryName();
				FName SectionName = Settings->GetSectionName();

				bool bDuplicate = false;
				for (TSharedPtr<FOpenSettingItem> OpenSettingsItem : OpenSettingsItems)
				{
					if (OpenSettingsItem->SectionName == SectionName &&
						OpenSettingsItem->CategoryName == CategoryName &&
						OpenSettingsItem->ContainerName == ContainerName)
					{
						bDuplicate = true;
						break;
					}
				}

				if (!bDuplicate)
				{
					OpenSettingsItems.Add(MakeShareable(new FOpenSettingItem(ContainerName, CategoryName, SectionName)));
				}
			}
		}
	}

	for (TSharedPtr<FOpenSettingItem> Item : OpenSettingsItems)
	{
		Items.Add(Item);
	}
}

void SBAOpenWindowMenu::AddCommandItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	const FBACommandsImpl& BACommands = FBACommands::Get();
	TArray<TSharedPtr<FUICommandInfo>> Commands = {
		BACommands.AddSymbolMenu,
		BACommands.EditDetailsMenu,
		BACommands.LinkPinMenu,
		BACommands.OpenContextMenu,
		BACommands.VariableSelectorMenu,
		BACommands.OpenBlueprintAssistHotkeySheet,
		BACommands.TabSwitcherMenu,
	};

	for (TSharedPtr<FUICommandInfo> Command : Commands)
	{
		if (FBAInputProcessor::Get().CanExecuteCommand(Command.ToSharedRef()))
		{
			TSharedPtr<FExecuteCommandItem> CommandItem = MakeShared<FExecuteCommandItem>(Command);
			CommandItem->AvailableLists = FBAInputProcessor::Get().GetCommandLists();
			Items.Add(CommandItem);
		}
	}

	AddEditorCommandItems(Items);
}

void SBAOpenWindowMenu::AddEditorCommandItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	TArray<TSharedPtr<FUICommandList>> AvailableLists;

	// FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	// AvailableLists.Add(LevelEditorModule.GetGlobalLevelEditorActions());

	// FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	// AvailableLists.Add(BlueprintEditorModule.GetsSharedBlueprintEditorCommands());

	IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");
	AvailableLists.Add(MainFrame.GetMainFrameCommandBindings());

	// Get all command infos
	const FInputBindingManager& InputBindingManager = FInputBindingManager::Get();

	TArray<TSharedPtr<FBindingContext>> AllBindingContexts;
	InputBindingManager.GetKnownInputContexts(AllBindingContexts);

	for (TSharedPtr<FBindingContext> BindingContext : AllBindingContexts)
	{
		TArray<TSharedPtr<FUICommandInfo>> LocalCommandInfos;
		InputBindingManager.GetCommandInfosFromContext(BindingContext->GetContextName(), LocalCommandInfos);
		for (TSharedPtr<FUICommandInfo> CommandInfo : LocalCommandInfos)
		{
			for (TSharedPtr<FUICommandList> CommandList : AvailableLists)
			{
				TSharedPtr<FExecuteCommandItem> CommandItem = MakeShareable(new FExecuteCommandItem(CommandInfo));

				if (CommandList->GetActionForCommand(CommandInfo))
				{
					UE_LOG(LogBlueprintAssist, Verbose, TEXT("Added command %s"), *CommandItem->ToString());
					CommandItem->AvailableLists = AvailableLists;
					Items.Add(CommandItem);
					break;
				}
			}
		}
	}
}

void SBAOpenWindowMenu::AddToolItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	TArray<UToolMenu*> AllMenus;
	// AllMenus.Append(ToolMenus->CollectHierarchy("LevelEditor.MainMenu"));
	// AllMenus.Append(ToolMenus->CollectHierarchy("MainFrame.MainTabMenu.File"));
	AllMenus.Append(ToolMenus->CollectHierarchy("LevelEditor.MainMenu.File"));

	// TODO: Need to figure out how to pick / generate the MenuContext to be used for the command
	UToolMenu* ParentContext = ToolMenus->FindMenu("MainFrame.MainTabMenu.File");
	// if (!ParentContext)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("No parent context"));
	// }
	// AllMenus.Append(ToolMenus->CollectHierarchy("LevelEditor.MainMenu.File"));
	// AllMenus.Append(ToolMenus->CollectHierarchy("MainFrame.NomadMainMenu"));

	for (UToolMenu* Menu : AllMenus)
	{
		// UE_LOG(LogTemp, Warning, TEXT("Found Menu %s"), *Menu->GetMenuName().ToString());
		for (FToolMenuSection& Section : Menu->Sections)
		{
			// UE_LOG(LogTemp, Warning, TEXT("\tSection %s"), *Section.Name.ToString());

			for (FToolMenuEntry& Block : Section.Blocks)
			{
				// UE_LOG(LogTemp, Warning, TEXT("\t\tBlock %s %s"), *Block.Label.Get().ToString(), *Block.Name.ToString());
				TSharedPtr<FToolsMenuItem> ToolsMenuItem = MakeShareable(new FToolsMenuItem());
				ToolsMenuItem->Menu = Menu;
				ToolsMenuItem->Entry = &Block;
				ToolsMenuItem->Section = &Section;
				ToolsMenuItem->ParentContext = ParentContext;
				Items.Add(ToolsMenuItem);
			}
		}
	}
}

#if BA_UE_VERSION_OR_LATER(5, 0)
void SBAOpenWindowMenu::AddWorkspaceMenuStructure(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items, TSet<FName>& ExistingTabNames)
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TArray<TSharedRef<FWorkspaceItem>> PendingItems;
	PendingItems.Add(MenuStructure.GetStructureRoot());
	PendingItems.Add(MenuStructure.GetLevelEditorCategory());
	PendingItems.Add(MenuStructure.GetLevelEditorViewportsCategory());
	PendingItems.Add(MenuStructure.GetLevelEditorDetailsCategory());
	PendingItems.Add(MenuStructure.GetLevelEditorCinematicsCategory());
	PendingItems.Add(MenuStructure.GetLevelEditorModesCategory());
	PendingItems.Add(MenuStructure.GetToolsCategory());
	PendingItems.Add(MenuStructure.GetDeveloperToolsDebugCategory());
	PendingItems.Add(MenuStructure.GetDeveloperToolsLogCategory());
	PendingItems.Add(MenuStructure.GetDeveloperToolsProfilingCategory());
	PendingItems.Add(MenuStructure.GetDeveloperToolsMiscCategory());
	PendingItems.Add(MenuStructure.GetAutomationToolsCategory());
	PendingItems.Add(MenuStructure.GetEditOptions());

	PendingItems.Add(MenuStructure.GetToolsStructureRoot());
	PendingItems.Add(MenuStructure.GetLevelEditorVirtualProductionCategory());
	PendingItems.Add(MenuStructure.GetLevelEditorWorldPartitionCategory());
	PendingItems.Add(MenuStructure.GetDeveloperToolsAuditCategory());
	PendingItems.Add(MenuStructure.GetDeveloperToolsPlatformsCategory());

	TSet<TSharedPtr<FWorkspaceItem>> VisitedItems;
	while (PendingItems.Num() > 0)
	{
		TSharedRef<FWorkspaceItem> CurrentItem = PendingItems.Pop();

		if (TSharedPtr<FTabSpawnerEntry> TabSpawnerEntry = CurrentItem->AsSpawnerEntry())
		{
			if (!TabSpawnerEntry->GetDisplayName().IsEmptyOrWhitespace() && !ExistingTabNames.Contains(TabSpawnerEntry->GetTabType()))
			{
				ExistingTabNames.Add(TabSpawnerEntry->GetTabType());

				TSharedPtr<FOpenTabSpawnerItem> NewItem = MakeShareable(new FOpenTabSpawnerItem(TabSpawnerEntry));
				Items.Add(NewItem);
				UE_LOG(LogBlueprintAssist, Verbose, TEXT("WORKSPACE ITEM %s %s"), *NewItem->GetTabName().ToString(), *TabSpawnerEntry->GetDisplayName().ToString());
			}
		}

		if (VisitedItems.Contains(CurrentItem))
		{
			continue;
		}
		VisitedItems.Add(CurrentItem);

		TArray<TSharedRef<FWorkspaceItem>> ChildItems = CurrentItem->GetChildItems();
		PendingItems.Append(ChildItems);
	}
}

void SBAOpenWindowMenu::AddTabManagerItems(TSharedPtr<FTabManager> TabManager, TSet<FName>& AddedTabs, TArray<TSharedPtr<FOpenWindowItem_Base>>& OutItems)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	FTabManager& TabManagerRef = *TabManager.Get();
	TSharedRef<FTabSpawner> NomadTabSpawner = TabManagerRef.*GNomadTabSpawner;

	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();

	for (auto& Elem : NomadTabSpawner.Get())
	{
		if (AddedTabs.Contains(Elem.Value->GetTabType()))
		{
			continue;
		}

		if (EditorUtilitySubsystem)
		{
			if (EditorUtilitySubsystem->RegisteredTabs.Contains(Elem.Value->GetTabType()))
			{
				UE_LOG(LogTemp, Warning, TEXT("Skipping %s"), *Elem.Value->GetTabType().ToString())
				continue;
			}
		}

		AddedTabs.Add(Elem.Value->GetTabType());

		OutItems.Add(MakeShareable(new FOpenTabSpawnerItem(Elem.Value, TabManager)));

		// UE_LOG(LogBlueprintAssist, Log, TEXT("\tADD NOMAD TAB %s %s"), *Elem.Key.ToString(), *Elem.Value->GetDisplayName().ToString());
	}

	FTabSpawner& TabSpawner = TabManagerRef.*GTabSpawner;
	for (auto& Elem : TabSpawner)
	{
		if (Elem.Value->GetDisplayName().IsEmptyOrWhitespace())
		{
			UE_LOG(LogBlueprintAssist, Verbose, TEXT("Tab %s has null tab type?"), *Elem.Key.ToString());
			continue;
		}

		if (AddedTabs.Contains(Elem.Value->GetTabType()))
		{
			continue;
		}

		if (EditorUtilitySubsystem)
		{
			if (EditorUtilitySubsystem->RegisteredTabs.Contains(Elem.Value->GetTabType()))
			{
				continue;
			}
		}

		AddedTabs.Add(Elem.Value->GetTabType());

		OutItems.Add(MakeShareable(new FOpenTabSpawnerItem(Elem.Value, TabManager)));
		// UE_LOG(LogBlueprintAssist, Log, TEXT("\tADD TAB %s %s"), *Elem.Key.ToString(), *Elem.Value->GetDisplayName().ToString());
	}
}
#endif

void SBAOpenWindowMenu::AddActionItems(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	const auto ShowEditorPreferences = FExecuteAction::CreateLambda([]()
	{
		FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Editor", "General", "Appearance");
	});

	Items.Add(MakeShared<FOpenWindowItem_Action>("Editor Preferences", ShowEditorPreferences));

	const auto ShowProjectSettings = FExecuteAction::CreateLambda([]()
	{
		FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Project", "Project", "General");
	});

	Items.Add(MakeShared<FOpenWindowItem_Action>("Project Settings", ShowProjectSettings));
}

void SBAOpenWindowMenu::AddMenuEntryBlocks(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();

	TArray<TSharedPtr<SWidget>> MenuEntries;
	FBAUtils::GetChildWidgets(Window, "SMenuEntryBlock", MenuEntries);

	for (auto Entry : MenuEntries)
	{
		if (TSharedPtr<SButton> Button = FIND_CHILD_WIDGET(Entry, SButton))
		{
			Items.Add(MakeShared<FWidgetItem>(Button));
		}
	}
}

void SBAOpenWindowMenu::AddEditorUtilityWidgets(TArray<TSharedPtr<FOpenWindowItem_Base>>& Items)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> EditorUtilsAssets;

#if BA_UE_VERSION_OR_LATER(5, 1)
	FARFilter ClassFilter;
	ClassFilter.bRecursiveClasses = true;
	ClassFilter.bRecursivePaths = true;
	ClassFilter.ClassPaths.Add(UEditorUtilityBlueprint::StaticClass()->GetClassPathName());
	ClassFilter.ClassPaths.Add(UEditorUtilityWidgetBlueprint::StaticClass()->GetClassPathName());
	AssetRegistry.GetAssets(ClassFilter, EditorUtilsAssets);
#elif BA_UE_VERSION_OR_LATER(5, 0)
	FARFilter ClassFilter;
	ClassFilter.ClassNames.Add(UEditorUtilityWidgetBlueprint::StaticClass()->GetFName());
	AssetRegistry.GetAssets(ClassFilter, EditorUtilsAssets);
#else
	TArray<const FAssetData*> Assets = AssetRegistry.GetAssetRegistryState()->GetAssetsByClassName(UEditorUtilityWidgetBlueprint::StaticClass()->GetFName());
	for (const FAssetData* Asset : Assets)
	{
		FAssetData NewAsset = *Asset;
		EditorUtilsAssets.Add(NewAsset);
	}
#endif

	for (const FAssetData& EditorAsset : EditorUtilsAssets)
	{
		Items.Add(MakeShared<FEditorUtilityItem>(EditorAsset));
	}
}

TSharedRef<ITableRow> SBAOpenWindowMenu::CreateItemWidget(TSharedPtr<FOpenWindowItem_Base> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	float VerticalPadding = 4.0f;

	TSharedRef<SHorizontalBox> TextHBox =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(STextBlock).Text(FText::FromString(Item->ToString()))
		];

	if (TOptional<FString> KeyboardShortcut = Item->GetKeyboardShortcut())
	{
		TextHBox->AddSlot().Padding(8, 0).VAlign(VAlign_Center).AutoWidth().AttachWidget(SNew(STextBlock)
			.Font(BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.BoldFont")))
			.Text(FText::FromString(KeyboardShortcut.GetValue())));
	}

	TSharedRef<SVerticalBox> TextVBox =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			TextHBox
		];

	FText RowTooltip;

	if (const FString* DetailsString = Item->GetDetailsString())
	{
		TextVBox->AddSlot().AttachWidget(SNew(STextBlock)
			.Font(BA_STYLE_CLASS::Get().GetFontStyle("ContentBrowser.AssetListViewClassFont"))
			.Text(FText::FromString(*DetailsString))
		);

		VerticalPadding = 2.0f;

		RowTooltip = FText::FromString(*DetailsString);
	}

	TSharedRef<SHorizontalBox> RowContents = SNew(SHorizontalBox);
	const FSlateBrush* ContextIcon = Item->GetIcon();

	if (!ContextIcon || ContextIcon == FStyleDefaults::GetNoBrush())
	{
		static const FSlateBrush* IconHelp = BA_STYLE_CLASS::Get().GetBrush("Icons.Help");
		ContextIcon = IconHelp;
	}

	if (ContextIcon)
	{
		RowContents->AddSlot().MaxWidth(16).AutoWidth().VAlign(VAlign_Center).Padding(0, 0, ContextIcon != nullptr ? 5 : 0, 0)
		[
			SNew(SImage).Image(ContextIcon)
		];
	}

	RowContents->AddSlot().AutoWidth().VAlign(VAlign_Center).HAlign(HAlign_Left).FillWidth(1)
	[
		TextVBox
	];

	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable).Padding(FMargin(2.0f, VerticalPadding)).ToolTipText(RowTooltip)
	[
		RowContents
	];
}

void SBAOpenWindowMenu::SelectItem(TSharedPtr<FOpenWindowItem_Base> Item)
{
	Item->SelectItem();
}
