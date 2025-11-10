/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "XTools_SwitchLanguageStyle.h"

/**
 * 语言切换工具栏命令定义
 */
class FXTools_SwitchLanguageCommands : public TCommands<FXTools_SwitchLanguageCommands>
{
public:
	FXTools_SwitchLanguageCommands()
		: TCommands<FXTools_SwitchLanguageCommands>(
			TEXT("XTools_SwitchLanguage"),
			NSLOCTEXT("Contexts", "XTools_SwitchLanguage", "XTools SwitchLanguage Plugin"),
			NAME_None,
			FXTools_SwitchLanguageStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	/** 语言切换按钮命令 */
	TSharedPtr<FUICommandInfo> PluginAction;
};
