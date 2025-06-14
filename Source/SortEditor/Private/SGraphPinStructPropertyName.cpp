#include "SGraphPinStructPropertyName.h"
#include "K2Node_SmartSort.h"
#include "EdGraphSchema_K2.h"
#include "Engine/UserDefinedStruct.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"

void SGraphPinStructPropertyName::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
    SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
    RefreshNameList();
}

void SGraphPinStructPropertyName::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const TArray<FString>& InPropertyNames)
{
    SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
    RefreshNameListFromArray(InPropertyNames);
}

TSharedRef<SWidget> SGraphPinStructPropertyName::GetDefaultValueWidget()
{
    return SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&PropertyOptions)
        .OnGenerateWidget(this, &SGraphPinStructPropertyName::OnGenerateWidget)
        .OnSelectionChanged(this, &SGraphPinStructPropertyName::OnSelectionChanged)
        .Content()
        [
            SNew(STextBlock)
            .Text(this, &SGraphPinStructPropertyName::GetSelectedText)
        ];
}

void SGraphPinStructPropertyName::RefreshNameList()
{
    PropertyOptions.Empty();

    UK2Node_SmartSort* SmartSortNode = GetSmartSortNode();
    if (!SmartSortNode)
    {
        return;
    }

    // 获取连接的数组类型
    UEdGraphPin* ArrayPin = SmartSortNode->FindPin(FSmartSort_Helper::PN_TargetArray);
    if (!ArrayPin || ArrayPin->LinkedTo.Num() == 0)
    {
        return;
    }

    // 获取连接的引脚类型
    FEdGraphPinType ConnectedType = ArrayPin->LinkedTo[0]->PinType;

    // 检查是否为结构体数组
    if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
        ConnectedType.PinSubCategoryObject.IsValid())
    {
        UScriptStruct* StructType = Cast<UScriptStruct>(ConnectedType.PinSubCategoryObject.Get());
        if (StructType)
        {
            // 获取可排序的属性列表
            TArray<FString> AvailableProperties = SmartSortNode->GetAvailableProperties(ConnectedType);

            for (const FString& PropName : AvailableProperties)
            {
                PropertyOptions.Add(MakeShareable(new FString(PropName)));
            }
        }
    }
}

UK2Node_SmartSort* SGraphPinStructPropertyName::GetSmartSortNode() const
{
    if (GraphPinObj)
    {
        return Cast<UK2Node_SmartSort>(GraphPinObj->GetOuter());
    }
    return nullptr;
}

void SGraphPinStructPropertyName::RefreshNameListFromArray(const TArray<FString>& InPropertyNames)
{
    PropertyOptions.Empty();

    for (const FString& PropName : InPropertyNames)
    {
        PropertyOptions.Add(MakeShareable(new FString(PropName)));
    }
}

void SGraphPinStructPropertyName::OnSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid() && GraphPinObj)
    {
        GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, *NewSelection);
    }
}

TSharedRef<SWidget> SGraphPinStructPropertyName::OnGenerateWidget(TSharedPtr<FString> InOption)
{
    return SNew(STextBlock)
        .Text(FText::FromString(InOption.IsValid() ? *InOption : TEXT("")));
}

FText SGraphPinStructPropertyName::GetSelectedText() const
{
    if (GraphPinObj)
    {
        return FText::FromString(GraphPinObj->GetDefaultAsString());
    }
    return FText::FromString(TEXT("选择属性"));
}
