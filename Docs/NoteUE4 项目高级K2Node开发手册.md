# NoteUE4 项目高级K2Node开发手册

> **核心思想**: 本手册严格基于 `NoteUE4` 项目的源码，提供其中核心蓝图节点（K2Node）的实现原理分析与API用法示例。
> **参考源 (Reference Source)**: [https://github.com/xusjtuer/NoteUE4/tree/master](https://github.com/xusjtuer/NoteUE4/tree/master)

---

## 目录
1.  [模板：K2Node 最小实现](#1-模板k2node-最小实现)
2.  [案例分析：按名称获取属性节点 (K2Node_GetPropertyByName)](#2-案例分析按名称获取属性节点-k2node_getpropertybyname)
3.  [案例分析：动态执行分支节点 (K2Node_ExecutionBranch)](#3-案例分析动态执行分支节点-k2node_executionbranch)
4.  [案例分析：通用数组排序节点 (K2Node_ArraySort)](#4-案例分析通用数组排序节点-k2node_arraysort)
5.  [案例分析：通用函数调用节点 (K2Node_Generic)](#5-案例分析通用函数调用节点-k2node_generic)

---

## 1. 模板：K2Node 最小实现

> **说明**: 本节提供了一个创建自定义蓝图节点的最基础模板。虽然`NoteUE4`项目中没有完全对应的如此简单的节点，但其所有高级节点的实现都遵循此基本结构。理解本节是后续案例分析的基础。

#### 🔹 目标
- 创建一个基础的、能执行C++逻辑的蓝图节点。
- 节点包含输入/输出执行引脚，以及数据引脚。
- 编译时，节点会展开成对一个静态库函数的调用。

#### 🔹 `MyK2Node.h` (模板)
```cpp
#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "MyK2Node.generated.h"

UCLASS()
class MYEDITOREXTENSION_API UMyK2Node : public UK2Node
{
    GENERATED_BODY()

public:
    // UEdGraphNode interface
    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    // End of UEdGraphNode interface

    // UK2Node interface
    virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
    // End of UK2Node interface
};
```

#### 🔹 `MyK2Node.cpp` (模板)
```cpp
#include "MyK2Node.h"
#include "MyBlueprintFunctionLibrary.h" // 假设运行时函数在另一个库中
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"

void UMyK2Node::AllocateDefaultPins()
{
    // 创建执行引脚
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

    // 创建数据引脚 (示例)
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, TEXT("Input"));
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, TEXT("Output"));
    
    Super::AllocateDefaultPins();
}

FText UMyK2Node::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return FText::FromString("My Custom Node");
}

void UMyK2Node::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    // 1. 创建函数调用节点
    UK2Node_CallFunction* CallFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    
    // 2. 设置要调用的运行时函数
    const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UMyBlueprintFunctionLibrary, MyStaticFunction);
    CallFunctionNode->FunctionReference.SetExternalMember(FunctionName, UMyBlueprintFunctionLibrary::StaticClass());
    CallFunctionNode->AllocateDefaultPins();

    // 3. 移动所有引脚的连接到新创建的函数调用节点上
    CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CallFunctionNode->GetExecPin());
    CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("Input")), *CallFunctionNode->FindPinChecked(TEXT("FuncInput")));
    CompilerContext.MovePinLinksToIntermediate(*FindPin(TEXT("Output")), *CallFunctionNode->GetReturnValuePin());
    CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *CallFunctionNode->GetThenPin());

    // 4. 断开自身所有连接，因为它已被替换
    BreakAllNodeLinks();
}
```

---

## 2. 案例分析：按名称获取属性节点 (K2Node_GetPropertyByName)

此案例是学习UE反射系统在K2Node中应用的最佳入门。它实现了一个可以从任意对象（Object）中，通过提供一个属性名称（FName），来获取该属性值的“智能”节点。

#### 🔹 目标
- 创建一个 `Get Property By Name` 节点。
- 节点输入一个`Object`和一个`FName`（属性名）。
- 节点输出一个通配符（Wildcard），其类型会根据找到的属性动态变化。

#### 🔹 技术拆解
- **通配符与类型传播**: 节点的核心是利用通配符引脚。当输入`Object`连接后，节点并不立即知道输出类型。只有在编译时（ExpandNode阶段），通过反射找到属性后，才能确定输出引脚的真正类型。
- **反射查找属性**: 使用 `FindFProperty` 在运行时根据 `FName` 查找`UObject`的`FProperty`。
- **泛型函数库**: 将运行时的取值逻辑封装在一个 `BlueprintCallable` 函数中，供`ExpandNode`调用。

#### 🔹 参考文件
- **节点实现**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/K2Node_GetPropertyByName.h`
- **节点实现**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/K2Node_GetPropertyByName.cpp`
- **运行时逻辑**: `/GET&SET Property/Source/GETandSET/Public/GetAndSet.h`
- **运行时逻辑**: `/GET&SET Property/Source/GET&SET/Private/GetAndSet.cpp`

#### 🔹 关键代码片段

**1. 节点定义 (`K2Node_GetPropertyByName.h`)**:
最引人注目的是它只有一个通配符输出引脚。
```cpp
// K2Node_GetPropertyByName.h
UCLASS()
class AWESOMEBLUEPRINTS_API UK2Node_GetPropertyByName : public UK2Node
{
    // ...
    // Pin accessors
    UEdGraphPin* GetTargetPin() const;
    UEdGraphPin* GetPropertyNamePin() const;
    UEdGraphPin* GetValuePin() const;
};
```

**2. 运行时取值函数 (`GetAndSet.h`)**:
这是一个使用 `CustomThunk` 的泛型函数，能处理任意属性类型。
```cpp
// GetAndSet.h
UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value"), Category = "Development")
static void GetObjectProperty(UObject* Object, FName PropertyName, int32& Value);
```

**3. Thunk实现 (`GetAndSet.cpp`)**:
手动解析参数，并调用一个真正干活的泛型函数。
```cpp
// GetAndSet.cpp
DECLARE_FUNCTION(execGetObjectProperty)
{
    P_GET_OBJECT(UObject, Object);
    P_GET_PROPERTY(FNameProperty, PropertyName);

    // ... 手动处理通配符参数 ...
    Stack.Step(Stack.Object, NULL);
    FProperty* ValueProp = Stack.MostRecentProperty;
    void* ValueAddr = Stack.MostRecentPropertyAddress;

    P_FINISH;
    P_NATIVE_BEGIN
    GenericGetObjectProperty(Object, PropertyName, ValueProp, ValueAddr);
    P_NATIVE_END
}
```

**4. 编译时展开 (`K2Node_GetPropertyByName.cpp`)**:
在 `ExpandNode` 中，节点生成对 `GetObjectProperty` 函数的调用。
```cpp
// K2Node_GetPropertyByName.cpp
void UK2Node_GetPropertyByName::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // ...
    UK2Node_CallFunction* CallFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    CallFunction->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UGetAndSet, GetObjectProperty), UGetAndSet::StaticClass());
    CallFunction->AllocateDefaultPins();

    // ... 连接输入引脚 ...
    
    // 关键：将通配符输出引脚的类型设置为我们通过反射找到的属性类型
    UEdGraphPin* ValuePin = GetValuePin();
    UEdGraphPin* FunctionValuePin = CallFunction->FindPin(TEXT("Value"));
    FunctionValuePin->PinType = ValuePin->PinType;

    CompilerContext.MovePinLinksToIntermediate(*ValuePin, *FunctionValuePin);

    BreakAllNodeLinks();
}
```

---

## 3. 案例分析：动态执行分支节点 (K2Node_ExecutionBranch)

此案例实现了一个能根据 `UEnum` 动态创建多个执行输出引脚的节点，是 `Switch on Enum` 节点的执行流版本，能让蓝图逻辑更整洁。

#### 🔹 目标
- 创建一个节点，其输出执行引脚的数量和名称由一个可配置的 `UEnum` 资产决定。
- 在编辑器中更改节点上选择的枚举类型后，输出引脚能自动更新。

#### 🔹 技术拆解
- **动态引脚生成**: 当用户在节点详情中更改了指定的 `UEnum` 资产时，通过覆写 `PostEditChangeProperty` 来捕获这个事件，并调用 `ReconstructNode()` 触发节点重建。
- **引脚重建**: 在 `ReallocatePinsDuringReconstruction` 函数中，根据当前选定的`UEnum`，清空旧的动态引脚并创建新的执行输出引脚。
- **编译时路由**: 在 `ExpandNode` 中，将此节点展开（替换）成一个标准的 `UK2Node_SwitchEnum` 节点，并将所有连线无缝地迁移过去。

#### 🔹 参考文件
- **节点实现**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/K2Node_ExecutionBranch.h`
- **节点实现**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/K2Node_ExecutionBranch.cpp`

#### 🔹 关键代码片段

**1. 保存枚举类型 (`K2Node_ExecutionBranch.h`)**:
```cpp
// K2Node_ExecutionBranch.h
UPROPERTY(EditAnywhere, Category = "Branch")
TObjectPtr<UEnum> Enum;
```

**2. 捕获属性变化并触发重建 (`K2Node_ExecutionBranch.cpp`)**:
```cpp
// K2Node_ExecutionBranch.cpp
void UK2Node_ExecutionBranch::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_ExecutionBranch, Enum))
    {
        // 当 Enum 属性变化时，重建节点
        ReconstructNode();
    }
    Super::PostEditChangeProperty(PropertyChangedEvent);
}
```

**3. 创建动态引脚 (`K2Node_ExecutionBranch.cpp`)**:
```cpp
// K2Node_ExecutionBranch.cpp
void UK2Node_ExecutionBranch::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
    Super::ReallocatePinsDuringReconstruction(OldPins);

    if (Enum)
    {
        // 遍历选定枚举的所有成员
        for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
        {
            // 为每个成员创建一个执行输出引脚
            FName PinName = FName(*Enum->GetNameStringByIndex(i));
            CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, PinName);
        }
    }
}
```
**4. 编译时展开为Switch节点 (`K2Node_ExecutionBranch.cpp`)**:
```cpp
// K2Node_ExecutionBranch.cpp
void UK2Node_ExecutionBranch::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // ...
    UK2Node_SwitchEnum* SwitchNode = CompilerContext.SpawnIntermediateNode<UK2Node_SwitchEnum>(this, SourceGraph);
    SwitchNode->Enum = this->Enum;
    SwitchNode->AllocateDefaultPins();
    
    // ... 移动所有输入和输出引脚的连接到SwitchNode上 ...

    BreakAllNodeLinks();
}
```
---

## 4. 案例分析：通用数组排序节点 (K2Node_ArraySort)

此案例是 `NoteUE4` 项目的皇冠明珠，它实现了一个能按任意属性对任意数组进行排序的高级节点，完美融合了反射、泛型函数和自定义UI三项高级技术。

#### 🔹 目标
- 创建一个 `Array Sort` 节点。
- 接受**任意类型**的数组（结构体、对象等）。
- 自动识别数组元素的属性，并通过一个**下拉菜单**让用户选择排序依据。

#### 🔹 技术拆解
- **动态属性发现 (Reflection)**: 当数组引脚连接后，通过 `TFieldIterator` 遍历连接的结构体或类的属性，筛选出可排序的（如数字类型），并将其名称列表缓存起来。
- **泛型函数调用 (CustomThunk)**: 核心运行时逻辑在一个 `CustomThunk` 函数中。该函数能接收 `void*` 指针和 `FProperty*` 描述，从而对任意类型的数组进行操作。
- **自定义引脚UI (Slate & PinFactory)**: 创建一个自定义的 `SGraphPin` 控件（一个下拉菜单），并通过 `FGraphPanelPinFactory` 注册它。当编辑器需要绘制该节点的属性选择引脚时，会使用这个自定义控件，并用第一步获取的属性名列表来填充菜单选项。

#### 🔹 参考文件
- **节点实现**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/K2Node_ArraySort.h`
- **节点实现**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/K2Node_ArraySort.cpp`
- **自定义UI**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/SGraphPinArraySortPropertyName.h`
- **自定义UI**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/SGraphPinArraySortPropertyName.cpp`
- **Pin工厂**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/K2BlueprintGraphPanelPinFactory.cpp`
- **运行时逻辑**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Public/AwesomeFunctionLibrary.h`
- **运行时逻辑**: `/AwesomeBlueprints/Source/AwesomeBlueprints/Private/AwesomeFunctionLibrary.cpp`

#### 🔹 关键代码片段

**1. 动态获取属性名 (`K2Node_ArraySort.cpp`)**:
```cpp
// K2Node_ArraySort.cpp
TArray<FString> UK2Node_ArraySort::GetPropertyNames()
{
    TArray<FString> PropertyNames;
    UEdGraphPin* TargetArrayPin = GetTargetArrayPin();

    if (TargetArrayPin && TargetArrayPin->LinkedTo.Num() > 0)
    {
        // ... 获取连接引脚的内部类型 (结构体或类) ...
        if (UScriptStruct* InnerStruct = Cast<UScriptStruct>(...))
        {
            // 使用 TFieldIterator 遍历所有 FProperty
            for (TFieldIterator<FProperty> It(InnerStruct); It; ++It)
            {
                if (CastField<FNumericProperty>(*It))
                {
                    PropertyNames.Add((*It)->GetName());
                }
            }
        }
    }
    return PropertyNames;
}
```

**2. 运行时排序的 `CustomThunk` 函数 (`AwesomeFunctionLibrary.cpp`)**:
```cpp
// AwesomeFunctionLibrary.cpp
DECLARE_FUNCTION(execArray_SortV2)
{
    // ... 手动解析通配符数组参数和其它参数 ...
    
    P_NATIVE_BEGIN
    // 调用真正的泛型排序函数
    GenericArray_SortV2(ArrayAddr, ArrayProp, PropertyName, bAscending);
    P_NATIVE_END
}
```

**3. 自定义Pin的工厂 (`K2BlueprintGraphPanelPinFactory.cpp`)**:
```cpp
// K2BlueprintGraphPanelPinFactory.cpp
TSharedPtr<class SGraphPin> FK2BlueprintGraphPanelPinFactory::CreatePin(class UEdGraphPin* InPin) const
{
    if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
    {
        if (UK2Node_ArraySort* ArraySortNode = Cast<UK2Node_ArraySort>(InPin->GetOuter()))
        {
            // ...
            // 创建并返回自定义的 SGraphPinArraySortPropertyName 控件
            return SNew(SGraphPinArraySortPropertyName, InPin, PropertyNames);
        }
    }
    return nullptr;
}
```

---

## 5. 案例分析：通用函数调用节点 (K2Node_Generic)

此案例实现了一个“元节点”：它本身不执行特定逻辑，而是通过一个可编辑的函数名，来动态地变成对该函数的调用。

#### 🔹 目标
- 创建一个 `Generic Call` 节点。
- 在节点的细节面板中可以输入一个 `FunctionName`。
- 输入函数名后，节点会自动生成与该函数签名匹配的所有输入输出引脚。

#### 🔹 技术拆解
- **数据驱动的节点重建**: 与 `K2Node_ExecutionBranch` 类似，通过覆写 `PostEditChangeProperty` 来监听 `FunctionName` 属性的变化，一旦变化就调用 `ReconstructNode()`。
- **反射查找函数签名**: 在重建过程中，使用 `FindField<UFunction>` 在全局查找用户输入的函数名。找到 `UFunction` 后，遍历其 `FProperty` 参数来创建匹配的引脚。
- **编译时动态绑定**: 在 `ExpandNode` 中，将节点展开成一个 `UK2Node_CallFunction`，但与之前案例不同的是，其调用的目标函数是根据节点实例的 `FunctionName` 属性动态决定的。

#### 🔹 参考文件
- **节点实现**: `/GenericNode/Source/GenericGraph/Private/K2Node_Generic.h`
- **节点实现**: `/GenericNode/Source/GenericGraph/Private/K2Node_Generic.cpp`
- **Slate UI**: `/GenericNode/Source/GenericGraph/Private/SGraphNodeGeneric.h`
- **Slate UI**: `/GenericNode/Source/GenericGraph/Private/SGraphNodeGeneric.cpp`

#### 🔹 关键代码片段

**1. 保存函数名和类 (`K2Node_Generic.h`)**:
```cpp
// K2Node_Generic.h
UPROPERTY(EditAnywhere, Category = "Generic")
TSubclassOf<UObject> FunctionClass;

UPROPERTY(EditAnywhere, Category = "Generic")
FName FunctionName;
```

**2. 查找函数并创建引脚 (`K2Node_Generic.cpp`)**:
```cpp
// K2Node_Generic.cpp
void UK2Node_Generic::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
    // ...
    if (FunctionClass)
    {
        // 查找函数
        UFunction* Function = FunctionClass->FindFunctionByName(FunctionName);
        if (Function)
        {
            // 根据函数签名创建引脚
            CreatePinsForFunction(Function);
        }
    }
    // ...
}
```

**3. 动态展开为指定函数调用 (`K2Node_Generic.cpp`)**:
```cpp
// K2Node_Generic.cpp
void UK2Node_Generic::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // ...
    UFunction* Function = FunctionClass ? FunctionClass->FindFunctionByName(FunctionName) : nullptr;
    if (Function)
    {
        UK2Node_CallFunction* CallFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
        CallFunction->SetFromFunction(Function); // 直接从UFunction对象设置
        CallFunction->AllocateDefaultPins();

        // ... 动态地将本节点的引脚连接移动到CallFunction节点的对应引脚上 ...
    }
    // ...
    BreakAllNodeLinks();
}
