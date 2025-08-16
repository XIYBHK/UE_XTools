#include "K2Node_SpawnActorFromPool.h"

#include "KismetCompiler.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "ObjectPoolLibrary.h"
#include "GameFramework/Actor.h"
#include "KismetCompilerMisc.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "K2Node_SpawnActorFromPool"

UK2Node_SpawnActorFromPool::UK2Node_SpawnActorFromPool(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    NodeTooltip = LOCTEXT("NodeTooltip", "尝试从对象池中生成一个新的Actor，使用指定的变换");
}

void UK2Node_SpawnActorFromPool::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    UClass* ActionKey = GetClass();
    if (ActionRegistrar.IsOpenForRegistration(ActionKey))
    {
        UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
        ActionRegistrar.AddBlueprintAction(ActionKey, Spawner);
    }
}

FText UK2Node_SpawnActorFromPool::GetMenuCategory() const
{
    return FText::FromString(TEXT("XTools|对象池|核心"));
}

FText UK2Node_SpawnActorFromPool::GetKeywords() const
{
    return LOCTEXT("SpawnFromPoolKeywords", "从池生成Actor 对象池 生成 创建 Actor 池化 复用 性能优化");
}

FText UK2Node_SpawnActorFromPool::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    FText NodeTitle = NSLOCTEXT("K2Node", "SpawnActorFromPool_BaseTitle", "从池生成Actor");
    if (TitleType != ENodeTitleType::MenuTitle)
    {
        if (UEdGraphPin* ClassPin = GetClassPin())
        {
            if (ClassPin->LinkedTo.Num() > 0)
            {
                // Blueprint will be determined dynamically, so we don't have the name in this case
                NodeTitle = NSLOCTEXT("K2Node", "SpawnActorFromPool_Title_Unknown", "从池生成Actor");
            }
            else if (ClassPin->DefaultObject == nullptr)
            {
                NodeTitle = NSLOCTEXT("K2Node", "SpawnActorFromPool_Title_NONE", "从池生成Actor NONE");
            }
            else
            {
                if (CachedNodeTitle.IsOutOfDate(this))
                {
                    FText ClassName;
                    if (UClass* PickedClass = Cast<UClass>(ClassPin->DefaultObject))
                    {
                        ClassName = PickedClass->GetDisplayNameText();
                    }

                    FFormatNamedArguments Args;
                    Args.Add(TEXT("ClassName"), ClassName);

                    // FText::Format() is slow, so we cache this to save on performance
                    CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "SpawnActorFromPool_Title_Class", "从池生成Actor {ClassName}"), Args), this);
                }
                NodeTitle = CachedNodeTitle;
            } 
        }
        else
        {
            NodeTitle = NSLOCTEXT("K2Node", "SpawnActorFromPool_Title_NONE", "从池生成Actor NONE");
        }
    }
    return NodeTitle;
}

FSlateIcon UK2Node_SpawnActorFromPool::GetIconAndTint(FLinearColor& OutColor) const
{
    // 使用与原生SpawnActorFromClass相同的图标
    static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SpawnActor_16x");
    return Icon;
}

void UK2Node_SpawnActorFromPool::AllocateDefaultPins()
{
    // 调用父类分配默认引脚
    Super::AllocateDefaultPins();
    
    // 更新返回值类型
    UpdateReturnValueType();
}

void UK2Node_SpawnActorFromPool::PinDefaultValueChanged(UEdGraphPin* Pin)
{
    Super::PinDefaultValueChanged(Pin);
    
    // 如果Class引脚的默认值改变，更新返回值类型
    if (Pin && Pin == GetClassPin())
    {
        UpdateReturnValueType();
    }
}

void UK2Node_SpawnActorFromPool::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
    Super::NotifyPinConnectionListChanged(Pin);
    
    // 如果Class引脚的连接改变，更新返回值类型
    if (Pin && Pin == GetClassPin())
    {
        UpdateReturnValueType();
    }
}

