# UE K2Node 开发权威指南

> **适用版本：Unreal Engine 5.3+**
>
> 本指南旨在提供一个全面、深入的K2Node开发手册，整合了从基础概念、核心API、高级技巧到最佳实践的完整知识体系。

---

## 📋 目录

1.  [**第一部分：核心概念与生命周期**](#第一部分核心概念与生命周期)
    -   [1.1 什么是K2Node？](#11-什么是k2node)
    -   [1.2 核心类结构](#12-核心类结构)
    -   [1.3 节点生命周期](#13-节点生命周期)
2.  [**第二部分：引脚管理与类型系统**](#第二部分引脚管理与类型系统)
    -   [2.1 引脚的创建与配置](#21-引脚的创建与配置)
    -   [2.2 动态引脚管理](#22-动态引脚管理)
    -   [2.3 类型系统与通配符(Wildcard)](#23-类型系统与通配符wildcard)
    -   [2.4 引脚事件处理](#24-引脚事件处理)
3.  [**第三部分：编译与展开 (ExpandNode)**](#第三部分编译与展开-expandnode)
    -   [3.1 `ExpandNode` 的作用](#31-expandnode-的作用)
    -   [3.2 实现 `ExpandNode` 的步骤](#32-实现-expandnode-的步骤)
    -   [3.3 Pure函数的特殊处理](#33-pure函数的特殊处理)
4.  [**第四部分：高级技术专题：通用数组排序节点解析**](#第四部分高级技术专题通用数组排序节点解析)
    -   [4.1 案例背景：创建全能排序节点](#41-案例背景创建全能排序节点)
    -   [4.2 核心技术1：利用反射动态发现属性](#42-核心技术1利用反射动态发现属性)
    -   [4.3 核心技术2：使用`CustomThunk`处理任意数组类型](#43-核心技术2使用customthunk处理任意数组类型)
    -   [4.4 核心技术3：通过Slate自定义引脚UI](#44-核心技术3通过slate自定义引脚ui)
5.  [**第五部分：UI/UX与最佳实践**](#第五部分uiux与最佳实践)
    -   [5.1 提升用户体验 (UI/UX)](#51-提升用户体验-uiux)
    -   [5.2 性能与安全](#52-性能与安全)
    -   [5.3 版本兼容与模块化](#53-版本兼容与模块化)
6.  [**第九部分：核心原则：编辑器与运行时模块分离**](#第九部分核心原则编辑器与运行时模块分离)
    -   [9.1 为什么需要分离？](#91-为什么需要分离)
    -   [9.2 如何正确分离模块？](#92-如何正确分离模块)
    -   [9.3 实践步骤示例](#93-实践步骤示例)

---

## 第一部分：核心概念与生命周期

### 1.1 什么是K2Node？

`UK2Node` 是虚幻引擎中用于创建自定义蓝图节点的C++基类。与简单的 `UFUNCTION` 不同，K2Node 提供了对节点外观、引脚（Pins）、行为和编译过程的完全控制。它允许你创建具有复杂逻辑、动态引脚和自定义UI的"智能"节点，这些是纯 `UFUNCTION` 无法实现的。

### 1.2 核心类结构

一个基本的K2Node至少需要一个头文件和一个源文件，并且所有编辑器相关的代码都必须包裹在 `#if WITH_EDITOR` 宏内。

```cpp
// MyK2Node.h
#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "MyK2Node.generated.h"

UCLASS()
class MYMODULE_API UMyK2Node : public UK2Node
{
    GENERATED_BODY()

public:
    //~ Begin UEdGraphNode Interface
    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    //~ End UEdGraphNode Interface

    //~ Begin UK2Node Interface
    virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
    //~ End UK2Node Interface
};
```

### 1.3 节点生命周期

理解节点的生命周期是避免bug的关键。

**生命周期流程:**

* **首次放置**:
    * `AllocateDefaultPins()`
* **节点重建 (当类型变化或手动刷新时)**:
    1.  `ReconstructNode()` (入口)
    2.  `ReallocatePinsDuringReconstruction()`
    3.  `RestoreSplitPins()` & `RewireOldPinsToNewPins()` (引擎自动处理连线迁移)
    4.  `PostReconstructNode()`
* **蓝图编译**:
    * `ExpandNode()` (将节点转译为底层代码)

**关键函数解释**:
-   **`AllocateDefaultPins`**: 节点首次被放置到图表中时调用，用于创建最基础的、默认状态的引脚。
-   **`ReconstructNode`**: 当节点的结构需要改变时（如用户连接了一个不同类型的引脚），由引擎或手动调用此函数。它负责管理整个重建过程。
-   **`ReallocatePinsDuringReconstruction`**: 在 `ReconstructNode` 内部调用，是重新创建引脚的主要场所。
-   **`PostReconstructNode`**: 所有引脚重建和连接恢复后调用，适合进行最终的清理或状态更新。
-   **`ExpandNode`**: 在蓝图**编译**时调用。这是K2Node的核心，它将自身"展开"或"转译"为一个或多个更简单的底层节点（通常是 `UK2Node_CallFunction`）。最终打包的游戏中不存在K2Node，只存在它展开后的结果。

---

## 第二部分：引脚管理与类型系统

### 2.1 引脚的创建与配置

引脚在 `AllocateDefaultPins` 或 `ReallocatePinsDuringReconstruction` 中使用 `CreatePin` 函数创建。

```cpp
// 在 AllocateDefaultPins 中创建引脚
// 1. 创建执行引脚
CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

// 2. 创建一个通配符(Wildcard)数组输入引脚
UEdGraphPin* InArrayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, TEXT("InArray"));
InArrayPin->PinType.ContainerType = EPinContainerType::Array;
InArrayPin->PinToolTip = LOCTEXT("InArray_Tooltip", "The array to process.");

// 3. 创建一个布尔输入引脚并设置默认值
UEdGraphPin* bAscendingPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, TEXT("bAscending"));
bAscendingPin->DefaultValue = TEXT("true");
```

### 2.2 动态引脚管理

当节点需要根据用户输入动态添加或删除引脚时，通常的模式是：

1.  在 `PinConnectionListChanged` 或 `PinDefaultValueChanged` 中检测到变化。
2.  调用 `ReconstructNode()` 来触发刷新。
3.  在 `ReallocatePinsDuringReconstruction` 中，根据当前的节点状态（例如，一个枚举下拉框的选择），决定创建哪些动态引脚。

### 2.3 类型系统与通配符(Wildcard)

通配符引脚是K2Node强大功能的核心。它允许你的节点接受多种数据类型。

**类型传播流程**:

1.  **初始状态**：在 `AllocateDefaultPins` 中，将需要支持多种类型的输入和输出引脚创建为 `PC_Wildcard`。
2.  **连接触发**：当用户将一个具有具体类型的引脚连接到你的通配符输入引脚时，`PinConnectionListChanged` 事件被触发。
3.  **类型同步**：在该事件的回调中，获取连接引脚的 `PinType`。
4.  **刷新节点**：将获取到的 `PinType` 应用到所有相关的通配符引脚上，然后调用 `ReconstructNode()` 来刷新整个节点。如果需要，你也可以在此刻根据新的类型创建额外的动态引脚。
5.  **断开连接**：如果用户断开了连接，你需要将引脚类型重置回 `PC_Wildcard` 并再次重建节点。

### 2.4 引脚事件处理

-   `PinConnectionListChanged(UEdGraphPin* Pin)`: 当任何引脚的连接状态发生改变时调用。这是实现类型传播和动态引脚逻辑的核心位置。
-   `PinDefaultValueChanged(UEdGraphPin* Pin)`: 当用户修改了某个引脚的默认值时调用。常用于根据枚举选项改变节点行为的场景。

---

## 第三部分：编译与展开 (ExpandNode)

### 3.1 `ExpandNode` 的作用

`ExpandNode` 是K2Node与最终执行代码之间的桥梁。在蓝图编译期间，编译器会调用每个K2Node的 `ExpandNode` 函数，该函数负责将自身替换为一组标准的、可执行的底层节点（通常是 `UK2Node_CallFunction`）。

这个过程是必不可少的，因为K2Node本身只存在于编辑器中，它不包含任何运行时逻辑。所有的运行时功能都必须由一个或多个 `UFUNCTION` 提供，而 `ExpandNode` 的工作就是生成对这些 `UFUNCTION` 的调用。

### 3.2 实现 `ExpandNode` 的步骤

```cpp
void UMyK2Node::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    // 1. 验证输入引脚是否已连接，如果未连接则报错。
    UEdGraphPin* MyInputPin = GetMyInputPin();
    if (MyInputPin == nullptr || MyInputPin->LinkedTo.Num() == 0)
    {
        CompilerContext.MessageLog.Error(TEXT("Error: Input pin on @@ must be connected."), this);
        BreakAllNodeLinks(); // 出错时断开所有连接，防止产生无效代码
        return;
    }

    // 2. 创建一个函数调用节点 (UK2Node_CallFunction)
    UK2Node_CallFunction* CallFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    
    // 3. 设置要调用的函数 (该函数必须是静态的，通常在BlueprintFunctionLibrary中)
    CallFunctionNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UMyBlueprintFunctionLibrary, MyRuntimeFunction), UMyBlueprintFunctionLibrary::StaticClass());
    CallFunctionNode->AllocateDefaultPins();

    // 4. 将K2Node的引脚连接"移动"到函数调用节点的对应引脚上
    // 移动执行引脚
    CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CallFunctionNode->GetExecPin());
    CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *CallFunctionNode->GetThenPin());

    // 移动数据引脚
    UEdGraphPin* FunctionInputPin = CallFunctionNode->FindPinChecked(TEXT("MyFunctionInputParamName"));
    CompilerContext.MovePinLinksToIntermediate(*MyInputPin, *FunctionInputPin);

    UEdGraphPin* MyOutputPin = GetMyOutputPin();
    UEdGraphPin* FunctionResultPin = CallFunctionNode->GetReturnValuePin();
    CompilerContext.MovePinLinksToIntermediate(*MyOutputPin, *FunctionResultPin);

    // 5. 断开当前K2Node的所有连接，因为它已经被CallFunctionNode替换了
    BreakAllNodeLinks();
}
```

### 3.3 Pure函数的特殊处理

如果要展开的函数是 `BlueprintPure`（没有执行引脚），`ExpandNode` 的处理方式略有不同。因为 `UK2Node_CallFunction` (for a pure function) 也没有执行引脚，你不能移动 `Exec/Then` 连接。这种情况下，你只需移动数据引脚的连接即可。如果你的K2Node本身有执行引脚但想调用一个pure函数，你可能需要生成一个"直通"节点（如 `UKismetSystemLibrary::Noop`）来承载执行流。

---

## 第四部分：高级技术专题：通用数组排序节点解析

本部分将以一个实际的开源项目 `K2Node_ArraySort` 为例，解析其背后的高级技术。

### 4.1 案例背景：创建全能排序节点

目标是创建一个蓝图节点，它可以：

1.  接受**任意类型**的数组（包括基础类型、结构体、对象）。
2.  如果数组元素是结构体或对象，**动态列出**其所有可排序的属性（如 `float`, `int`, `string` 等）。
3.  用户可以从下拉菜单中选择一个属性进行排序。
4.  支持升序和降序。

### 4.2 核心技术1：利用反射动态发现属性

为了动态获取结构体或类的属性列表，我们需要深入使用UE的C++反射系统。

```cpp
// 在K2Node类中的某个函数
TArray<FName> UMyArraySortNode::GetSortablePropertyNames()
{
    TArray<FName> Names;
    UEdGraphPin* InArrayPin = GetInArrayPin();

    // 检查输入数组引脚是否已连接
    if (InArrayPin && InArrayPin->LinkedTo.Num() > 0)
    {
        FEdGraphPinType ArrayInnerType = InArrayPin->LinkedTo[0]->PinType.PinSubCategoryObject.Get();

        // 如果是结构体
        if (UScriptStruct* Struct = Cast<UScriptStruct>(ArrayInnerType.PinSubCategoryObject.Get()))
        {
            // 使用TFieldIterator遍历其所有FProperty
            for (TFieldIterator<FProperty> It(Struct); It; ++It)
            {
                FProperty* Property = *It;
                // 筛选出可排序的属性类型（例如，数字、字符串等）
                if (CastField<FNumericProperty>(Property) || CastField<FStrProperty>(Property))
                {
                    Names.Add(Property->GetFName());
                }
            }
        }
        // 如果是UObject类
        else if (UClass* Class = Cast<UClass>(ArrayInnerType.PinSubCategoryObject.Get()))
        {
            // 同样的方法遍历
            for (TFieldIterator<FProperty> It(Class); It; ++It)
            {
                FProperty* Property = *It;
                if (CastField<FNumericProperty>(Property) || CastField<FStrProperty>(Property))
                {
                    Names.Add(Property->GetFName());
                }
            }
        }
    }
    return Names;
}
```

### 4.3 核心技术2：使用`CustomThunk`处理任意数组类型

标准 `UFUNCTION` 的参数类型是固定的。为了让一个函数能处理任意类型的数组，我们需要使用 `CustomThunk`。

**Thunking** 是一种元编程技术，允许你在函数被调用时，在底层手动解析参数并执行自定义逻辑。

1.  **在`UFUNCTION`中声明`CustomThunk`**:

    ```cpp
    // In a BlueprintFunctionLibrary
    UFUNCTION(BlueprintCallable, CustomThunk, meta = (ArrayParm = "TargetArray"))
    static void Array_Sort(const TArray<int32>& TargetArray, FName PropertyName);
    ```

    -   `CustomThunk` 告诉UE，这个函数的C++实现不是 `Array_Sort`，而是一个名为 `execArray_Sort` 的特殊函数。
    -   `ArrayParm` 是一个元数据标签，用于标识哪个参数是我们要操作的目标通配符数组。

2.  **实现 `DECLARE_FUNCTION` 和 `exec` 函数**:

    ```cpp
    // In the .cpp file
    DECLARE_FUNCTION(execArray_Sort)
    {
        // 1. 从堆栈(Stack)中读取参数
        Stack.Step(Stack.Object, NULL); // 跳过`self`参数

        // 2. 获取通配符数组的地址和其FArrayProperty
        void* ArrayAddr = Stack.MostRecentPropertyAddress;
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(Stack.MostRecentProperty);

        // 3. 获取其他参数
        P_GET_PROPERTY(FNameProperty, PropertyName);
        P_FINISH; // 完成参数读取

        // 4. 调用通用的、基于void*的排序实现
        P_NATIVE_BEGIN
        GenericArray_Sort(ArrayAddr, ArrayProp, PropertyName);
        P_NATIVE_END
    }
    ```

3.  **实现泛型排序逻辑**:

    ```cpp
    void UMyBlueprintFunctionLibrary::GenericArray_Sort(void* ArrayAddr, FArrayProperty* ArrayProp, FName PropertyName)
    {
        if (!ArrayAddr || !ArrayProp) return;

        // FScriptArrayHelper 是一个用于操作底层数组内存的工具
        FScriptArrayHelper ArrayHelper(ArrayProp, ArrayAddr);

        // ... 在这里，你可以使用ArrayHelper.GetRawPtr(Index)获取元素地址
        // ... 结合反射系统，找到PropertyName对应的FProperty
        // ... 然后对元素进行比较和交换 (ArrayHelper.Swap)
    }
    ```

### 4.4 核心技术3：通过Slate自定义引脚UI

为了显示动态属性的下拉菜单，你需要创建一个自定义的 `SGraphPin` Widget。

1.  **创建`FGraphPanelPinFactory`**: 这是一个工厂类，用于告诉蓝图编辑器在何种情况下使用你的自定义Pin Widget。

    ```cpp
    class FMyPinFactory : public FGraphPanelPinFactory
    {
        virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
        {
            // 如果这个Pin属于我们的排序节点，并且是属性名称引脚
            if (InPin->GetOwningNode()->IsA<UMyArraySortNode>() && InPin->PinName == TEXT("PropertyName"))
            {
                // 创建并返回我们的自定义Slate Widget
                return SNew(SMyPropertyNamePin, InPin);
            }
            return nullptr;
        }
    };
    ```

2.  **创建自定义`SGraphPin`**:

    ```cpp
    // SMyPropertyNamePin.h
    class SMyPropertyNamePin : public SGraphPin
    {
    public:
        SLATE_BEGIN_ARGS(SMyPropertyNamePin) {}
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

    protected:
        virtual TSharedPtr<SWidget> GetDefaultValueWidget() override;
        // ...
    };
    ```

    -   在 `GetDefaultValueWidget` 中，你可以返回一个 `SComboBox<TSharedPtr<FName>>`，它的选项列表数据源自上面实现的 `GetSortablePropertyNames()` 函数。

3.  **注册工厂**: 在你的插件或编辑器模块的 `StartupModule` 中，注册这个Pin工厂。

    ```cpp
    FEdGraphUtilities::RegisterVisualPinFactory(MakeShareable(new FMyPinFactory()));
    ```

---

## 第五部分：UI/UX与最佳实践

### 5.1 提升用户体验 (UI/UX)

-   **清晰的命名和分类**:
    -   `GetNodeTitle`: 提供简洁明了的节点标题。
    -   `GetMenuCategory`: 将你的节点放入合适的右键菜单分类中，如 `"MyNodes|Array"`。
    -   `GetTooltipText`: 提供详细的节点和引脚说明。
-   **视觉提示**:
    -   `GetCornerIcon`: 为节点右上角设置一个图标，增加辨识度。
    -   使用 `CompilerContext.MessageLog` 在 `ExpandNode` 中提供 `Error`, `Warning`, `Note` 信息，它们会以不同颜色显示在节点上。
-   **智能默认值**: 为引脚提供合理的默认值，让用户开箱即用。
-   **多语言支持**: 所有面向用户的字符串（标题、提示、错误信息）都应使用 `LOCTEXT` 进行本地化。

### 5.2 性能与安全

-   **空指针检查**: 在访问任何指针之前（尤其是 `UEdGraphPin*`），务必进行有效性检查。在 `ExpandNode` 中，使用 `ensure` 或 `check`；在运行时函数中，使用 `if (!Ptr) return;`。
-   **事务管理**: 任何会修改节点结构的操作（特别是 `ReconstructNode`）都应包裹在 `FScopedTransaction` 中，以确保撤销/重做功能正常工作。
-   **性能考量**: `CustomThunk` 和反射虽然强大，但有性能开销。确保它们只在必要时（编辑器中或一次性编译时）使用。运行时的排序算法应尽可能高效。对于大型数组操作，考虑使用 `TArrayView` 避免数据拷贝。

### 5.3 版本兼容与模块化

-   **模块依赖**: 将编辑器专用代码（K2Node, Slate UI）和运行时代码（BlueprintFunctionLibrary）分离到不同的模块中，或至少使用 `#if WITH_EDITOR` 宏进行隔离。在 `.Build.cs` 文件中，只在 `Target.bBuildEditor` 为 `true` 时才添加 `"UnrealEd"`, `"BlueprintGraph"` 等编辑器模块依赖。
-   **版本兼容**: 如果你更新了节点，改变了引脚的名称，应重写 `GetRedirectPinNames()` 来将旧名称映射到新名称，这样用户的旧蓝图就不会损坏。对于类或属性的重命名，使用 `FCoreRedirects`。

---

## 第九部分：核心原则：编辑器与运行时模块分离

在开发任何包含编辑器功能（如自定义蓝图节点 `UK2Node`）的插件时，最重要的一个原则是：**严格分离编辑器代码和运行时代码**。违反这个原则是导致插件打包失败最常见的原因。

### 9.1 为什么需要分离？

- **打包环境的差异**：
  - **编辑器环境**：编译时，所有模块（Runtime, Editor, UncookedOnly 等）都会被加载和链接。因此，在 `Runtime` 模块中引用 `UK2Node` 等编辑器类在编辑器里可以正常编译和运行。
  - **打包环境**：当为游戏打包 `Shipping` 或 `Development` 版本时（例如 `UnrealGame` 目标），**只有 `Runtime` 类型的模块会被编译和链接**。编辑器相关的模块（`UnrealEd`, `BlueprintGraph` 等）完全不存在。

- **常见的打包错误**：
  1. **找不到父类 (`Unable to find parent class`)**: 如果你的 `UK2Node_MyNode` 继承自 `UK2Node` 但被放在 `Runtime` 模块中，打包时编译器会因为找不到 `UK2Node` 的定义而报错。
  2. **链接错误 (`LNK2019: unresolved external symbol`)**: 如果你的 `Runtime` 模块代码调用了任何编辑器API，或者即使被 `#if WITH_EDITOR` 包裹，但其 `UCLASS` 的 `.gen.cpp` 文件被链接，都会在打包时产生链接错误。

### 9.2 如何正确分离模块？

最佳实践是为你的插件创建至少两个模块：一个用于运行时，一个用于编辑器。

- `MyPlugin` (Type: `Runtime`): 存放所有游戏运行时需要的功能和类。例如 `UFUNCTION` 库、`UActorComponent` 等。这个模块**绝对不能**包含任何编辑器相关的头文件或代码。
- `MyPluginEditor` (Type: `UncookedOnly` 或 `Editor`): 存放所有仅在编辑器中使用的功能。
  - `UK2Node` 节点定义。
  - `SWidget` 自定义UI。
  - `FAssetTypeActions_Base` 资产操作。
  - `FGraphPanelPinFactory` 自定义图钉。
  - 所有需要 `include "UnrealEd.h"` 或其他编辑器头文件的代码。

**注意**: 对于蓝图节点，模块类型推荐使用 `UncookedOnly` 而不是 `Editor`，可以避免在蓝图中出现"来自纯编辑器模块"的警告。

### 9.3 实践步骤示例

假设我们要创建一个排序节点 `UK2Node_SmartSort`。

1. **创建 `Sort` (Runtime) 模块**:
   - `Sort.Build.cs`: 这是一个 `Runtime` 模块，不应包含任何编辑器依赖。
   - `USortBPLibrary.h/.cpp`: 可以在这里定义实际的排序函数，供 `K2Node` 调用。

2. **创建 `SortEditor` (UncookedOnly) 模块**:
   - 在 `.uplugin` 文件中添加新模块的声明:
     ```json
     {
         "Name": "SortEditor",
         "Type": "UncookedOnly",
         "LoadingPhase": "Default"
     }
     ```
   - **`SortEditor.Build.cs`**:
     ```csharp
     PublicDependencyModuleNames.AddRange(new string[] { "Core", "Sort" }); // 依赖运行时模块
     PrivateDependencyModuleNames.AddRange(
         new string[] {
             "CoreUObject",
             "Engine",
             "Slate",
             "SlateCore",
             "UnrealEd",        // 编辑器核心
             "BlueprintGraph",  // K2Node需要
             "GraphEditor",     // 图表编辑器
             // ... 其他编辑器依赖
         }
     );
     ```
   - **`K2Node_SmartSort.h/.cpp`**: 将这个文件放在 `SortEditor` 模块的 `Private` 或 `Public` 目录中。
     - **重要**: 确保更新API宏：`class SORTEDITOR_API UK2Node_SmartSort : public UK2Node`。

3. **处理 `#if WITH_EDITOR` 的陷阱**:
   - **问题**: 即使你在 `.cpp` 中用 `#if WITH_EDITOR` 包裹了所有实现，但只要 `.h` 文件中的 `UCLASS()` 被UHT处理，它就会生成一个 `.gen.cpp` 文件。在打包时，这个 `.gen.cpp` 文件仍然会被编译，但找不到实现，导致链接失败。
   - **解决方案**: **不要依赖 `#if WITH_EDITOR` 来混合代码**。将编辑器相关的头文件和源文件**整个**移动到编辑器模块中，这是最干净、最可靠的方法。

通过这种方式，可以确保插件在编辑器中功能完整，并且在打包时能够顺利剔除所有不必要的编辑器代码，从而成功打包。

