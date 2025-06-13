# UE5 K2Node 开发完整指南

> 基于智能排序节点 (K2Node_SmartSort) 的实战经验总结

## 📋 目录

- [基础架构](#基础架构)
- [核心API参考](#核心api参考)
- [引脚管理](#引脚管理)
- [类型系统](#类型系统)
- [编译展开](#编译展开)
- [事件处理](#事件处理)
- [最佳实践](#最佳实践)
- [常见问题](#常见问题)

---

## 基础架构

### 1. 类继承结构

```cpp
// 基础K2Node继承
UCLASS()
class SORT_API UK2Node_SmartSort : public UK2Node
{
    GENERATED_BODY()

public:
    // 构造函数 - 初始化编辑器专用数据
    UK2Node_SmartSort(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // 核心虚函数重写
    virtual void AllocateDefaultPins() override;
    virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
    virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
    virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
    virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
    virtual void PostReconstructNode() override;
    virtual void EarlyValidation(FCompilerResultsLog& MessageLog) const override;
    virtual FName GetCornerIcon() const override;
    virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
    virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
};
```

### 2. 编辑器条件编译

```cpp
#if WITH_EDITOR
// 所有K2Node相关代码都应该在编辑器条件编译内
#include "K2Node.h"
#include "K2Node_SmartSort.generated.h"

// 实现代码...

#endif // WITH_EDITOR
```

### 3. 引脚名称常量管理

```cpp
struct FSmartSort_Helper
{
    // 使用静态常量管理引脚名称，避免硬编码
    static const FName PN_TargetArray;
    static const FName PN_SortedArray;
    static const FName PN_OriginalIndices;
    static const FName PN_Ascending;
    static const FName PN_SortMode;
    static const FName PN_Location;
    static const FName PN_Direction;
    static const FName PN_Axis;

    // 编译时常量优化
    static constexpr int32 MaxDynamicPins = 3;
};

// 在.cpp文件中定义
const FName FSmartSort_Helper::PN_TargetArray(TEXT("TargetArray"));
const FName FSmartSort_Helper::PN_SortedArray(TEXT("SortedArray"));
const FName FSmartSort_Helper::PN_OriginalIndices(TEXT("OriginalIndices"));
// ...
```

---

## 核心API参考

### 1. 节点生命周期函数

#### AllocateDefaultPins() - 创建默认引脚
```cpp
void UK2Node_SmartSort::AllocateDefaultPins()
{
    // 1. 创建执行引脚
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

    // 2. 创建数据引脚
    UEdGraphPin* ArrayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, FSmartSort_Helper::PN_TargetArray);
    ArrayPin->PinType.ContainerType = EPinContainerType::Array;
    ArrayPin->PinToolTip = LOCTEXT("ArrayPin_Tooltip", "要排序的数组").ToString();

    // 3. 创建输出引脚
    UEdGraphPin* SortedArrayPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, FSmartSort_Helper::PN_SortedArray);
    SortedArrayPin->PinType.ContainerType = EPinContainerType::Array;
    SortedArrayPin->PinToolTip = LOCTEXT("SortedArrayPin_Tooltip", "排序后的数组").ToString();

    // 4. 创建索引输出引脚
    UEdGraphPin* IndicesPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, FSmartSort_Helper::PN_OriginalIndices);
    IndicesPin->PinType.ContainerType = EPinContainerType::Array;
    IndicesPin->PinToolTip = LOCTEXT("IndicesPin_Tooltip", "原始索引数组").ToString();

    // 5. 创建升序/降序引脚
    UEdGraphPin* AscendingPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, FSmartSort_Helper::PN_Ascending);
    AscendingPin->DefaultValue = TEXT("true");
    AscendingPin->PinToolTip = LOCTEXT("AscendingPin_Tooltip", "是否升序排列").ToString();

    // 6. 创建排序模式引脚（隐藏，动态显示）
    UEdGraphPin* ModePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, nullptr, FSmartSort_Helper::PN_SortMode);
    ModePin->bHidden = true;
    ModePin->PinToolTip = LOCTEXT("ModePin_Tooltip", "排序模式").ToString();

    // 7. 调用父类方法
    Super::AllocateDefaultPins();
}
```

#### ExpandNode() - 编译时展开
```cpp
void UK2Node_SmartSort::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    // 1. 验证输入
    UEdGraphPin* ArrayInputPin = GetArrayInputPin();
    if (!ArrayInputPin)
    {
        CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_NoArrayPin", "[智能排序] 节点 %% 缺少数组输入引脚。").ToString(), this);
        BreakAllNodeLinks();
        return;
    }

    if (ArrayInputPin->LinkedTo.Num() == 0)
    {
        CompilerContext.MessageLog.Warning(*LOCTEXT("SmartSort_NoArrayConnected", "[智能排序] 节点 %% 未连接任何数组。").ToString(), this);
        BreakAllNodeLinks();
        return;
    }

    // 2. 获取连接的数组类型
    FEdGraphPinType ConnectedType = ArrayInputPin->LinkedTo[0]->PinType;
    if (ConnectedType.ContainerType != EPinContainerType::Array)
    {
        CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_NotAnArray", "[智能排序] 连接的引脚不是数组类型。").ToString(), this);
        BreakAllNodeLinks();
        return;
    }

    // 3. 根据类型确定要调用的库函数名
    FName FunctionName;
    if (!DetermineSortFunction(ConnectedType, FunctionName))
    {
        CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_NoMatchingFunction", "找不到与选项匹配的排序函数 for node %%.").ToString(), this);
        BreakAllNodeLinks();
        return;
    }

    // 4. 创建函数调用节点
    UK2Node_CallFunction* CallFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    CallFunctionNode->FunctionReference.SetExternalMember(FunctionName, USortLibrary::StaticClass());
    CallFunctionNode->AllocateDefaultPins();

    // 5. 检查函数是否为Pure函数并进行相应处理
    UFunction* Function = CallFunctionNode->GetTargetFunction();
    bool bIsPureFunction = Function && Function->HasAnyFunctionFlags(FUNC_BlueprintPure);

    if (bIsPureFunction)
    {
        CreatePureFunctionExecutionFlow(CompilerContext, CallFunctionNode, SourceGraph);
    }
    else
    {
        // 连接执行引脚（非Pure函数）
        UEdGraphPin* MyExecPin = GetExecPin();
        UEdGraphPin* FuncExecPin = CallFunctionNode->GetExecPin();
        if (MyExecPin && FuncExecPin)
        {
            CompilerContext.MovePinLinksToIntermediate(*MyExecPin, *FuncExecPin);
        }
    }

    // 6. 连接引脚
    ConnectArrayInputPin(CompilerContext, ArrayInputPin, CallFunctionNode);
    ConnectOutputPins(CompilerContext, CallFunctionNode);
    ConnectDynamicInputPins(CompilerContext, CallFunctionNode);

    // 7. 清理原始连接
    BreakAllNodeLinks();
}
```

### 2. 引脚管理API

#### 创建引脚
```cpp
// 基础引脚创建
UEdGraphPin* Pin = CreatePin(
    EGPD_Input,                          // 方向：输入/输出
    UEdGraphSchema_K2::PC_Wildcard,      // 类型：通配符/具体类型
    PinName                              // 引脚名称
);

// 设置引脚属性
Pin->PinType.ContainerType = EPinContainerType::Array;  // 容器类型
Pin->PinToolTip = TEXT("提示信息");                      // 工具提示
Pin->DefaultValue = TEXT("默认值");                      // 默认值
Pin->bHidden = true;                                    // 隐藏状态
```

#### 引脚查找和访问
```cpp
// 通过名称查找引脚
FORCEINLINE UEdGraphPin* GetArrayInputPin() const
{
    return FindPin(FSmartSort_Helper::PN_TargetArray);
}

// 获取执行引脚
UEdGraphPin* GetExecPin() const
{
    return FindPin(UEdGraphSchema_K2::PN_Execute);
}

UEdGraphPin* GetThenPin() const
{
    return FindPin(UEdGraphSchema_K2::PN_Then);
}
```

#### 动态引脚管理
```cpp
void RebuildDynamicPins()
{
    // 1. 移除现有动态引脚
    static const TArray<FName> DynamicPinNames = {
        FSmartSort_Helper::PN_Location,
        FSmartSort_Helper::PN_Direction,
        FSmartSort_Helper::PN_Axis
    };

    // 反向迭代避免索引问题
    for (int32 i = Pins.Num() - 1; i >= 0; --i)
    {
        if (UEdGraphPin* Pin = Pins[i])
        {
            if (DynamicPinNames.Contains(Pin->PinName))
            {
                RemovePin(Pin);
            }
        }
    }

    // 2. 根据当前状态创建新引脚
    CreateDynamicPinsBasedOnType();
}
```

---

## 引脚管理

### 1. 引脚类型系统

#### 常用引脚类型
```cpp
// 基础数据类型
UEdGraphSchema_K2::PC_Boolean    // 布尔值
UEdGraphSchema_K2::PC_Byte       // 字节/枚举
UEdGraphSchema_K2::PC_Int        // 整数
UEdGraphSchema_K2::PC_Float      // 浮点数
UEdGraphSchema_K2::PC_String     // 字符串
UEdGraphSchema_K2::PC_Name       // 名称
UEdGraphSchema_K2::PC_Text       // 文本

// 复杂类型
UEdGraphSchema_K2::PC_Object     // 对象引用
UEdGraphSchema_K2::PC_Struct     // 结构体
UEdGraphSchema_K2::PC_Wildcard   // 通配符

// 执行类型
UEdGraphSchema_K2::PC_Exec       // 执行流
```

#### 容器类型
```cpp
// 设置数组类型
Pin->PinType.ContainerType = EPinContainerType::Array;

// 设置Set类型
Pin->PinType.ContainerType = EPinContainerType::Set;

// 设置Map类型
Pin->PinType.ContainerType = EPinContainerType::Map;
```

#### 结构体引脚
```cpp
// 创建Vector引脚
UEdGraphPin* VectorPin = CreatePin(
    EGPD_Input,
    UEdGraphSchema_K2::PC_Struct,
    TBaseStructure<FVector>::Get(),
    PinName
);

// 创建Transform引脚
UEdGraphPin* TransformPin = CreatePin(
    EGPD_Input,
    UEdGraphSchema_K2::PC_Struct,
    TBaseStructure<FTransform>::Get(),
    PinName
);
```

#### 枚举引脚
```cpp
// 创建枚举引脚
UEdGraphPin* EnumPin = CreatePin(
    EGPD_Input,
    UEdGraphSchema_K2::PC_Byte,
    StaticEnum<EYourEnumType>(),
    PinName
);

// 设置枚举默认值
void SetEnumPinDefaultValue(UEdGraphPin* EnumPin, UEnum* EnumClass)
{
    if (EnumPin && EnumClass && EnumClass->NumEnums() > 1)
    {
        FString FirstEnumName = EnumClass->GetNameStringByIndex(0);
        EnumPin->DefaultValue = FirstEnumName;
    }
}
```

### 2. 引脚连接管理

#### 引脚连接事件
```cpp
// 引脚连接改变时调用
void PinConnectionListChanged(UEdGraphPin* Pin) override
{
    Super::PinConnectionListChanged(Pin);

    // 防止递归调用
    if (bIsReconstructingPins) return;

    if (Pin && Pin->PinName == FSmartSort_Helper::PN_TargetArray)
    {
        bIsReconstructingPins = true;

        // 处理类型传播
        if (Pin->LinkedTo.Num() > 0)
        {
            PropagatePinType(Pin->LinkedTo[0]->PinType);
        }
        else
        {
            ResetToWildcardType();
        }

        // 重建动态引脚
        RebuildDynamicPins();

        // 标记蓝图需要重新编译
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
            FBlueprintEditorUtils::FindBlueprintForNode(this)
        );

        bIsReconstructingPins = false;
    }
}
```

#### 类型传播
```cpp
void PropagatePinType(const FEdGraphPinType& TypeToPropagate)
{
    UEdGraphPin* ArrayInputPin = GetArrayInputPin();
    UEdGraphPin* SortedArrayOutputPin = GetSortedArrayOutputPin();

    if (ArrayInputPin && SortedArrayOutputPin)
    {
        // 应用类型到相关引脚
        ArrayInputPin->PinType = TypeToPropagate;
        SortedArrayOutputPin->PinType = TypeToPropagate;

        // 确保引脚可见
        ArrayInputPin->bHidden = false;
        SortedArrayOutputPin->bHidden = false;

        // 标记引脚需要重绘
        ArrayInputPin->bWasTrashed = false;
        SortedArrayOutputPin->bWasTrashed = false;
    }
}
```

---

## 类型系统

### 1. 类型解析

#### 获取连接的类型
```cpp
FEdGraphPinType GetResolvedArrayType() const
{
    if (const UEdGraphPin* ArrayPin = GetArrayInputPin())
    {
        if (ArrayPin->LinkedTo.Num() > 0 && ArrayPin->LinkedTo[0])
        {
            return ArrayPin->LinkedTo[0]->PinType;
        }
    }

    // 返回通配符类型
    FEdGraphPinType WildcardType;
    WildcardType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    WildcardType.ContainerType = EPinContainerType::Array;
    return WildcardType;
}
```

#### 类型检查
```cpp
// 检查是否为Actor类型
bool IsActorType(const FEdGraphPinType& PinType) const
{
    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object &&
        PinType.PinSubCategoryObject.IsValid())
    {
        if (UClass* Class = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
        {
            return Class->IsChildOf(AActor::StaticClass());
        }
    }
    return false;
}

// 检查是否为Vector类型
bool IsVectorType(const FEdGraphPinType& PinType) const
{
    return PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
           PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get();
}
```

### 2. 枚举类型管理

#### 根据类型获取枚举
```cpp
UEnum* GetSortModeEnumForType(const FEdGraphPinType& ArrayType) const
{
    if (IsActorType(ArrayType))
    {
        return StaticEnum<ESmartSort_ActorSortMode>();
    }
    else if (IsVectorType(ArrayType))
    {
        return StaticEnum<ESmartSort_VectorSortMode>();
    }

    return nullptr; // 基础类型不需要枚举
}
```

#### 枚举值处理
```cpp
// 获取枚举值
uint8 GetEnumValue(UEdGraphPin* EnumPin, UEnum* EnumClass) const
{
    if (!EnumPin || !EnumClass) return 0;

    const FString DefaultStr = EnumPin->GetDefaultAsString();
    if (DefaultStr.IsEmpty()) return 0;

    int64 IntValue = EnumClass->GetValueByNameString(DefaultStr);
    return (IntValue != INDEX_NONE) ? static_cast<uint8>(IntValue) : 0;
}
```

---

## 编译展开

### 1. 函数节点创建

#### 创建调用节点
```cpp
// 创建函数调用节点
UK2Node_CallFunction* CallFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);

// 设置函数引用
CallFunctionNode->FunctionReference.SetExternalMember(
    GET_FUNCTION_NAME_CHECKED(USortLibrary, SortActorsByDistance),
    USortLibrary::StaticClass()
);

// 分配引脚
CallFunctionNode->AllocateDefaultPins();
```

#### Pure函数特殊处理
```cpp
void CreatePureFunctionExecutionFlow(FKismetCompilerContext& CompilerContext,
                                   UK2Node_CallFunction* CallFunctionNode,
                                   UEdGraph* SourceGraph)
{
    // 对于Pure函数，创建中间执行节点
    UK2Node_CallFunction* PassthroughNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    PassthroughNode->FunctionReference.SetExternalMember(
        GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString),
        UKismetSystemLibrary::StaticClass()
    );
    PassthroughNode->AllocateDefaultPins();

    // 连接执行流
    ConnectExecutionFlow(CompilerContext, PassthroughNode, CallFunctionNode);
}
```

### 2. 引脚连接

#### 连接引脚的核心API
```cpp
// 移动引脚连接到中间节点
CompilerContext.MovePinLinksToIntermediate(*SourcePin, *TargetPin);

// 直接连接两个引脚
SourcePin->MakeLinkTo(TargetPin);

// 断开引脚连接
SourcePin->BreakLinkTo(TargetPin);

// 断开引脚的所有连接
SourcePin->BreakAllPinLinks();
```

#### 查找目标函数的引脚
```cpp
bool ConnectArrayInputPin(FKismetCompilerContext& CompilerContext,
                         UEdGraphPin* ArrayInputPin,
                         UK2Node_CallFunction* CallFunctionNode)
{
    // 定义可能的数组输入引脚名称
    static const TArray<FString> ArrayInputPinNames = {
        TEXT("Actors"),
        TEXT("Vectors"),
        TEXT("InArray")
    };

    // 查找匹配的输入引脚
    for (const FString& PinName : ArrayInputPinNames)
    {
        if (UEdGraphPin* FuncInputArrayPin = CallFunctionNode->FindPin(*PinName, EGPD_Input))
        {
            CompilerContext.MovePinLinksToIntermediate(*ArrayInputPin, *FuncInputArrayPin);
            return true;
        }
    }

    return false;
}
```

---

## 事件处理

### 1. 节点重建事件

#### ReallocatePinsDuringReconstruction()
```cpp
void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override
{
    AllocateDefaultPins();

    // 先恢复连接，然后再重建动态引脚
    RestoreSplitPins(OldPins);

    // 重建动态引脚（在连接恢复后）
    RebuildDynamicPins();
}
```

#### PostReconstructNode()
```cpp
void PostReconstructNode() override
{
    Super::PostReconstructNode();

    // 确保引脚类型正确传播
    UEdGraphPin* ArrayInputPin = GetArrayInputPin();
    if (ArrayInputPin && ArrayInputPin->LinkedTo.Num() > 0)
    {
        PropagatePinType(ArrayInputPin->LinkedTo[0]->PinType);
    }

    // 确保动态引脚正确显示
    RebuildDynamicPins();
}
```

### 2. 验证和错误处理

#### EarlyValidation()
```cpp
void EarlyValidation(FCompilerResultsLog& MessageLog) const override
{
    Super::EarlyValidation(MessageLog);

    const UEdGraphPin* ArrayPin = GetArrayInputPin();
    if (!ArrayPin || ArrayPin->LinkedTo.Num() == 0)
    {
        MessageLog.Warning(*LOCTEXT("SmartSort_NoArray", "警告：[智能排序] 节点 %% 未连接任何数组。").ToString(), this);
    }
    else if (GetResolvedArrayType().PinCategory == UEdGraphSchema_K2::PC_Wildcard)
    {
        MessageLog.Error(*LOCTEXT("SmartSort_ResolveFailed", "错误：[智能排序] 节点 %% 未能解析出有效的数组类型。").ToString(), this);
    }
}
```

#### 连接限制
```cpp
bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override
{
    if (MyPin->PinName == FSmartSort_Helper::PN_TargetArray && OtherPin != nullptr && !OtherPin->PinType.IsArray())
    {
        OutReason = LOCTEXT("InputNotArray", "输入必须是一个数组。").ToString();
        return true;
    }
    return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}
```

---

## 最佳实践

### 1. 性能优化

#### 使用FORCEINLINE
```cpp
// 对于简单的访问器函数使用内联
FORCEINLINE UEdGraphPin* GetArrayInputPin() const
{
    return FindPin(FSmartSort_Helper::PN_TargetArray);
}
```

#### 静态常量优化
```cpp
// 使用静态常量避免重复分配
static const TArray<FName> DynamicPinNames = {
    FSmartSort_Helper::PN_Location,
    FSmartSort_Helper::PN_Direction,
    FSmartSort_Helper::PN_Axis
};

// 编译时常量
static constexpr int32 MaxDynamicPins = 3;
```

#### 编译时断言
```cpp
// 在构造函数中添加编译时验证
static_assert(static_cast<int32>(ESmartSort_ActorSortMode::ByAzimuth) == 4, "Actor排序模式枚举不完整");
static_assert(static_cast<int32>(ESmartSort_VectorSortMode::ByAxis) == 2, "Vector排序模式枚举不完整");
```

### 2. 内存管理

#### 智能指针使用
```cpp
// 使用TObjectPtr代替原始指针
UPROPERTY()
TObjectPtr<UEnum> CurrentSortEnum;
```

#### 空指针检查
```cpp
// 总是检查指针有效性
if (const UEdGraphPin* ArrayPin = GetArrayInputPin())
{
    if (ArrayPin->LinkedTo.Num() > 0 && ArrayPin->LinkedTo[0])
    {
        // 安全访问
    }
}
```

### 3. 错误处理

#### 详细的错误消息
```cpp
CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_NoArrayPin", "[智能排序] 节点 %% 缺少数组输入引脚。").ToString(), this);
```

#### 优雅的错误恢复
```cpp
if (!ArrayInputPin)
{
    CompilerContext.MessageLog.Error(*ErrorMessage, this);
    BreakAllNodeLinks();  // 清理连接
    return;               // 提前返回
}
```

---

## 常见问题

### 1. 引脚连接问题

**Q: 引脚连接后类型没有传播？**
A: 确保在`PinConnectionListChanged`中调用了`PropagatePinType`并标记蓝图需要重新编译。

**Q: 动态引脚没有正确显示？**
A: 检查`RebuildDynamicPins`的调用时机，确保在类型传播后调用。

### 2. 编译展开问题

**Q: ExpandNode中找不到目标函数？**
A: 检查函数名称是否正确，使用`GET_FUNCTION_NAME_CHECKED`宏确保类型安全。

**Q: Pure函数的执行流连接失败？**
A: Pure函数需要特殊处理，使用中间节点来连接执行流。

### 3. 性能问题

**Q: 节点重建时性能差？**
A: 使用递归标志防止重复调用，优化引脚查找算法。

**Q: 编译时间过长？**
A: 使用编译时常量和静态断言，减少运行时计算。

---

## 总结

这份指南涵盖了UE5 K2Node开发的核心概念和实践经验。通过遵循这些最佳实践，你可以创建出高质量、高性能的自定义蓝图节点。

关键要点：
- ✅ **正确的生命周期管理** - 理解节点的创建、重建和展开过程
- ✅ **类型安全** - 使用强类型检查和编译时验证
- ✅ **性能优化** - 使用内联、静态常量和高效算法
- ✅ **错误处理** - 提供详细的错误信息和优雅的错误恢复
- ✅ **用户体验** - 动态UI、智能默认值和直观的交互

记住：好的K2Node不仅功能强大，更要易于使用和维护！
```
```
```