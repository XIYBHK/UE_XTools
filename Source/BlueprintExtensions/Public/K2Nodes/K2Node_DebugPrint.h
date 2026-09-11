/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_DebugPrint.generated.h"

/** 多值调试打印；运行时只依赖 BlueprintExtensionsRuntime。 */
UCLASS(meta=(Keywords="DebugPrint Print String 调试 打印 多值"))
class BLUEPRINTEXTENSIONS_API UK2Node_DebugPrint : public UK2Node, public IK2Node_AddPinInterface
{
    GENERATED_BODY()
public:
    UK2Node_DebugPrint();
    virtual void AllocateDefaultPins() override;
    virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
    virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
    virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual FText GetMenuCategory() const override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FLinearColor GetNodeTitleColor() const override;
    virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
    virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
    virtual void AddInputPin() override;
    virtual void RemoveInputPin(UEdGraphPin* Pin) override;
    virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;

private:
    // 稳定名称保证删除中间引脚不会把后续连接错配到其他值。
    UPROPERTY()
    TArray<FName> ValuePinNames;
    UPROPERTY()
    int32 NextValueId = 0;

    UEdGraphPin* CreateValuePin(FName Name);
    bool IsValuePin(const UEdGraphPin* Pin) const;
    void NotifyChanged();
};
