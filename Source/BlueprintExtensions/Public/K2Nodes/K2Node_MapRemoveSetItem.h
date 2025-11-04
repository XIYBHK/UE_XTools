#pragma once

#include "BlueprintActionFilter.h"
#include "BlueprintNodeSignature.h"
#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_MapRemoveSetItem.generated.h"


class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class FString;
class SWidget;
class UEdGraph;
class UEdGraphPin;
class UObject;
struct FEdGraphPinType;
struct FLinearColor;

UCLASS(MinimalAPI, Category = "XTools|K2Node", meta=(Keywords = "XTools, Map"))
class UK2Node_MapRemoveSetItem : public UK2Node
{
    GENERATED_BODY()

public:

//节点基本属性
#pragma region NodeProperties
    
    virtual bool IsNodePure() const override { return false; }
    virtual bool ShouldDrawCompact() const { return true; }
    virtual bool IncludeParentNodeContextMenu() const override { return true; }
    virtual int32 GetNodeRefreshPriority() const override { return EBaseNodeRefreshPriority::Low_UsesDependentWildcard; }
    
#pragma endregion
    
//节点外观
#pragma region NodeAppearance
    
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetCompactNodeTitle() const override;
    virtual FText GetTooltipText() const override;
    virtual FText GetMenuCategory() const override;
    virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
    virtual TSharedPtr<SWidget> CreateNodeImage() const override;
    
#pragma endregion
    
//蓝图编译
#pragma region BlueprintCompile
    
    virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;

#pragma endregion

//蓝图系统集成
#pragma region BlueprintSystem
    
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
    virtual void ReconstructNode() override;
    virtual void PostReconstructNode() override;
    virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
    
#pragma endregion

//引脚管理
#pragma region PinManagement
    
    virtual void AllocateDefaultPins() override;
    virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
    virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;

    UEdGraphPin* GetInputMapPin() const;
    UEdGraphPin* GetInputKeyPin() const;
    UEdGraphPin* GetInputItemPin() const;
    
private:
    void PropagatePinType();
    FEdGraphPinType GetKeyPinType() const;
    FEdGraphPinType GetItemPinType() const;

    static const FName InputMapPinName;
    static const FName InputKeyPinName;
    static const FName InputItemPinName;

    
#pragma endregion


};