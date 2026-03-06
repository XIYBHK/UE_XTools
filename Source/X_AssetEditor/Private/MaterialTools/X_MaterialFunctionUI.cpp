#include "MaterialTools/X_MaterialFunctionUI.h"

#include "Materials/MaterialFunction.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Logging/LogMacros.h"
#include "MaterialTools/X_MaterialFunctionCore.h"
#include "X_AssetEditor.h"

// 内容浏览器相关
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"

// UE核心
#include "Engine/Engine.h"
#include "Engine/Selection.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "X_MaterialFunctionUI"

void SX_MaterialNodePicker::Construct(const FArguments& InArgs)
{
    OnNodeSelectedDelegate = InArgs._OnNodeSelected;
    NodeNames = FX_MaterialFunctionUI::GetCommonNodeNames();
    RefreshFilteredNodeNames();
    if (FilteredNodeNames.Num() > 0)
    {
        PendingSelection = FilteredNodeNames[0];
    }

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 8.0f, 8.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("SelectNodeTitle", "选择目标节点"))
            .Font(FAppStyle::Get().GetFontStyle("HeadingFont"))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f, 8.0f, 4.0f)
        [
            SAssignNew(SearchBox, SSearchBox)
            .HintText(LOCTEXT("NodeSearchHint", "搜索目标节点"))
            .OnTextChanged(this, &SX_MaterialNodePicker::HandleSearchTextChanged)
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SAssignNew(NodeListView, SListView<TSharedPtr<FName>>)
            .ListItemsSource(&FilteredNodeNames)
            .SelectionMode(ESelectionMode::Single)
            .OnGenerateRow(this, &SX_MaterialNodePicker::GenerateNodeItem)
            .OnSelectionChanged(this, &SX_MaterialNodePicker::HandleNodeSelectionChanged)
            .OnMouseButtonDoubleClick(this, &SX_MaterialNodePicker::HandleNodeDoubleClicked)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .HAlign(HAlign_Right)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
                    .ForegroundColor(FLinearColor::White)
                    .ContentPadding(FMargin(8, 2))
                    .IsEnabled_Lambda([this]()
                    {
                        return PendingSelection.IsValid();
                    })
                    .OnClicked(this, &SX_MaterialNodePicker::OnConfirmClicked)
                    [
                        SNew(STextBlock)
                        .TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
                        .Text(LOCTEXT("NodePickerConfirm", "确定"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "FlatButton")
                    .ForegroundColor(FLinearColor::White)
                    .ContentPadding(FMargin(8, 2))
                    .OnClicked(this, &SX_MaterialNodePicker::OnCancelClicked)
                    [
                        SNew(STextBlock)
                        .TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
                        .Text(LOCTEXT("NodePickerCancel", "取消"))
                    ]
                ]
            ]
        ]
    ];

    if (NodeListView.IsValid() && PendingSelection.IsValid())
    {
        NodeListView->SetSelection(PendingSelection);
    }

    if (SearchBox.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
    }
}

TSharedRef<SWindow> SX_MaterialNodePicker::CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected)
{
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("MaterialNodePickerTitle", "选择目标节点"))
        .ClientSize(FVector2D(300, 400))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    
    Window->SetContent(
        SNew(SX_MaterialNodePicker)
        .OnNodeSelected(OnNodeSelected)
    );
    
    return Window;
}

TSharedRef<ITableRow> SX_MaterialNodePicker::GenerateNodeItem(TSharedPtr<FName> NodeName, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FName>>, OwnerTable)
        [
            SNew(STextBlock)
            .Text(NodeName.IsValid() ? FText::FromName(*NodeName) : FText::GetEmpty())
            .Font(FAppStyle::Get().GetFontStyle("NormalFont"))
        ];
}

