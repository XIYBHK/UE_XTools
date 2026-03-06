// Copyright 2024 Gradess Games. All Rights Reserved.


#include "BlueprintScreenshotToolCommandManager.h"
#include "BlueprintScreenshotToolCommands.h"
#include "BlueprintScreenshotToolHandler.h"
#include "Interfaces/IMainFrameModule.h"
#include "Toolkits/AssetEditorToolkit.h"

void FBlueprintScreenshotToolCommandManager::RegisterCommands()
{
	FBlueprintScreenshotToolCommands::Register();

	MapCommands();
	RegisterToolbarExtension();
}

void FBlueprintScreenshotToolCommandManager::UnregisterCommands()
{
	UnregisterToolbarExtension();
	if (const TSharedPtr<FUICommandList> MainFrameCommandList = MainFrameCommands.Pin())
	{
		MainFrameCommandList->UnmapAction(FBlueprintScreenshotToolCommands::Get().TakeScreenshot);
		MainFrameCommandList->UnmapAction(FBlueprintScreenshotToolCommands::Get().OpenDirectory);
	}

	MainFrameCommands.Reset();
	FBlueprintScreenshotToolCommands::Unregister();
	CommandList.Reset();
}

void FBlueprintScreenshotToolCommandManager::OnTakeScreenshot()
{
}

void FBlueprintScreenshotToolCommandManager::MapCommands()
{
	CommandList = MakeShareable(new FUICommandList());
	CommandList->MapAction(
		FBlueprintScreenshotToolCommands::Get().TakeScreenshot,
		FExecuteAction::CreateStatic(UBlueprintScreenshotToolHandler::TakeScreenshot),
		FCanExecuteAction());

	CommandList->MapAction(
		FBlueprintScreenshotToolCommands::Get().OpenDirectory,
		FExecuteAction::CreateStatic(UBlueprintScreenshotToolHandler::OpenDirectory),
		FCanExecuteAction());

	const auto& EditorCommandList = IMainFrameModule::Get().GetMainFrameCommandBindings();
	EditorCommandList->Append(CommandList.ToSharedRef());
	MainFrameCommands = EditorCommandList;
}

void FBlueprintScreenshotToolCommandManager::RegisterToolbarExtension()
{
	auto ExtensibilityManager = FAssetEditorToolkit::GetSharedToolBarExtensibilityManager();
	if (!ExtensibilityManager.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("XTools_BlueprintScreenshotTool: ToolBarExtensibilityManager is invalid, skip toolbar extension registration."));
		return;
	}
	
	ToolbarExtension = MakeShareable(new FExtender());
	ToolbarExtension->AddToolBarExtension("Asset", EExtensionHook::After, CommandList, FToolBarExtensionDelegate::CreateStatic(&FBlueprintScreenshotToolCommandManager::AddToolbarExtension));
	ExtensibilityManager->AddExtender(ToolbarExtension);
}

void FBlueprintScreenshotToolCommandManager::UnregisterToolbarExtension()
{
	if (!ToolbarExtension.IsValid())
	{
		return;
	}

	const auto ExtensibilityManager = FAssetEditorToolkit::GetSharedToolBarExtensibilityManager();
	if (ExtensibilityManager.IsValid())
	{
		ExtensibilityManager->RemoveExtender(ToolbarExtension);
	}

	ToolbarExtension.Reset();
}

void FBlueprintScreenshotToolCommandManager::AddToolbarExtension(FToolBarBuilder& ToolBarBuilder)
{
	ToolBarBuilder.AddToolBarButton(FBlueprintScreenshotToolCommands::Get().TakeScreenshot);
}
