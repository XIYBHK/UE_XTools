#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "SGraphPin.h"

/**
 * 排序模块的图形引脚工厂
 * 用于创建自定义的引脚UI组件
 */
class SORTEDITOR_API FSortGraphPinFactory : public FGraphPanelPinFactory
{
public:
    virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;
};
