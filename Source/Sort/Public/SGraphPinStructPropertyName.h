#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "Widgets/Input/SComboBox.h"
#include "K2Node_SmartSort.h"

/**
 * 结构体属性名称选择引脚
 * 提供下拉菜单来选择结构体的可排序属性
 */
class SORT_API SGraphPinStructPropertyName : public SGraphPin
{
public:
    SLATE_BEGIN_ARGS(SGraphPinStructPropertyName) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const TArray<FString>& InPropertyNames);

protected:
    // 重写SGraphPin的虚函数
    virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

    // 刷新属性名称列表
    void RefreshNameList();

    // 从数组刷新属性名称列表
    void RefreshNameListFromArray(const TArray<FString>& InPropertyNames);

    // 获取关联的智能排序节点
    UK2Node_SmartSort* GetSmartSortNode() const;

    // 下拉菜单回调函数
    void OnSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FString> InOption);
    FText GetSelectedText() const;

private:
    // 属性名称列表
    TArray<TSharedPtr<FString>> PropertyOptions;

    // 下拉菜单组件
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ComboBox;
};
