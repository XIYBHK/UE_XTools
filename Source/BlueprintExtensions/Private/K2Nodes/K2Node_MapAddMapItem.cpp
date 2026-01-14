#include "K2Nodes/K2Node_MapAddMapItem.h"

// 编辑器功能
#include "EdGraphSchema_K2.h"

// 蓝图系统
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "SPinTypeSelector.h"

// 编译器-FKCHandler相关
#include "BPTerminal.h"
#include "EdGraphUtilities.h"
#include "KismetCompilerMisc.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiledFunctionContext.h"
#include "BlueprintCompiledStatement.h"

// 功能库
#include "Libraries/MapExtensionsLibrary.h"

// 辅助类
#include "K2NodePinTypeHelpers.h"

#define LOCTEXT_NAMESPACE "XTools_K2Node_MapAddMapItem"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_MapAddMapItem::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "Map添加Map元素");
}

FText UK2Node_MapAddMapItem::GetCompactNodeTitle() const
{
    return LOCTEXT("CompactNodeTitle", "添加元素");
}

FText UK2Node_MapAddMapItem::GetTooltipText() const
{
    return LOCTEXT("TooltipText", "向Map中结构体值的Map字段添加键值对");
}

FText UK2Node_MapAddMapItem::GetMenuCategory() const
{
    return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Map");;
}

FSlateIcon UK2Node_MapAddMapItem::GetIconAndTint(FLinearColor& OutColor) const
{
    static FSlateIcon Icon("EditorStyle", "GraphEditor.MakeMap_16x");
    return Icon;
}

TSharedPtr<SWidget> UK2Node_MapAddMapItem::CreateNodeImage() const
{
    return SPinTypeSelector::ConstructPinTypeImage(GetInputMapPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

class FKCHandler_MapAddMapItem : public FNodeHandlingFunctor
{

public:

	FKCHandler_MapAddMapItem(FKismetCompilerContext& InCompilerContext) 
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

    virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
	    // 先调用父类注册基础网络
	    FNodeHandlingFunctor::RegisterNets(Context, Node);
	    
	    UK2Node_MapAddMapItem* MapNode = CastChecked<UK2Node_MapAddMapItem>(Node);

	    // 处理Map引脚
	    UEdGraphPin* MapPin = MapNode->GetInputMapPin();
	    UEdGraphPin* MapPinNet = FEdGraphUtilities::GetNetFromPin(MapPin);

	    // 处理 Key 引脚
	    UEdGraphPin* KeyPin = MapNode->GetInputKeyPin();
	    UEdGraphPin* KeyPinNet = FEdGraphUtilities::GetNetFromPin(KeyPin);
	    ValidateAndRegisterNetIfLiteral(Context, KeyPin);

	    // 处理 SubKey 引脚
	    UEdGraphPin* SubKeyPin = MapNode->GetInputSubKeyPin();
	    UEdGraphPin* SubKeyPinNet = FEdGraphUtilities::GetNetFromPin(SubKeyPin);
	    ValidateAndRegisterNetIfLiteral(Context, SubKeyPin);
	    
	    // 处理 Item 引脚
	    UEdGraphPin* ItemPin = MapNode->GetInputItemPin();
	    UEdGraphPin* ItemPinNet = FEdGraphUtilities::GetNetFromPin(ItemPin);
	    ValidateAndRegisterNetIfLiteral(Context, ItemPin);
	}

    virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
    {
        // 验证执行引脚
        UEdGraphPin* ThenPin = Context.FindRequiredPinByName(Node, UEdGraphSchema_K2::PN_Then, EGPD_Output);

        // 缓存节点引用
        UK2Node_MapAddMapItem* MapNode = CastChecked<UK2Node_MapAddMapItem>(Node);

        // 获取所需的Pin
        UEdGraphPin* MapPin = MapNode->GetInputMapPin();
	    UEdGraphPin* MapPinNet = FEdGraphUtilities::GetNetFromPin(MapPin);
	    FBPTerminal** MapTerm = Context.NetMap.Find(MapPinNet);

	    UEdGraphPin* KeyPin = MapNode->GetInputKeyPin();
	    UEdGraphPin* KeyPinNet = FEdGraphUtilities::GetNetFromPin(KeyPin);
	    FBPTerminal* KeyTerm = Context.NetMap.FindRef(KeyPinNet);

	    UEdGraphPin* SubKeyPin = MapNode->GetInputSubKeyPin();
	    UEdGraphPin* SubKeyPinNet = FEdGraphUtilities::GetNetFromPin(SubKeyPin);
	    FBPTerminal* SubKeyTerm = Context.NetMap.FindRef(SubKeyPinNet);

	    UEdGraphPin* ItemPin = MapNode->GetInputItemPin();
	    UEdGraphPin* ItemPinNet = FEdGraphUtilities::GetNetFromPin(ItemPin);
	    FBPTerminal* ItemTerm = Context.NetMap.FindRef(ItemPinNet);

	    // 在执行 MapAdd 前进行安全检查
	    if (!*MapTerm || !KeyTerm || !SubKeyTerm || !ItemTerm)
	    {
	        // 【修复】使用 Warning 避免触发 EdGraphNode.h:563 断言崩溃
	        Context.MessageLog.Warning(*NSLOCTEXT("K2Node", "Error_InvalidTerminals", "引脚寄了").ToString(), Node);
	        return;
	    }
	    
	    // MapAdd
	    FBlueprintCompiledStatement& MapAddStmt = Context.AppendStatementForNode(Node);
	    MapAddStmt.Type = KCST_CallFunction;
	    MapAddStmt.FunctionToCall = FindUField<UFunction>(UMapExtensionsLibrary::StaticClass(), TEXT("Map_AddMapItem"));
	    MapAddStmt.RHS.Add(*MapTerm);
	    MapAddStmt.RHS.Add(KeyTerm);
	    MapAddStmt.RHS.Add(SubKeyTerm);
	    MapAddStmt.RHS.Add(ItemTerm);
	    
	    // 执行完后跳转到Then引脚
	    FBlueprintCompiledStatement& Continue = Context.AppendStatementForNode(Node);
	    Continue.Type = KCST_UnconditionalGoto;
	    Context.GotoFixupRequestMap.Add(&Continue, ThenPin);
    }
};

class FNodeHandlingFunctor* UK2Node_MapAddMapItem::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_MapAddMapItem(CompilerContext);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_MapAddMapItem::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    UClass* ActionKey = GetClass();
    if (ActionRegistrar.IsOpenForRegistration(ActionKey))
    {
        UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
        check(NodeSpawner != nullptr);

        ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
    }
}

