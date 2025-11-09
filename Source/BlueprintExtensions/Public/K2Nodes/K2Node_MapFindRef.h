#pragma once

#include "BlueprintActionFilter.h"
#include "BlueprintNodeSignature.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateIcon.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_MapFindRef.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FString;
class SWidget;
class UEdGraph;
class UEdGraphPin;
class UObject;
struct FEdGraphPinType;
struct FLinearColor;

UCLASS(MinimalAPI, Category = "XTools|K2Node", meta=(Keywords = "XTools, Map"))
class UK2Node_MapFindRef : public UK2Node
{
    GENERATED_BODY()

public:
    UK2Node_MapFindRef(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

//节点基本属性
#pragma region NodeProperties
    
    virtual bool IsNodePure() const override { return true; }
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
    
    // 使用ExpandNode而不是自定义Handler
    virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

#pragma endregion

//蓝图系统集成
#pragma region BlueprintSystem
    
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
    virtual FBlueprintNodeSignature GetSignature() const override;
    virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
    virtual void PostReconstructNode() override;
    
#pragma endregion

//引脚管理
#pragma region PinManagement
    
    virtual void AllocateDefaultPins() override;
    virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
    virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;

    UEdGraphPin* GetMapPin() const { return Pins[0]; }
    UEdGraphPin* GetKeyPin() const { return Pins[1]; }
    UEdGraphPin* GetValuePin() const { return Pins[2]; }
    UEdGraphPin* GetFoundResultPin() const { return Pins[3]; }
    
#pragma endregion
    
//引用处理
#pragma region ReferenceHandling 
    
    void SetDesiredReturnType(bool bAsReference);

private:
    void ToggleReturnPin();
    void PropagatePinType();
    bool IsSetToReturnRef() const;
    
    UPROPERTY()
    bool bReturnByRefDesired;
    
#pragma endregion
    
};
