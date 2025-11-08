/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "Framework/Text/SlateHyperlinkRun.h"

/**
 * XTools 欢迎/更新弹窗
 * 在插件首次打开或更新时显示欢迎信息和更新日志
 */
class XTOOLS_API FXToolsWelcomePopup
{
public:
	/** 注册弹窗（在模块启动时调用） */
	static void Register();

	/** 打开弹窗 */
	static void Open();

	/** 超链接点击回调 */
	static void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);
};



