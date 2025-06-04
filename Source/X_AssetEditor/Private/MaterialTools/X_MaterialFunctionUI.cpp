#include "MaterialTools/X_MaterialFunctionUI.h"

#include "Materials/MaterialFunction.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Logging/LogMacros.h"
#include "MaterialTools/X_MaterialFunctionCore.h"

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
    
    // 获取常用节点名称
    NodeNames = FX_MaterialFunctionUI::GetCommonNodeNames();
    
    ChildSlot
    [
        SNew(SVerticalBox)
        
        // 标题
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(5)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("SelectNodeTitle", "选择目标节点"))
            .Font(FAppStyle::Get().GetFontStyle("HeadingFont"))
        ]
        
        // 节点列表
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(5)
        [
            SAssignNew(NodeListBox, SScrollBox)
        ]
    ];
    
    // 填充节点列表
    if (NodeListBox.IsValid())
    {
        for (TSharedPtr<FName> NodeName : NodeNames)
        {
            NodeListBox->AddSlot()
            [
                GenerateNodeItem(NodeName)
            ];
        }
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

TSharedRef<SWidget> SX_MaterialNodePicker::GenerateNodeItem(TSharedPtr<FName> NodeName)
{
    return SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "NoBorder")
        .OnClicked(this, &SX_MaterialNodePicker::OnNodeSelected, NodeName)
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Center)
        .ContentPadding(FMargin(5, 2))
        [
            SNew(STextBlock)
            .Text(FText::FromName(*NodeName))
            .Font(FAppStyle::Get().GetFontStyle("NormalFont"))
        ];
}

FReply SX_MaterialNodePicker::OnNodeSelected(TSharedPtr<FName> NodeName)
{
    if (OnNodeSelectedDelegate.IsBound())
    {
        OnNodeSelectedDelegate.Execute(*NodeName);
    }
    
    // 关闭窗口
    TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
    if (Window.IsValid())
    {
        Window->RequestDestroyWindow();
    }
    
    return FReply::Handled();
}

// 使用新版的资产选择器实现
TSharedRef<SWindow> FX_MaterialFunctionUI::CreateMaterialFunctionPickerWindow(FOnMaterialFunctionSelected OnFunctionSelected)
{
    // 打印日志，标明使用了哪个方法
    UE_LOG(LogTemp, Warning, TEXT("### 调用了 FX_MaterialFunctionUI::CreateMaterialFunctionPickerWindow - 使用新版选择器"));
    
    // 创建窗口
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("MaterialFunctionPickerTitle", "选择材质函数"))
        .ClientSize(FVector2D(400, 600))
        .SupportsMaximize(false)
        .SupportsMinimize(false);

    // 配置资产选择器
    FAssetPickerConfig AssetPickerConfig;
    AssetPickerConfig.Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
    AssetPickerConfig.Filter.bRecursiveClasses = true;
    AssetPickerConfig.bAllowNullSelection = false;
    AssetPickerConfig.bCanShowFolders = true;
    AssetPickerConfig.bCanShowClasses = true;
    AssetPickerConfig.bShowTypeInColumnView = true;
    AssetPickerConfig.bShowPathInColumnView = true;
    AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

    // 配置选择回调
    AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(
        [OnFunctionSelected, &Window](const FAssetData& AssetData)
        {
            UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(AssetData.GetAsset());
            if (MaterialFunction && OnFunctionSelected.IsBound())
            {
                OnFunctionSelected.Execute(MaterialFunction);
            }
            
            // 关闭窗口
            Window->RequestDestroyWindow();
        });

    // 配置双击回调
    AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateLambda(
        [OnFunctionSelected, &Window](const FAssetData& AssetData)
        {
            UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(AssetData.GetAsset());
            if (MaterialFunction && OnFunctionSelected.IsBound())
            {
                OnFunctionSelected.Execute(MaterialFunction);
            }
            
            // 关闭窗口
            Window->RequestDestroyWindow();
        });

    // 创建资产选择器小部件
    TSharedRef<SWidget> AssetPickerWidget = SNew(SBox)
        .WidthOverride(400.0f)
        .HeightOverride(600.0f)
        [
            FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().CreateAssetPicker(AssetPickerConfig)
        ];

    // 设置窗口内容
    Window->SetContent(AssetPickerWidget);

    // 添加为模态窗口
    FSlateApplication::Get().AddModalWindow(Window, nullptr, false);

    return Window;
}

TSharedRef<SWindow> FX_MaterialFunctionUI::CreateNodePickerWindow(FOnMaterialNodeSelected OnNodeSelected)
{
    return SX_MaterialNodePicker::CreateNodePickerWindow(OnNodeSelected);
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