// Copyright 2024 Gradess Games. All Rights Reserved.


#include "BlueprintScreenshotToolCommandManager.h"
#include "BlueprintScreenshotToolCommands.h"
#include "BlueprintScreenshotToolHandler.h"
#include "BlueprintEditorModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

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
	// Kismet 模块的 StartupModule 含 check(GEditor)（BlueprintEditorModule.cpp:202），
	// 本模块为 Default 加载阶段，此时 GEditor 尚未创建，强制加载 Kismet 会导致引擎启动崩溃；
	// 延迟到 OnPostEngineInit（届时 GEditor 已就绪）再注册。
	if (!GEngine || !GEngine->IsInitialized())
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FBlueprintScreenshotToolCommandManager::RegisterToolbarExtension);
		return;
	}

	// The Blueprint Editor owns this extensibility manager and applies its extenders to its toolbar.
	auto ExtensibilityManager = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet").GetMenuExtensibilityManager();
	if (!ExtensibilityManager.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("XTools_BlueprintScreenshotTool: Blueprint Editor extensibility manager is invalid, skip toolbar extension registration."));
		return;
	}
	
	ToolbarExtension = MakeShareable(new FExtender());
	ToolbarExtension->AddToolBarExtension("Asset", EExtensionHook::After, CommandList, FToolBarExtensionDelegate::CreateStatic(&FBlueprintScreenshotToolCommandManager::AddToolbarExtension));
	ExtensibilityManager->AddExtender(ToolbarExtension);
}

void FBlueprintScreenshotToolCommandManager::UnregisterToolbarExtension()
{
	// 引擎初始化前模块即被卸载时，清理挂起的延迟注册委托，避免悬空回调
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	if (!ToolbarExtension.IsValid())
	{
		return;
	}

	if (FBlueprintEditorModule* BlueprintEditorModule = FModuleManager::GetModulePtr<FBlueprintEditorModule>("Kismet"))
	{
		if (const TSharedPtr<FExtensibilityManager> ExtensibilityManager = BlueprintEditorModule->GetMenuExtensibilityManager())
		{
			ExtensibilityManager->RemoveExtender(ToolbarExtension);
		}
	}

	ToolbarExtension.Reset();
}

void FBlueprintScreenshotToolCommandManager::AddToolbarExtension(FToolBarBuilder& ToolBarBuilder)
{
	ToolBarBuilder.AddToolBarButton(FBlueprintScreenshotToolCommands::Get().TakeScreenshot);
}