void SX_MaterialNodePicker::RefreshFilteredNodeNames()
{
    FilteredNodeNames.Reset();

    const FString SearchLower = SearchText.ToLower();
    for (const TSharedPtr<FName>& NodeName : NodeNames)
    {
        if (!NodeName.IsValid())
        {
            continue;
        }

        const FString NodeText = NodeName->ToString();
        if (SearchLower.IsEmpty() || NodeText.ToLower().Contains(SearchLower))
        {
            FilteredNodeNames.Add(NodeName);
        }
    }

    if (!FilteredNodeNames.Contains(PendingSelection))
    {
        PendingSelection = FilteredNodeNames.Num() > 0 ? FilteredNodeNames[0] : nullptr;
    }
}

void SX_MaterialNodePicker::ConfirmSelection()
{
    if (!PendingSelection.IsValid())
    {
        return;
    }

    if (OnNodeSelectedDelegate.IsBound())
    {
        OnNodeSelectedDelegate.Execute(*PendingSelection);
    }

    if (TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
    {
        Window->RequestDestroyWindow();
    }
}

void SX_MaterialNodePicker::HandleSearchTextChanged(const FText& InSearchText)
{
    SearchText = InSearchText.ToString();
    RefreshFilteredNodeNames();

    if (NodeListView.IsValid())
    {
        NodeListView->RequestListRefresh();
        if (PendingSelection.IsValid())
        {
            NodeListView->SetSelection(PendingSelection);
            NodeListView->RequestScrollIntoView(PendingSelection);
        }
    }
}

void SX_MaterialNodePicker::HandleNodeSelectionChanged(TSharedPtr<FName> NodeName, ESelectInfo::Type SelectInfo)
{
    PendingSelection = NodeName;
}

void SX_MaterialNodePicker::HandleNodeDoubleClicked(TSharedPtr<FName> NodeName)
{
    PendingSelection = NodeName;
    ConfirmSelection();
}

FReply SX_MaterialNodePicker::OnConfirmClicked()
{
    ConfirmSelection();
    return FReply::Handled();
}

FReply SX_MaterialNodePicker::OnCancelClicked()
{
    if (TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
    {
        Window->RequestDestroyWindow();
    }

    return FReply::Handled();
}

// 使用新版的资产选择器实现
TSharedRef<SWindow> FX_MaterialFunctionUI::CreateMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected)
{
    return ShowMaterialFunctionPickerWindow(OnFunctionSelected);
}

TSharedRef<SWindow> FX_MaterialFunctionUI::ShowMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected)
{
    // 打印日志，标明使用了哪个方法
    UE_LOG(LogX_AssetEditor, Verbose, TEXT("FX_MaterialFunctionUI::ShowMaterialFunctionPickerWindow 调用"));
    
    // 创建窗口
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("MaterialFunctionPickerTitle", "选择材质函数"))
        .ClientSize(FVector2D(400, 600))
        .SupportsMaximize(false)
        .SupportsMinimize(false);

    const TWeakPtr<SWindow> WeakWindow = Window;
    TSharedPtr<FAssetData> PendingSelection = MakeShared<FAssetData>();

    auto ConfirmSelection = [OnFunctionSelected, WeakWindow, PendingSelection]()
    {
        if (!PendingSelection.IsValid() || !PendingSelection->IsValid())
        {
            return;
        }

        UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(PendingSelection->GetAsset());
        if (MaterialFunction && OnFunctionSelected.IsBound())
        {
            OnFunctionSelected.Execute(MaterialFunction);
        }

        if (const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin())
        {
            PinnedWindow->RequestDestroyWindow();
        }
    };

    // 配置资产选择器
    FAssetPickerConfig AssetPickerConfig;
    AssetPickerConfig.Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
    AssetPickerConfig.Filter.bRecursiveClasses = true;
    AssetPickerConfig.Filter.bRecursivePaths = true;      // 递归搜索路径
    AssetPickerConfig.bAllowNullSelection = false;
    AssetPickerConfig.bCanShowFolders = true;
    AssetPickerConfig.bCanShowClasses = true;
    AssetPickerConfig.bShowTypeInColumnView = true;
    AssetPickerConfig.bShowPathInColumnView = true;
    AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

    // 配置内容显示选项 - 默认只显示项目内容
    AssetPickerConfig.bForceShowEngineContent = false;    // 不强制显示引擎内容
    AssetPickerConfig.bForceShowPluginContent = false;    // 不强制显示插件内容
    AssetPickerConfig.bCanShowDevelopersFolder = true;    // 允许显示开发者文件夹（但默认不显示）

    // 启用过滤器UI和其他选项
    AssetPickerConfig.bAddFilterUI = true;               // 启用过滤器UI
    AssetPickerConfig.bCanShowRealTimeThumbnails = true;
    AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
    AssetPickerConfig.SaveSettingsName = TEXT("MaterialFunctionPicker"); // 保存设置

    // 单击仅选中，避免误触就执行操作；双击作为确认行为
    AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(
        [PendingSelection](const FAssetData& AssetData)
        {
            *PendingSelection = AssetData;
            if (const UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(AssetData.GetAsset()))
            {
                UE_LOG(LogX_AssetEditor, Verbose, TEXT("材质函数已选中(待确认): %s"), *MaterialFunction->GetName());
            }
        });

    // 配置双击回调
    AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateLambda(
        [PendingSelection, ConfirmSelection](const FAssetData& AssetData)
        {
            *PendingSelection = AssetData;
            ConfirmSelection();
        });

    // 创建资产选择器小部件
    TSharedRef<SWidget> AssetPickerWidget =
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBox)
            .WidthOverride(400.0f)
            .HeightOverride(560.0f)
            [
                FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().CreateAssetPicker(AssetPickerConfig)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.0f, 6.0f, 8.0f, 8.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .HAlign(HAlign_Right)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
                    .ForegroundColor(FLinearColor::White)
                    .ContentPadding(FMargin(8, 2))
                    .IsEnabled_Lambda([PendingSelection]()
                    {
                        return PendingSelection.IsValid() && PendingSelection->IsValid();
                    })
                    .OnClicked_Lambda([ConfirmSelection]()
                    {
                        ConfirmSelection();
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
                        .Text(LOCTEXT("MaterialFunctionPickerConfirm", "确定"))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "FlatButton")
                    .ForegroundColor(FLinearColor::White)
                    .ContentPadding(FMargin(8, 2))
                    .OnClicked_Lambda([WeakWindow]()
                    {
                        if (const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin())
                        {
                            PinnedWindow->RequestDestroyWindow();
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
                        .Text(LOCTEXT("MaterialFunctionPickerCancel", "取消"))
                    ]
                ]
            ]
        ];

    // 设置窗口内容
    Window->SetContent(AssetPickerWidget);

    // 添加为模态窗口
    FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow(), false);

    return Window;
}

TSharedRef<SWindow> FX_MaterialFunctionUI::CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected)
{
    return SX_MaterialNodePicker::CreateNodePickerWindow(OnNodeSelected);
}

TSharedRef<SWindow> FX_MaterialFunctionUI::ShowNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected)
{
    TSharedRef<SWindow> Window = CreateNodePickerWindow(OnNodeSelected);
    FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow(), false);
    return Window;
}

TArray<TSharedPtr<FName>> FX_MaterialFunctionUI::GetCommonNodeNames()
{
    TArray<TSharedPtr<FName>> NodeNames;
    
    // 添加常用节点名称
    NodeNames.Add(MakeShareable(new FName(TEXT("BaseColor"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Metallic"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Specular"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Roughness"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Emissive"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Opacity"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Normal"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("AmbientOcclusion"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Refraction"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Subsurface"))));
    NodeNames.Add(MakeShareable(new FName(TEXT("Custom"))));
    
    return NodeNames;
}

#undef LOCTEXT_NAMESPACE