void UK2Node_SpawnActorFromPool::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
    // 调用父类的方法
    Super::ReallocatePinsDuringReconstruction(OldPins);
    
    // 确保返回值引脚类型正确
    UpdateReturnValueType();
}

void UK2Node_SpawnActorFromPool::UpdateReturnValueType()
{
    if (UEdGraphPin* ResultPin = GetResultPin())
    {
        // 获取当前设置的Class
        UEdGraphPin* ClassPin = GetClassPin();
        UClass* SpawnClass = nullptr;
        
        if (ClassPin)
        {
            // 首先检查连接的引脚
            if (ClassPin->LinkedTo.Num() > 0)
            {
                // 如果有连接，尝试从连接的引脚获取类型
                UEdGraphPin* LinkedPin = ClassPin->LinkedTo[0];
                if (LinkedPin->PinType.PinSubCategoryObject.IsValid())
                {
                    SpawnClass = Cast<UClass>(LinkedPin->PinType.PinSubCategoryObject.Get());
                }
            }
            else
            {
                // 如果没有连接，从默认值获取
                SpawnClass = Cast<UClass>(ClassPin->DefaultObject);
            }
        }
        
        // 设置返回值引脚为正确的Actor类型
        ResultPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        ResultPin->PinType.PinSubCategory = NAME_None;
        ResultPin->PinType.PinSubCategoryObject = (SpawnClass ? SpawnClass : AActor::StaticClass());
        
        // 标记缓存的节点标题为过期，以便下次获取时重新生成
        CachedNodeTitle.MarkDirty();
        
        // 刷新节点显示
        if (GetGraph())
        {
            GetGraph()->NotifyGraphChanged();
        }
    }
}

