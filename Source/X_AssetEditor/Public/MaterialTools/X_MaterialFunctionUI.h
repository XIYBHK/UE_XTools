/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialFunction.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Framework/Application/SlateApplication.h"

class UMaterialFunctionInterface;
class SWindow;
class SButton;
class STextBlock;
class SSearchBox;

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
    TSharedRef<ITableRow> GenerateNodeItem(TSharedPtr<FName> NodeName, const TSharedRef<STableViewBase>& OwnerTable);

    void RefreshFilteredNodeNames();
    void ConfirmSelection();
    void HandleSearchTextChanged(const FText& InSearchText);
    void HandleNodeSelectionChanged(TSharedPtr<FName> NodeName, ESelectInfo::Type SelectInfo);
    void HandleNodeDoubleClicked(TSharedPtr<FName> NodeName);
    FReply OnConfirmClicked();
    FReply OnCancelClicked();

private:
    /** 节点选择回调 */
    FOnMaterialNodeSelected OnNodeSelectedDelegate;

    /** 节点名称列表 */
    TArray<TSharedPtr<FName>> NodeNames;

    /** 过滤后的节点名称列表 */
    TArray<TSharedPtr<FName>> FilteredNodeNames;

    /** 当前搜索文本 */
    FString SearchText;

    /** 当前待确认节点 */
    TSharedPtr<FName> PendingSelection;

    /** 节点搜索框 */
    TSharedPtr<SSearchBox> SearchBox;

    /** 节点列表视图 */
    TSharedPtr<SListView<TSharedPtr<FName>>> NodeListView;
};

/**
 * 材质函数UI管理类
 */
class X_ASSETEDITOR_API FX_MaterialFunctionUI
{
public:
    /**
     * 创建材质函数选择器窗口
     * 兼容旧接口。当前行为为显示模态窗口并返回窗口引用。
     * @param OnFunctionSelected - 选择回调
     * @return 窗口引用
     */
    static TSharedRef<SWindow> CreateMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected);

    /**
     * 显示材质函数选择器窗口
     * @param OnFunctionSelected - 选择回调
     * @return 窗口引用
     */
    static TSharedRef<SWindow> ShowMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected);

    /**
     * 创建节点选择器窗口
     * 仅创建窗口，不负责显示。
     * @param OnNodeSelected - 选择回调
     * @return 窗口引用
     */
    static TSharedRef<SWindow> CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected);

    /**
     * 显示节点选择器窗口
     * @param OnNodeSelected - 选择回调
     * @return 窗口引用
     */
    static TSharedRef<SWindow> ShowNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected);

    /**
     * 获取常用节点名称
     * @return 节点名称列表
     */
    static TArray<TSharedPtr<FName>> GetCommonNodeNames();
}; 
