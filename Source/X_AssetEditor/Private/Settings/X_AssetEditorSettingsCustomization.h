/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;

/**
 * 自定义设置面板
 * 控制分类展开状态（默认展开）并添加重置按钮
 */
class FX_AssetEditorSettingsCustomization : public IDetailCustomization
{
public:
	/** 创建自定义实例 */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization 接口 */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** 处理重置前缀映射按钮点击 */
	FReply OnResetPrefixMappingsClicked();

	/** 缓存的DetailBuilder用于刷新 */
	IDetailLayoutBuilder* CachedDetailBuilder = nullptr;
};