void UK2Node_MapAddMapItem::PostReconstructNode()
{
    Super::PostReconstructNode();
    PropagatePinType();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_MapAddMapItem::AllocateDefaultPins()
{
    // 创建执行引脚
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

    UEdGraphNode::FCreatePinParams PinParams;
    UEdGraphPin* MapPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, InputMapPinName, PinParams);
    MapPin->PinType.ContainerType = EPinContainerType::Map;
    
    // 在UE5.4中，使用这种方式设置Map的键值类型
    FEdGraphTerminalType WildcardType;
    WildcardType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
    MapPin->PinType.PinValueType = WildcardType;
    MapPin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;

    // 创建 Key 输入引脚
    UEdGraphPin* KeyPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, InputKeyPinName);
    KeyPin->PinType.ContainerType = EPinContainerType::None;

    // 创建 SubKey 输入引脚
    UEdGraphPin* SubKeyPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, InputSubKeyPinName);
    SubKeyPin->PinType.ContainerType = EPinContainerType::None;
	
    // 创建 Item 输入引脚
    UEdGraphPin* ItemPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, InputItemPinName);
    ItemPin->PinType.ContainerType = EPinContainerType::None;

    // 设置引脚默认值
    MapPin->PinFriendlyName = LOCTEXT("MapPin", "Target");
    KeyPin->PinFriendlyName = LOCTEXT("KeyPin", "Key");
    SubKeyPin->PinFriendlyName = LOCTEXT("SubKeyPin", "SubKey"); 
    ItemPin->PinFriendlyName = LOCTEXT("ItemPin", "Item");

    Super::AllocateDefaultPins();
}

void UK2Node_MapAddMapItem::PinDefaultValueChanged(UEdGraphPin* Pin)
{
    Super::PinDefaultValueChanged(Pin);
    PropagatePinType();
}

