// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialFunction.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Application/SlateApplication.h"

class UMaterialFunctionInterface;
class SWindow;
class SButton;
class STextBlock;

/**
 * 材质函数选择回调
 */
DECLARE_DELEGATE_OneParam(FOnMaterialFunctionSelected, UMaterialFunctionInterface*);

/**
 * 材质节点选择回调
 */
DECLARE_DELEGATE_OneParam(FOnMaterialNodeSelected, FName);

/**
 * 材质节点选择器部件
 */
class SX_MaterialNodePicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SX_MaterialNodePicker)
    {}
        SLATE_EVENT(FOnMaterialNodeSelected, OnNodeSelected)
    SLATE_END_ARGS()

    /**
     * 构造函数
     */
    void Construct(const FArguments& InArgs);

    /**
     * 创建节点选择器窗口
     * @param OnNodeSelected - 选择回调
     * @return 窗口引用
     */
    static TSharedRef<SWindow> CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected);

private:
    /**
     * 生成节点列表项
     * @param NodeName - 节点名称
     * @return 窗口部件
     */
    TSharedRef<SWidget> GenerateNodeItem(TSharedPtr<FName> NodeName);

    /**
     * 节点选择处理
     * @param NodeName - 选中的节点名称
     * @return 回复
     */
    FReply OnNodeSelected(TSharedPtr<FName> NodeName);

private:
    /** 节点选择回调 */
    FOnMaterialNodeSelected OnNodeSelectedDelegate;

    /** 节点名称列表 */
    TArray<TSharedPtr<FName>> NodeNames;

    /** 节点列表滚动框 */
    TSharedPtr<SScrollBox> NodeListBox;
};

/**
 * 材质函数UI管理类
 */
class X_ASSETEDITOR_API FX_MaterialFunctionUI
{
public:
    /**
     * 创建材质函数选择器窗口
     * 使用ContentBrowser的资产选择器实现
     * @param OnFunctionSelected - 选择回调
     * @return 窗口引用
     */
    static TSharedRef<SWindow> CreateMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected);

    /**
     * 创建节点选择器窗口
     * @param OnNodeSelected - 选择回调
     * @return 窗口引用
     */
    static TSharedRef<SWindow> CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected);

    /**
     * 获取常用节点名称
     * @return 节点名称列表
     */
    static TArray<TSharedPtr<FName>> GetCommonNodeNames();
}; 