/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * 语言切换插件样式管理
 * 负责加载和管理工具栏图标资源
 */
class FXTools_SwitchLanguageStyle
{
public:
	/** 初始化样式 */
	static void Initialize();

	/** 关闭样式 */
	static void Shutdown();

	/** 重新加载纹理资源 */
	static void ReloadTextures();

	/** 获取样式集 */
	static const ISlateStyle& Get();

	/** 获取样式集名称 */
	static FName GetStyleSetName();

private:
	/** 创建样式集 */
	static TSharedRef<class FSlateStyleSet> Create();

private:
	/** 样式集实例 */
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