void UK2Node_MapAddMapItem::ReconstructNode()
{
    Super::ReconstructNode();
    PropagatePinType();
}

void UK2Node_MapAddMapItem::PinConnectionListChanged(UEdGraphPin* Pin)
{
    Super::PinConnectionListChanged(Pin);

    // 如果是 Map 引脚被改变
    if (Pin && Pin->PinName == InputMapPinName)
    {
        // 如果 Map 引脚没有连接
        if (Pin->LinkedTo.Num() == 0)
        {
            // 断开 Key 和 Value 引脚的连接
            if (UEdGraphPin* KeyPin = GetInputKeyPin())
            {
                KeyPin->BreakAllPinLinks();
            }

            if (UEdGraphPin* SubKeyPin = GetInputSubKeyPin())
            {
                SubKeyPin->BreakAllPinLinks();
            }
            
            if (UEdGraphPin* ItemPin = GetInputItemPin())
            {
                ItemPin->BreakAllPinLinks();
            }
        }
    }

    // 更新引脚类型
    PropagatePinType();
}

void UK2Node_MapAddMapItem::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
    Super::NotifyPinConnectionListChanged(Pin);
    PropagatePinType();
}

bool UK2Node_MapAddMapItem::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
    // 先检查参数有效性
    if (!MyPin || !OtherPin)
    {
        return false;
    }

    // 如果是 Map 引脚，执行 Map 相关的检查
    if (MyPin->PinName == InputMapPinName)
    {
        // Map 类型检查
        if (OtherPin->PinType.ContainerType != EPinContainerType::Map)
        {
            OutReason = TEXT("目标引脚必须是Map类型");
            return true;
        }

        // Value 类型检查
        if (!FK2NodePinTypeHelpers::ValidateMapValueIsStruct(OtherPin, &OutReason))
        {
            return true;
        }

        // 结构体成员检查
        if (const UScriptStruct* StructType = Cast<UScriptStruct>(OtherPin->PinType.PinValueType.TerminalSubCategoryObject.Get()))
        {
            if (!FK2NodePinTypeHelpers::ValidateStructHasSinglePropertyOfType(StructType, FMapProperty::StaticClass(), &OutReason))
            {
                return true;
            }
        }
        return false;
    }

    // 如果是 Key、SubKey 或 Item 引脚，检查 Map 是否已连接
    if (MyPin->PinName == InputKeyPinName || MyPin->PinName == InputSubKeyPinName || MyPin->PinName == InputItemPinName)
    {
        UEdGraphPin* MapPin = GetInputMapPin();
        // 确保 MapPin 存在
        if (!MapPin)
        {
            return true; // 如果 MapPin 不存在，禁止连接
        }

        // 检查 Map 是否已连接
        bool bMapConnected = MapPin->LinkedTo.Num() > 0;

        // 如果 Map 未连接，禁止连接 Key、SubKey 和 Item
        if (!bMapConnected)
        {
            OutReason = TEXT("必须先连接Map引脚");
            return true;
        }
    
        // 如果是 Key 引脚
        if (MyPin->PinName == InputKeyPinName)
        {
            // Map 已连接，检查类型匹配
            FEdGraphPinType KeyType = GetKeyPinType();
            if (KeyType.PinCategory != UEdGraphSchema_K2::PC_Wildcard &&
                KeyType.PinCategory != OtherPin->PinType.PinCategory)
            {
                OutReason = TEXT("Key类型不匹配");
                return true;
            }
        }

        // 如果是 SubKey 引脚
        if (MyPin->PinName == InputSubKeyPinName)
        {
            // Map 已连接，检查类型匹配
            FEdGraphPinType SubKeyType = GetSubKeyPinType();
            if (SubKeyType.PinCategory != UEdGraphSchema_K2::PC_Wildcard &&
                SubKeyType.PinCategory != OtherPin->PinType.PinCategory)
            {
                OutReason = TEXT("SubKey类型不匹配");
                return true;
            }
        }
    
        // 如果是 Item 引脚
        if (MyPin->PinName == InputItemPinName)
        {
            FEdGraphPinType ItemType = GetItemPinType();
            if (ItemType.PinCategory != UEdGraphSchema_K2::PC_Wildcard &&
                ItemType.PinCategory != OtherPin->PinType.PinCategory)
            {
                OutReason = TEXT("Item类型不匹配");
                return true;
            }
        }
    }


    
    return false;
}



