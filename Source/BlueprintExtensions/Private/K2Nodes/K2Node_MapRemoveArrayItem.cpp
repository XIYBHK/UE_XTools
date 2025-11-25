#include "K2Nodes/K2Node_MapRemoveArrayItem.h"

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

#define LOCTEXT_NAMESPACE "XTools_K2Node_MapRemoveArrayItem"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region NodeAppearance

FText UK2Node_MapRemoveArrayItem::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "Map移除数组元素");
}

FText UK2Node_MapRemoveArrayItem::GetCompactNodeTitle() const
{
    return LOCTEXT("CompactNodeTitle", "移除元素");
}

FText UK2Node_MapRemoveArrayItem::GetTooltipText() const
{
    return LOCTEXT("TooltipText", "从Map中结构体值的数组字段移除元素");
}

FText UK2Node_MapRemoveArrayItem::GetMenuCategory() const
{
    return LOCTEXT("MenuCategory", "XTools|Blueprint Extensions|Map");;
}

FSlateIcon UK2Node_MapRemoveArrayItem::GetIconAndTint(FLinearColor& OutColor) const
{
    static FSlateIcon Icon("EditorStyle", "GraphEditor.MakeMap_16x");
    return Icon;
}

TSharedPtr<SWidget> UK2Node_MapRemoveArrayItem::CreateNodeImage() const
{
    return SPinTypeSelector::ConstructPinTypeImage(GetInputMapPin());
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintCompile

class FKCHandler_MapRemoveArrayItem : public FNodeHandlingFunctor
{

public:

	FKCHandler_MapRemoveArrayItem(FKismetCompilerContext& InCompilerContext) 
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

    virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
	    // 先调用父类注册基础网络
	    FNodeHandlingFunctor::RegisterNets(Context, Node);
	    
	    UK2Node_MapRemoveArrayItem* MapNode = CastChecked<UK2Node_MapRemoveArrayItem>(Node);

	    // 处理Map引脚
	    UEdGraphPin* MapPin = MapNode->GetInputMapPin();
	    UEdGraphPin* MapPinNet = FEdGraphUtilities::GetNetFromPin(MapPin);

	    // 处理 Key 引脚
	    UEdGraphPin* KeyPin = MapNode->GetInputKeyPin();
	    UEdGraphPin* KeyPinNet = FEdGraphUtilities::GetNetFromPin(KeyPin);
	    ValidateAndRegisterNetIfLiteral(Context, KeyPin);
	    
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
        UK2Node_MapRemoveArrayItem* MapNode = CastChecked<UK2Node_MapRemoveArrayItem>(Node);

        // 获取所需的Pin
        UEdGraphPin* MapPin = MapNode->GetInputMapPin();
	    UEdGraphPin* MapPinNet = FEdGraphUtilities::GetNetFromPin(MapPin);
	    FBPTerminal** MapTerm = Context.NetMap.Find(MapPinNet);

	    UEdGraphPin* KeyPin = MapNode->GetInputKeyPin();
	    UEdGraphPin* KeyPinNet = FEdGraphUtilities::GetNetFromPin(KeyPin);
	    FBPTerminal* KeyTerm = Context.NetMap.FindRef(KeyPinNet);

	    UEdGraphPin* ItemPin = MapNode->GetInputItemPin();
	    UEdGraphPin* ItemPinNet = FEdGraphUtilities::GetNetFromPin(ItemPin);
	    FBPTerminal* ItemTerm = Context.NetMap.FindRef(ItemPinNet);

	    // 在执行 MapAdd 前进行安全检查
	    if (!*MapTerm || !KeyTerm || !ItemTerm)
	    {
	        // 【修复】使用 Warning 避免触发 EdGraphNode.h:563 断言崩溃
	        Context.MessageLog.Warning(*NSLOCTEXT("K2Node", "Error_InvalidTerminals", "引脚寄了").ToString(), Node);
	        return;
	    }
	    
	    // MapRemove
	    FBlueprintCompiledStatement& MapRemoveStmt = Context.AppendStatementForNode(Node);
	    MapRemoveStmt.Type = KCST_CallFunction;
	    MapRemoveStmt.FunctionToCall = FindUField<UFunction>(UMapExtensionsLibrary::StaticClass(), TEXT("Map_RemoveArrayItem"));
	    MapRemoveStmt.RHS.Add(*MapTerm);
	    MapRemoveStmt.RHS.Add(KeyTerm);
	    MapRemoveStmt.RHS.Add(ItemTerm);
	    
	    // 执行完后跳转到Then引脚
	    FBlueprintCompiledStatement& Continue = Context.AppendStatementForNode(Node);
	    Continue.Type = KCST_UnconditionalGoto;
	    Context.GotoFixupRequestMap.Add(&Continue, ThenPin);
    }

};

