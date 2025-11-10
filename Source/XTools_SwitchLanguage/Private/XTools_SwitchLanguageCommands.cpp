/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "XTools_SwitchLanguageCommands.h"

#define LOCTEXT_NAMESPACE "FXTools_SwitchLanguageModule"

void FXTools_SwitchLanguageCommands::RegisterCommands()
{
	UI_COMMAND(
		PluginAction,
		"SwitchLanguage",
		"Toggle between English and Chinese",
		EUserInterfaceActionType::Button,
		FInputChord()  // 无快捷键
	);
}

#undef LOCTEXT_NAMESPACE