const FName UK2Node_MapAddMapItem::InputMapPinName(TEXT("MapPin"));
UEdGraphPin* UK2Node_MapAddMapItem::GetInputMapPin() const
{
	return FindPin(InputMapPinName);
}

const FName UK2Node_MapAddMapItem::InputKeyPinName(TEXT("KeyPin"));
UEdGraphPin* UK2Node_MapAddMapItem::GetInputKeyPin() const
{
	return FindPin(InputKeyPinName);
}

const FName UK2Node_MapAddMapItem::InputSubKeyPinName(TEXT("SubKeyPin"));
UEdGraphPin* UK2Node_MapAddMapItem::GetInputSubKeyPin() const
{
    return FindPin(InputSubKeyPinName);
}

const FName UK2Node_MapAddMapItem::InputItemPinName(TEXT("ItemPin"));
UEdGraphPin* UK2Node_MapAddMapItem::GetInputItemPin() const
{
	return FindPin(InputItemPinName);
}



FEdGraphPinType UK2Node_MapAddMapItem::GetKeyPinType() const
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;

    UEdGraphPin* MapPin = GetInputMapPin();
    FK2NodePinTypeHelpers::GetMapKeyType(MapPin, PinType);

    return PinType;
}

FEdGraphPinType UK2Node_MapAddMapItem::GetSubKeyPinType() const
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;

    UEdGraphPin* MapPin = GetInputMapPin();
    if (MapPin && MapPin->LinkedTo.Num() > 0)
    {
        const FEdGraphPinType& MapPinType = MapPin->LinkedTo[0]->PinType;
        if (MapPinType.ContainerType == EPinContainerType::Map &&
            MapPinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Struct)
        {
            if (const UScriptStruct* StructType = Cast<UScriptStruct>(MapPinType.PinValueType.TerminalSubCategoryObject.Get()))
            {
                const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
                FK2NodePinTypeHelpers::GetMapKeyTypeFromStructProperty(StructType, PinType, Schema);
            }
        }
    }

    return PinType;
}

FEdGraphPinType UK2Node_MapAddMapItem::GetItemPinType() const
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;

    UEdGraphPin* MapPin = GetInputMapPin();
    if (MapPin && MapPin->LinkedTo.Num() > 0)
    {
        const FEdGraphPinType& MapPinType = MapPin->LinkedTo[0]->PinType;
        if (MapPinType.ContainerType == EPinContainerType::Map &&
            MapPinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Struct)
        {
            if (const UScriptStruct* StructType = Cast<UScriptStruct>(MapPinType.PinValueType.TerminalSubCategoryObject.Get()))
            {
                const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
                FK2NodePinTypeHelpers::GetMapValueTypeFromStructProperty(StructType, PinType, Schema);
            }
        }
    }

    return PinType;
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ReferenceHandling 

void UK2Node_MapAddMapItem::PropagatePinType()
{
    UEdGraphPin* MapPin = GetInputMapPin();
    UEdGraphPin* KeyPin = GetInputKeyPin();
    UEdGraphPin* SubKeyPin = GetInputSubKeyPin();
    UEdGraphPin* ItemPin = GetInputItemPin();

    // 如果 Map 引脚有连接
    if (MapPin && MapPin->LinkedTo.Num() > 0)
    {
        const FEdGraphPinType& MapPinType = MapPin->LinkedTo[0]->PinType;
        if (MapPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            MapPin->PinType = MapPinType;
        }

        if (KeyPin && KeyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            KeyPin->PinType = GetKeyPinType();
        }

        if (SubKeyPin && SubKeyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            SubKeyPin->PinType = GetSubKeyPinType();
        }
        
        if (ItemPin && ItemPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            ItemPin->PinType = GetItemPinType();
        }
    }
    else
    {
        // 重置为Wildcard类型
        FK2NodePinTypeHelpers::ResetMapPinToWildcard(MapPin);
        FK2NodePinTypeHelpers::ResetPinToWildcard(KeyPin);
        FK2NodePinTypeHelpers::ResetPinToWildcard(SubKeyPin);
        FK2NodePinTypeHelpers::ResetPinToWildcard(ItemPin);
    }

    GetGraph()->NotifyGraphChanged();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE