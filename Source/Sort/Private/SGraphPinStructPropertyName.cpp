#include "SGraphPinStructPropertyName.h"
#include "K2Node_SmartSort.h"
#include "EdGraphSchema_K2.h"
#include "Engine/UserDefinedStruct.h"

void SGraphPinStructPropertyName::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
    RefreshNameList();
    SGraphPinNameList::Construct(SGraphPinNameList::FArguments(), InGraphPinObj, NameList);
}

void SGraphPinStructPropertyName::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const TArray<FString>& InPropertyNames)
{
    // 直接使用传入的属性列表，不需要动态获取
    RefreshNameListFromArray(InPropertyNames);
    SGraphPinNameList::Construct(SGraphPinNameList::FArguments(), InGraphPinObj, NameList);
}

void SGraphPinStructPropertyName::RefreshNameList()
{
    NameList.Empty();
    
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
                TSharedPtr<FName> NamePropertyItem = MakeShareable(new FName(*PropName));
                NameList.Add(NamePropertyItem);
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
    NameList.Empty();

    for (const FString& PropName : InPropertyNames)
    {
        TSharedPtr<FName> NamePropertyItem = MakeShareable(new FName(*PropName));
        NameList.Add(NamePropertyItem);
    }
}
