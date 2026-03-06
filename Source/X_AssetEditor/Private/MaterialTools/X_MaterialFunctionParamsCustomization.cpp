#include "MaterialTools/X_MaterialFunctionParamsCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "MaterialTools/X_MaterialFunctionParams.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "X_MaterialFunctionParamsCustomization"

TSharedRef<IPropertyTypeCustomization> FX_MaterialFunctionParamsCustomization::MakeInstance()
{
    return MakeShared<FX_MaterialFunctionParamsCustomization>();
}

void FX_MaterialFunctionParamsCustomization::CustomizeHeader(
    TSharedRef<IPropertyHandle> StructPropertyHandle,
    FDetailWidgetRow& HeaderRow,
    IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
    HeaderRow
        .NameContent()
        [
            StructPropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        .MinDesiredWidth(250.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("MaterialFunctionParamsHeader", "配置材质函数节点布局与连接行为"))
            .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
        ];
}

void FX_MaterialFunctionParamsCustomization::CustomizeChildren(
    TSharedRef<IPropertyHandle> StructPropertyHandle,
    IDetailChildrenBuilder& StructBuilder,
    IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
    const TSharedPtr<IPropertyHandle> NodeNameHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, NodeName));
    const TSharedPtr<IPropertyHandle> PosXHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, PosX));
    const TSharedPtr<IPropertyHandle> PosYHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, PosY));
    const TSharedPtr<IPropertyHandle> SetupConnectionsHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bSetupConnections));
    const TSharedPtr<IPropertyHandle> SmartConnectHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bEnableSmartConnect));
    const TSharedPtr<IPropertyHandle> UseAttributesHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bUseMaterialAttributes));
    const TSharedPtr<IPropertyHandle> ConnectionModeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, ConnectionMode));
    const TSharedPtr<IPropertyHandle> BaseColorHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bConnectToBaseColor));
    const TSharedPtr<IPropertyHandle> MetallicHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bConnectToMetallic));
    const TSharedPtr<IPropertyHandle> RoughnessHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bConnectToRoughness));
    const TSharedPtr<IPropertyHandle> NormalHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bConnectToNormal));
    const TSharedPtr<IPropertyHandle> EmissiveHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bConnectToEmissive));
    const TSharedPtr<IPropertyHandle> AOHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FX_MaterialFunctionParams, bConnectToAO));

    StructBuilder.AddCustomRow(LOCTEXT("OverviewFilter", "概览"))
        .WholeRowContent()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("OverviewText", "推荐优先使用“智能连接”。只有需要明确指定 Base Color、Roughness 等最终属性时，再关闭智能连接并改用手动目标。"))
                .AutoWrapText(true)
            ]
        ];

    IDetailGroup& BasicGroup = StructBuilder.AddGroup("MaterialFunctionParamsBasic", LOCTEXT("BasicGroup", "基本"));
    if (NodeNameHandle.IsValid())
    {
        BasicGroup.AddPropertyRow(NodeNameHandle.ToSharedRef());
    }

    IDetailGroup& LayoutGroup = StructBuilder.AddGroup("MaterialFunctionParamsLayout", LOCTEXT("LayoutGroup", "布局"));
    if (PosXHandle.IsValid())
    {
        LayoutGroup.AddPropertyRow(PosXHandle.ToSharedRef());
    }
    if (PosYHandle.IsValid())
    {
        LayoutGroup.AddPropertyRow(PosYHandle.ToSharedRef());
    }

    IDetailGroup& ConnectionGroup = StructBuilder.AddGroup("MaterialFunctionParamsConnection", LOCTEXT("ConnectionGroup", "连接"));
    if (SetupConnectionsHandle.IsValid())
    {
        ConnectionGroup.AddPropertyRow(SetupConnectionsHandle.ToSharedRef());
    }
    if (SmartConnectHandle.IsValid())
    {
        ConnectionGroup.AddPropertyRow(SmartConnectHandle.ToSharedRef());
    }
    if (UseAttributesHandle.IsValid())
    {
        ConnectionGroup.AddPropertyRow(UseAttributesHandle.ToSharedRef());
    }
    if (ConnectionModeHandle.IsValid())
    {
        ConnectionGroup.AddPropertyRow(ConnectionModeHandle.ToSharedRef());
    }

    ConnectionGroup.AddWidgetRow()
        .WholeRowContent()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(TAttribute<FText>::CreateLambda([
                    SetupConnectionsHandle,
                    SmartConnectHandle,
                    UseAttributesHandle,
                    ConnectionModeHandle]()
                {
                    bool bSetupConnections = true;
                    bool bSmartConnect = true;
                    bool bUseAttributes = false;
                    uint8 ConnectionModeValue = static_cast<uint8>(EConnectionMode::Add);

                    if (SetupConnectionsHandle.IsValid())
                    {
                        SetupConnectionsHandle->GetValue(bSetupConnections);
                    }
                    if (SmartConnectHandle.IsValid())
                    {
                        SmartConnectHandle->GetValue(bSmartConnect);
                    }
                    if (UseAttributesHandle.IsValid())
                    {
                        UseAttributesHandle->GetValue(bUseAttributes);
                    }
                    if (ConnectionModeHandle.IsValid())
                    {
                        ConnectionModeHandle->GetValue(ConnectionModeValue);
                    }

                    if (!bSetupConnections)
                    {
                        return LOCTEXT("ConnectionSummaryDisabled", "当前不会自动连接材质属性，只会把材质函数节点添加到图中。");
                    }

                    if (bUseAttributes)
                    {
                        return LOCTEXT("ConnectionSummaryAttributes", "当前策略：优先接入 Material Attributes 主链路，并尽量保留现有链路结构。");
                    }

                    if (bSmartConnect)
                    {
                        return LOCTEXT("ConnectionSummarySmart", "当前策略：根据函数输入输出语义自动推断连接位置，并尝试插入到最合适的材质链路。");
                    }

                    const EConnectionMode ConnectionMode = static_cast<EConnectionMode>(ConnectionModeValue);
                    switch (ConnectionMode)
                    {
                    case EConnectionMode::None:
                        return LOCTEXT("ConnectionSummaryDirect", "当前策略：手动模式，直接把函数输出连接到选中的最终材质属性。");
                    case EConnectionMode::Add:
                        return LOCTEXT("ConnectionSummaryAdd", "当前策略：手动模式，通过 Add 节点把函数输出叠加到选中的最终材质属性。");
                    case EConnectionMode::Multiply:
                        return LOCTEXT("ConnectionSummaryMultiply", "当前策略：手动模式，通过 Multiply 节点把函数输出乘到选中的最终材质属性。");
                    case EConnectionMode::Auto:
                    default:
                        return LOCTEXT("ConnectionSummaryAuto", "当前策略：自动选择连接方式。");
                    }
                }))
                .AutoWrapText(true)
            ]
        ];

    IDetailGroup& TargetsGroup = StructBuilder.AddGroup("MaterialFunctionParamsTargets", LOCTEXT("TargetsGroup", "连接目标"));
    TargetsGroup.HeaderRow()
    [
        SNew(STextBlock)
        .Text(LOCTEXT("TargetsHeader", "连接目标"))
        .Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
    ];

    TargetsGroup.AddWidgetRow()
        .WholeRowContent()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("TargetsHint", "手动模式下可选择一个或多个最终材质属性。若函数只有单输出，建议只勾选一个目标。"))
            .AutoWrapText(true)
            .Visibility(TAttribute<EVisibility>::CreateLambda([SmartConnectHandle, UseAttributesHandle]()
            {
                bool bSmartConnect = true;
                bool bUseAttributes = false;
                if (SmartConnectHandle.IsValid())
                {
                    SmartConnectHandle->GetValue(bSmartConnect);
                }
                if (UseAttributesHandle.IsValid())
                {
                    UseAttributesHandle->GetValue(bUseAttributes);
                }
                return (!bSmartConnect && !bUseAttributes) ? EVisibility::Visible : EVisibility::Collapsed;
            }))
        ];

    TargetsGroup.AddWidgetRow()
        .WholeRowContent()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            .Visibility(TAttribute<EVisibility>::CreateLambda([
                SetupConnectionsHandle,
                SmartConnectHandle,
                UseAttributesHandle,
                BaseColorHandle,
                MetallicHandle,
                RoughnessHandle,
                NormalHandle,
                EmissiveHandle,
                AOHandle]()
            {
                bool bSetupConnections = true;
                bool bSmartConnect = true;
                bool bUseAttributes = false;
                if (SetupConnectionsHandle.IsValid())
                {
                    SetupConnectionsHandle->GetValue(bSetupConnections);
                }
                if (SmartConnectHandle.IsValid())
                {
                    SmartConnectHandle->GetValue(bSmartConnect);
                }
                if (UseAttributesHandle.IsValid())
                {
                    UseAttributesHandle->GetValue(bUseAttributes);
                }

                if (!bSetupConnections || bSmartConnect || bUseAttributes)
                {
                    return EVisibility::Collapsed;
                }

                auto IsChecked = [](const TSharedPtr<IPropertyHandle>& Handle)
                {
                    bool bValue = false;
                    return Handle.IsValid() && Handle->GetValue(bValue) == FPropertyAccess::Success && bValue;
                };

                const bool bHasAnyTarget = IsChecked(BaseColorHandle)
                    || IsChecked(MetallicHandle)
                    || IsChecked(RoughnessHandle)
                    || IsChecked(NormalHandle)
                    || IsChecked(EmissiveHandle)
                    || IsChecked(AOHandle);

                return bHasAnyTarget ? EVisibility::Collapsed : EVisibility::Visible;
            }))
            [
                SNew(STextBlock)
                .Text(LOCTEXT("TargetsWarning", "当前处于手动连接模式，但还没有选择任何最终材质属性。继续执行时只会添加节点，不会建立输出连接。"))
                .ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.4f, 1.0f))
                .AutoWrapText(true)
            ]
        ];

    TargetsGroup.AddWidgetRow()
        .WholeRowContent()
        [
            SNew(STextBlock)
            .Text(TAttribute<FText>::CreateLambda([
                SetupConnectionsHandle,
                SmartConnectHandle,
                UseAttributesHandle,
                BaseColorHandle,
                MetallicHandle,
                RoughnessHandle,
                NormalHandle,
                EmissiveHandle,
                AOHandle]()
            {
                bool bSetupConnections = true;
                bool bSmartConnect = true;
                bool bUseAttributes = false;
                if (SetupConnectionsHandle.IsValid())
                {
                    SetupConnectionsHandle->GetValue(bSetupConnections);
                }
                if (SmartConnectHandle.IsValid())
                {
                    SmartConnectHandle->GetValue(bSmartConnect);
                }
                if (UseAttributesHandle.IsValid())
                {
                    UseAttributesHandle->GetValue(bUseAttributes);
                }

                if (!bSetupConnections || bSmartConnect || bUseAttributes)
                {
                    return FText::GetEmpty();
                }

                TArray<FText> Targets;
                auto AppendTarget = [&Targets](const TSharedPtr<IPropertyHandle>& Handle, const TCHAR* Label)
                {
                    bool bValue = false;
                    if (Handle.IsValid() && Handle->GetValue(bValue) == FPropertyAccess::Success && bValue)
                    {
                        Targets.Add(FText::FromString(Label));
                    }
                };

                AppendTarget(BaseColorHandle, TEXT("Base Color"));
                AppendTarget(MetallicHandle, TEXT("Metallic"));
                AppendTarget(RoughnessHandle, TEXT("Roughness"));
                AppendTarget(NormalHandle, TEXT("Normal"));
                AppendTarget(EmissiveHandle, TEXT("Emissive Color"));
                AppendTarget(AOHandle, TEXT("Ambient Occlusion"));

                if (Targets.Num() == 0)
                {
                    return FText::GetEmpty();
                }

                return FText::Format(
                    LOCTEXT("TargetsSummary", "当前手动目标：{0}"),
                    FText::Join(FText::FromString(TEXT("、")), Targets));
            }))
            .AutoWrapText(true)
            .Visibility(TAttribute<EVisibility>::CreateLambda([SetupConnectionsHandle, SmartConnectHandle, UseAttributesHandle]()
            {
                bool bSetupConnections = true;
                bool bSmartConnect = true;
                bool bUseAttributes = false;
                if (SetupConnectionsHandle.IsValid())
                {
                    SetupConnectionsHandle->GetValue(bSetupConnections);
                }
                if (SmartConnectHandle.IsValid())
                {
                    SmartConnectHandle->GetValue(bSmartConnect);
                }
                if (UseAttributesHandle.IsValid())
                {
                    UseAttributesHandle->GetValue(bUseAttributes);
                }
                return (bSetupConnections && !bSmartConnect && !bUseAttributes) ? EVisibility::Visible : EVisibility::Collapsed;
            }))
        ];

    auto AddTargetProperty = [&TargetsGroup](const TSharedPtr<IPropertyHandle>& PropertyHandle)
    {
        if (PropertyHandle.IsValid())
        {
            TargetsGroup.AddPropertyRow(PropertyHandle.ToSharedRef());
        }
    };

    AddTargetProperty(BaseColorHandle);
    AddTargetProperty(MetallicHandle);
    AddTargetProperty(RoughnessHandle);
    AddTargetProperty(NormalHandle);
    AddTargetProperty(EmissiveHandle);
    AddTargetProperty(AOHandle);
}

#undef LOCTEXT_NAMESPACE