class FNodeHandlingFunctor* UK2Node_MapRemoveArrayItem::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_MapRemoveArrayItem(CompilerContext);
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region BlueprintSystem

void UK2Node_MapRemoveArrayItem::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    UClass* ActionKey = GetClass();
    if (ActionRegistrar.IsOpenForRegistration(ActionKey))
    {
        UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
        check(NodeSpawner != nullptr);

        ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
    }
}

void UK2Node_MapRemoveArrayItem::PostReconstructNode()
{
    Super::PostReconstructNode();
    PropagatePinType();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region PinManagement

void UK2Node_MapRemoveArrayItem::AllocateDefaultPins()
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
	
    // 创建 Item 输入引脚
    UEdGraphPin* ItemPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, InputItemPinName);
    ItemPin->PinType.ContainerType = EPinContainerType::None;

    // 设置引脚默认值
    MapPin->PinFriendlyName = LOCTEXT("MapPin", "Target");
    KeyPin->PinFriendlyName = LOCTEXT("KeyPin", "Key");
    ItemPin->PinFriendlyName = LOCTEXT("ItemPin", "Item");

    Super::AllocateDefaultPins();
}

void UK2Node_MapRemoveArrayItem::PinDefaultValueChanged(UEdGraphPin* Pin)
{
    Super::PinDefaultValueChanged(Pin);
    PropagatePinType();
}

void UK2Node_MapRemoveArrayItem::ReconstructNode()
{
    Super::ReconstructNode();
    PropagatePinType();
}

void UK2Node_MapRemoveArrayItem::PinConnectionListChanged(UEdGraphPin* Pin)
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
            
            if (UEdGraphPin* ItemPin = GetInputItemPin())
            {
                ItemPin->BreakAllPinLinks();
            }
        }
    }

    // 更新引脚类型
    PropagatePinType();
}

void UK2Node_MapRemoveArrayItem::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
    Super::NotifyPinConnectionListChanged(Pin);
    PropagatePinType();
}

bool UK2Node_MapRemoveArrayItem::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
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
        if (OtherPin->PinType.PinValueType.TerminalCategory != UEdGraphSchema_K2::PC_Struct)
        {
            OutReason = TEXT("Map的Value必须是结构体类型");
            return true;
        }

        // 结构体成员检查
        if (const UScriptStruct* StructType = Cast<UScriptStruct>(OtherPin->PinType.PinValueType.TerminalSubCategoryObject.Get()))
        {
            FProperty* FirstProperty = StructType->PropertyLink;
            if (!FirstProperty)
            {
                OutReason = TEXT("结构体必须包含一个成员变量");
                return true;
            }

            if (FirstProperty->Next != nullptr)
            {
                OutReason = TEXT("结构体只能包含一个成员变量");
                return true;
            }

            if (!FirstProperty->IsA<FArrayProperty>())
            {
                OutReason = TEXT("结构体的成员必须是数组类型");
                return true;
            }
        }
        return false;
    }

    // 如果是 Key 或 Item 引脚，检查 Map 是否已连接
    if (MyPin->PinName == InputKeyPinName || MyPin->PinName == InputItemPinName)
    {
        UEdGraphPin* MapPin = GetInputMapPin();
        // 确保 MapPin 存在
        if (!MapPin)
        {
            return true; // 如果 MapPin 不存在，禁止连接
        }

        // 检查 Map 是否已连接
        bool bMapConnected = MapPin->LinkedTo.Num() > 0;
    
        // 如果 Map 未连接，禁止连接 Key 和 Item
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
        
        // 如果是 Item 引脚，检查类型匹配
        if (MyPin->PinName == InputItemPinName)
        {
            FEdGraphPinType ItemType = GetItemPinType();
            if (ItemType.PinCategory != UEdGraphSchema_K2::PC_Wildcard &&
                ItemType.PinCategory != OtherPin->PinType.PinCategory)
            {
                OutReason = TEXT("Value类型不匹配");
                return true;
            }
        }
    }


    
    return false;
}



const FName UK2Node_MapRemoveArrayItem::InputMapPinName(TEXT("MapPin"));
UEdGraphPin* UK2Node_MapRemoveArrayItem::GetInputMapPin() const
{
	return FindPin(InputMapPinName);
}

const FName UK2Node_MapRemoveArrayItem::InputKeyPinName(TEXT("KeyPin"));
UEdGraphPin* UK2Node_MapRemoveArrayItem::GetInputKeyPin() const
{
	return FindPin(InputKeyPinName);
}

