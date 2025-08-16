#pragma once

#include "CoreMinimal.h"
#include "K2Node_SpawnActorFromClass.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_SpawnActorFromPool.generated.h"

UCLASS()
class OBJECTPOOLEDITOR_API UK2Node_SpawnActorFromPool : public UK2Node_SpawnActorFromClass
{
    GENERATED_BODY()
public:
    UK2Node_SpawnActorFromPool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    
public:
    // 菜单注册/外观：注册到右键菜单并设置外观
    virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual FText GetMenuCategory() const override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
    virtual FText GetKeywords() const override;

    // 重写引脚分配以确保返回值类型正确
    virtual void AllocateDefaultPins() override;
    
    // 处理引脚连接和默认值变化以更新返回值类型
    virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
    virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
    virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
    
    // 仅重写 Expand，复用父类的引脚分配与动态重建逻辑
    virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, class UEdGraph* SourceGraph) override;

private:
    // 更新返回值引脚类型的辅助方法
    void UpdateReturnValueType();
    
    /** Cached node title */
    mutable FNodeTextCache CachedNodeTitle;
};