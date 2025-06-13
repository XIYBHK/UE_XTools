#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "SGraphPinNameList.h"
#include "K2Node_SmartSort.h"

/**
 * 结构体属性名称选择引脚
 * 提供下拉菜单来选择结构体的可排序属性
 */
class SORT_API SGraphPinStructPropertyName : public SGraphPinNameList
{
public:
    SLATE_BEGIN_ARGS(SGraphPinStructPropertyName) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const TArray<FString>& InPropertyNames);

protected:
    // 刷新属性名称列表
    void RefreshNameList();

    // 从数组刷新属性名称列表
    void RefreshNameListFromArray(const TArray<FString>& InPropertyNames);

    // 获取关联的智能排序节点
    UK2Node_SmartSort* GetSmartSortNode() const;

private:
    // 属性名称列表
    TArray<TSharedPtr<FName>> NameList;
};