const FName UK2Node_MapRemoveArrayItem::InputItemPinName(TEXT("ItemPin"));
UEdGraphPin* UK2Node_MapRemoveArrayItem::GetInputItemPin() const
{
	return FindPin(InputItemPinName);
}



FEdGraphPinType UK2Node_MapRemoveArrayItem::GetKeyPinType() const
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    
    // 获取Map引脚
    UEdGraphPin* MapPin = GetInputMapPin();
    if (MapPin && MapPin->LinkedTo.Num() > 0)
    {
        // 从连接的Map引脚获取类型信息
        const FEdGraphPinType& MapPinType = MapPin->LinkedTo[0]->PinType;
        if (MapPinType.ContainerType == EPinContainerType::Map)
        {
            // 直接使用Map的���类型（KeyPinType）来设置
            PinType.PinCategory = MapPinType.PinCategory;
            PinType.PinSubCategory = MapPinType.PinSubCategory;
            PinType.PinSubCategoryObject = MapPinType.PinSubCategoryObject;
            PinType.ContainerType = EPinContainerType::None;
        }
    }
    
    return PinType;
}

FEdGraphPinType UK2Node_MapRemoveArrayItem::GetItemPinType() const
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;

    // 获取Map引脚
    UEdGraphPin* MapPin = GetInputMapPin();
    if (MapPin && MapPin->LinkedTo.Num() > 0)
    {
        // 从连接的Map引脚获取类型信息
        const FEdGraphPinType& MapPinType = MapPin->LinkedTo[0]->PinType;
        if (MapPinType.ContainerType == EPinContainerType::Map)
        {
            // 如Item是结构体类型
            if (MapPinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Struct)
            {
                // 获取结构体类型
                if (const UScriptStruct* StructType = Cast<UScriptStruct>(MapPinType.PinValueType.TerminalSubCategoryObject.Get()))
                {
                    // 遍历结构体属性
                    for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
                    {
                        if (FProperty* Property = *PropIt)
                        {
                            // 获取第一个属性的类型
                            const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
                            Schema->ConvertPropertyToPinType(Property, PinType);

                            // 如果是数组类型，转换为单个元素类型
                            if (PinType.ContainerType == EPinContainerType::Array)
                            {
                                PinType.ContainerType = EPinContainerType::None;
                            }
                            break; // 只取第一个属性
                        }
                    }
                }
            }
            else
            {
                // 非结构体类型，使用原始类型
                PinType.PinCategory = MapPinType.PinValueType.TerminalCategory;
                PinType.PinSubCategoryObject = MapPinType.PinValueType.TerminalSubCategoryObject;
                PinType.ContainerType = EPinContainerType::None;
            }
        }
    }

    return PinType;
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region ReferenceHandling 

void UK2Node_MapRemoveArrayItem::PropagatePinType()
{
    UEdGraphPin* MapPin = GetInputMapPin();
    UEdGraphPin* KeyPin = GetInputKeyPin();
    UEdGraphPin* ItemPin = GetInputItemPin();

    // 如果 Map 引脚有连接
    if (MapPin && MapPin->LinkedTo.Num() > 0)
    {
        const FEdGraphPinType& MapPinType = MapPin->LinkedTo[0]->PinType;
        if (MapPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            MapPin->PinType = MapPinType;
        }

        // 使用GetKeyPinType和GetItemPinType获取类型
        if (KeyPin && KeyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            KeyPin->PinType = GetKeyPinType();
        }
    	
        if (ItemPin && ItemPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
        {
            ItemPin->PinType = GetItemPinType();
        }
    }
    else
    {
        // 重置为Wildcard类型
        if (MapPin)
        {
            MapPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
            MapPin->PinType.ContainerType = EPinContainerType::Map;
            
            // 重要：重置Map的键值类型
            FEdGraphTerminalType WildcardTerminal;
            WildcardTerminal.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
            MapPin->PinType.PinValueType = WildcardTerminal;
            
            // 清除可能残留的类型信息
            MapPin->PinType.PinSubCategoryObject = nullptr;
            MapPin->PinType.PinSubCategory = NAME_None;
        }
        
        if (KeyPin)
        {
            KeyPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
            KeyPin->PinType.ContainerType = EPinContainerType::None;
            KeyPin->PinType.PinSubCategoryObject = nullptr;
            KeyPin->PinType.PinSubCategory = NAME_None;
        }
    	
        if (ItemPin)
        {
            ItemPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
            ItemPin->PinType.ContainerType = EPinContainerType::None;
            ItemPin->PinType.PinSubCategoryObject = nullptr;
            ItemPin->PinType.PinSubCategory = NAME_None;
        }
    }

    GetGraph()->NotifyGraphChanged();
}

#pragma endregion

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#undef LOCTEXT_NAMESPACE