void UK2Node_SpawnActorFromPool::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // 完全对齐UE原生SpawnActorFromClass的ExpandNode实现模式
    // 参考：Engine/Source/Editor/BlueprintGraph/Private/K2Node_SpawnActorFromClass.cpp 第575-583行

    const UEdGraphSchema_K2* K2Schema = CompilerContext.GetSchema();
    
    // Pins on this node
    UEdGraphPin* ThisExec = FindPin(UEdGraphSchema_K2::PN_Execute);
    UEdGraphPin* ThisThen = FindPin(UEdGraphSchema_K2::PN_Then);
    UEdGraphPin* ThisClass = FindPinChecked(TEXT("Class"));
    UEdGraphPin* ThisTransform = FindPinChecked(TEXT("SpawnTransform"));
    UEdGraphPin* ThisReturn = GetResultPin();
    UEdGraphPin* ThisWorld = FindPin(TEXT("WorldContextObject"));
    if (!ThisWorld)
    {
        ThisWorld = FindPin(TEXT("WorldContext"));
    }

    // 解析Class用于GenerateAssignmentNodes（与原生第473行一致）
    UClass* ClassToSpawn = GetClassToSpawn();
    
    // 检查Class有效性（与原生第475-482行一致）
    if (!ThisClass || ((0 == ThisClass->LinkedTo.Num()) && (NULL == ClassToSpawn)))
    {
        CompilerContext.MessageLog.Error(*FText::Format(
            FText::FromString(TEXT("Spawn node {0} must have a {1} specified.")), 
            FText::FromString(GetNodeTitle(ENodeTitleType::ListView).ToString()), 
            FText::FromString(TEXT("Class"))
        ).ToString(), this, ThisClass);
        BreakAllNodeLinks();
        return;
    }

    //////////////////////////////////////////////////////////////////////////
    // 创建 'AcquireDeferredFromPool' 调用节点（对应原生BeginSpawn）
    UK2Node_CallFunction* CallAcquireNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    CallAcquireNode->SetFromFunction(FindUField<UFunction>(UObjectPoolLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UObjectPoolLibrary, AcquireDeferredFromPool)));
    CallAcquireNode->AllocateDefaultPins();

    UEdGraphPin* CallAcquireExec = CallAcquireNode->GetExecPin();
    UEdGraphPin* CallAcquireWorldContextPin = CallAcquireNode->FindPin(TEXT("WorldContext"));
    UEdGraphPin* CallAcquireActorClassPin = CallAcquireNode->FindPinChecked(TEXT("ActorClass"));
    UEdGraphPin* CallAcquireResult = CallAcquireNode->GetReturnValuePin();

    // 移动exec连接（对应原生第501行）
    CompilerContext.MovePinLinksToIntermediate(*ThisExec, *CallAcquireExec);

    // 处理Class连接（对应原生第503-512行）
    if (ThisClass->LinkedTo.Num() > 0)
    {
        CompilerContext.MovePinLinksToIntermediate(*ThisClass, *CallAcquireActorClassPin);
    }
    else
    {
        CallAcquireActorClassPin->DefaultObject = ClassToSpawn;
    }

    // 处理WorldContext连接（对应原生第514-518行）
    if (ThisWorld && CallAcquireWorldContextPin)
    {
        CompilerContext.MovePinLinksToIntermediate(*ThisWorld, *CallAcquireWorldContextPin);
    }

    //////////////////////////////////////////////////////////////////////////
    // 创建 'FinalizeSpawnFromPool' 调用节点（对应原生FinishSpawn）
    UK2Node_CallFunction* CallFinalizeNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    CallFinalizeNode->SetFromFunction(FindUField<UFunction>(UObjectPoolLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UObjectPoolLibrary, FinalizeSpawnFromPool)));
    CallFinalizeNode->AllocateDefaultPins();

    UEdGraphPin* CallFinalizeExec = CallFinalizeNode->GetExecPin();
    UEdGraphPin* CallFinalizeThen = CallFinalizeNode->GetThenPin();
    UEdGraphPin* CallFinalizeActor = CallFinalizeNode->FindPinChecked(TEXT("Actor"));
    UEdGraphPin* CallFinalizeTransform = CallFinalizeNode->FindPinChecked(TEXT("SpawnTransform"));
    UEdGraphPin* CallFinalizeWorldContext = CallFinalizeNode->FindPin(TEXT("WorldContext"));
    UEdGraphPin* CallFinalizeResult = CallFinalizeNode->GetReturnValuePin();

    // 移动Then连接（对应原生第557行）
    CompilerContext.MovePinLinksToIntermediate(*ThisThen, *CallFinalizeThen);

    // 复制Transform连接（对应原生第560行）
    CompilerContext.MovePinLinksToIntermediate(*ThisTransform, *CallFinalizeTransform);

    // 复制WorldContext连接
    if (ThisWorld && CallFinalizeWorldContext)
    {
        CompilerContext.CopyPinLinksToIntermediate(*CallAcquireWorldContextPin, *CallFinalizeWorldContext);
    }

    // 连接Actor从Acquire到Finalize（对应原生第566行）
    CallAcquireResult->MakeLinkTo(CallFinalizeActor);

    // 移动返回值连接 - 应该连接到AcquireResult（Actor），而不是FinalizeResult（bool）
    if (ThisReturn && CallAcquireResult)
    {
        CallAcquireResult->PinType = ThisReturn->PinType; // 复制类型以使用正确的Actor子类
        CompilerContext.MovePinLinksToIntermediate(*ThisReturn, *CallAcquireResult);
    }

    //////////////////////////////////////////////////////////////////////////
    // 创建属性赋值节点（完全对应原生第575-576行）
    UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(
        CompilerContext, SourceGraph, CallAcquireNode, this, CallAcquireResult, ClassToSpawn);

    // 连接赋值链到Finalize（完全对应原生第579行）
    LastThen->MakeLinkTo(CallFinalizeExec);

    // 断开原节点所有连接（对应原生第582行）
    BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
