# GitHub项目K2Node_ArraySort实现分析

> 基于 https://github.com/xusjtuer/NoteUE4/tree/master/AwesomeBlueprints 项目的源码分析

## 📋 项目概述

这个GitHub项目实现了一个强大的K2Node_ArraySort节点，**可以传入结构体和对象数组，动态获取其属性名称，并根据选择的属性进行排序**。这是一个非常优秀的K2Node实现范例。

---

## 🏗️ 核心架构设计

### 1. K2Node层面 - 编辑器节点

```cpp
class UK2Node_ArraySort : public UK2Node
{
    // 核心引脚访问器
    UEdGraphPin* GetTargetArrayPin() const { return Pins[2]; }      // 目标数组
    UEdGraphPin* GetPropertyNamePin() const { return Pins[3]; }     // 属性名称
    UEdGraphPin* GetAscendingPinPin() const { return Pins[4]; }     // 升序/降序

    // 关键功能：获取可排序的属性名称
    TArray<FString> GetPropertyNames();

    // 动态更新属性选择
    void SetDefaultValueOfPropertyNamePin();
    void RefreshPropertyNameOptions();

    // 类型传播
    void PropagatePinType(FEdGraphPinType& InType);
};
```

### 2. 运行时库函数

```cpp
class UAwesomeFunctionLibrary : public UBlueprintFunctionLibrary
{
    // 通用数组排序函数（支持任意类型）
    UFUNCTION(BlueprintCallable, CustomThunk, meta = (ArrayParm = "TargetArray"))
    static void Array_SortV2(const TArray<int32>& TargetArray, FName PropertyName, bool bAscending);

    // 自定义Thunk函数处理任意类型
    DECLARE_FUNCTION(execArray_SortV2);

    // 核心实现函数
    static void GenericArray_SortV2(void* TargetArray, FArrayProperty* ArrayProp, FName PropertyName, bool bAscending);

private:
    // 快速排序实现
    static void QuickSort_RecursiveByProperty(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, int32 Low, int32 High, bool bAscending);
    static int32 QuickSort_PartitionByProperty(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, int32 Low, int32 High, bool bAscending);
    static bool GenericComparePropertyValue(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, int32 j, int32 High, bool bAscending);
};
```

---

## 🔍 属性发现机制

### 动态属性枚举实现

```cpp
TArray<FString> UK2Node_ArraySort::GetPropertyNames()
{
    TArray<FString> PropertyNames;
    UEdGraphPin* TargetArrayPin = GetTargetArrayPin();

    if (TargetArrayPin && TargetArrayPin->LinkedTo.Num() > 0)
    {
        UEdGraphPin* LinkArrayPin = TargetArrayPin->LinkedTo[0];

        // 处理结构体数组
        if (UScriptStruct* InnerStruct = Cast<UScriptStruct>(LinkArrayPin->PinType.PinSubCategoryObject.Get()))
        {
            for (TFieldIterator<FProperty> It(InnerStruct); It; ++It)
            {
                const FProperty* BaseProp = *It;
                // 只添加可排序的属性类型
                if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(BaseProp))
                {
                    PropertyNames.Add(BaseProp->GetAuthoredName());
                }
                else if (const FNumericProperty* NumProp = CastField<FNumericProperty>(BaseProp))
                {
                    PropertyNames.Add(BaseProp->GetAuthoredName());
                }
            }
        }
        // 处理对象数组
        else if (UClass* InnerObjClass = Cast<UClass>(LinkArrayPin->PinType.PinSubCategoryObject.Get()))
        {
            for (TFieldIterator<FProperty> It(InnerObjClass); It; ++It)
            {
                const FProperty* BaseProp = *It;
                // 同样只添加可排序的属性
                if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(BaseProp))
                {
                    PropertyNames.Add(BaseProp->GetAuthoredName());
                }
                else if (const FNumericProperty* NumProp = CastField<FNumericProperty>(BaseProp))
                {
                    PropertyNames.Add(BaseProp->GetAuthoredName());
                }
            }
        }
    }

    return PropertyNames;
}
```

### 属性查找辅助函数

```cpp
namespace AwesomeFunctionLibraryHelper
{
    template<typename T>
    T* FindFProperty(const UStruct* Owner, FName FieldName)
    {
        // 检查字段名是否有效
        if (FieldName.IsNone()) {
            return nullptr;
        }

        // 通过比较FName（整数）而不是字符串来搜索
        for (TFieldIterator<T> It(Owner); It; ++It)
        {
            if ((It->GetFName() == FieldName) || (It->GetAuthoredName() == FieldName.ToString()))
            {
                return *It;
            }
        }

        return nullptr;
    }
}
```

---

## ⚙️ 运行时属性访问实现

### 核心排序逻辑

```cpp
void UAwesomeFunctionLibrary::GenericArray_SortV2(void* TargetArray, FArrayProperty* ArrayProp, FName PropertyName, bool bAscending)
{
    if (!TargetArray) return;

    FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
    FProperty* SortProp = nullptr;

    if (ArrayHelper.Num() < 2) return;

    // 根据数组元素类型查找排序属性
    if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(ArrayProp->Inner))
    {
        // 对象数组：在类中查找属性
        SortProp = AwesomeFunctionLibraryHelper::FindFProperty<FProperty>(ObjectProp->PropertyClass, PropertyName);
    }
    else if (const FStructProperty* StructProp = CastField<FStructProperty>(ArrayProp->Inner))
    {
        // 结构体数组：在结构体中查找属性
        SortProp = AwesomeFunctionLibraryHelper::FindFProperty<FProperty>(StructProp->Struct, PropertyName);
    }
    else
    {
        // 基础类型数组：直接使用元素属性
        SortProp = ArrayProp->Inner;
    }

    if (SortProp)
    {
        // 使用快速排序算法
        QuickSort_RecursiveByProperty(ArrayHelper, ArrayProp->Inner, SortProp, 0, ArrayHelper.Num() - 1, bAscending);
    }
}
```

### 属性值比较实现

```cpp
bool UAwesomeFunctionLibrary::GenericComparePropertyValue(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, int32 j, int32 High, bool bAscending)
{
    void* LeftValueAddr = nullptr;
    void* RightValueAddr = nullptr;

    // 获取属性值地址
    if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(InnerProp))
    {
        // 对象属性：先获取对象，再获取属性值
        UObject* LeftObject = ObjectProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(j));
        UObject* RightObject = ObjectProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(High));
        LeftValueAddr = SortProp->ContainerPtrToValuePtr<void>(LeftObject);
        RightValueAddr = SortProp->ContainerPtrToValuePtr<void>(RightObject);
    }
    else
    {
        // 结构体属性：直接获取属性值
        LeftValueAddr = SortProp->ContainerPtrToValuePtr<void>(ArrayHelper.GetRawPtr(j));
        RightValueAddr = SortProp->ContainerPtrToValuePtr<void>(ArrayHelper.GetRawPtr(High));
    }

    // 根据属性类型进行比较
    bool bResult = false;
    if (const FNumericProperty* NumericProp = CastField<FNumericProperty>(SortProp))
    {
        if (NumericProp->IsFloatingPoint())
        {
            bResult = NumericProp->GetFloatingPointPropertyValue(LeftValueAddr) <
                     NumericProp->GetFloatingPointPropertyValue(RightValueAddr);
        }
        else if (NumericProp->IsInteger())
        {
            bResult = NumericProp->GetSignedIntPropertyValue(LeftValueAddr) <
                     NumericProp->GetSignedIntPropertyValue(RightValueAddr);
        }
    }
    else if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(SortProp))
    {
        bResult = !BoolProp->GetPropertyValue(LeftValueAddr) && BoolProp->GetPropertyValue(RightValueAddr);
    }
    else if (const FNameProperty* NameProp = CastField<FNameProperty>(SortProp))
    {
        bResult = NameProp->GetPropertyValue(LeftValueAddr).ToString() <
                 NameProp->GetPropertyValue(RightValueAddr).ToString();
    }
    else if (const FStrProperty* StringProp = CastField<FStrProperty>(SortProp))
    {
        bResult = StringProp->GetPropertyValue(LeftValueAddr) < StringProp->GetPropertyValue(RightValueAddr);
    }
    else if (const FTextProperty* TextProp = CastField<FTextProperty>(SortProp))
    {
        bResult = TextProp->GetPropertyValue(LeftValueAddr).ToString() <
                 TextProp->GetPropertyValue(RightValueAddr).ToString();
    }

    return bResult == bAscending;
}
```

---

## 🎨 用户界面实现

### 自定义引脚UI

```cpp
// 自定义引脚工厂
class FK2BlueprintGraphPanelPinFactory : public FGraphPanelPinFactory
{
    virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
    {
        if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
        {
            if (UK2Node_ArraySort* ArraySortNode = Cast<UK2Node_ArraySort>(InPin->GetOuter()))
            {
                UEdGraphPin* ArrayPin = ArraySortNode->GetTargetArrayPin();
                if (ArrayPin)
                {
                    if (ArrayPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
                        ArrayPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
                    {
                        // 创建属性选择下拉菜单
                        TArray<FString> PropertyNames = ArraySortNode->GetPropertyNames();
                        return SNew(SGraphPinArraySortPropertyName, ArraySortNode->GetPropertyNamePin(), PropertyNames);
                    }
                    else
                    {
                        // 基础类型，使用默认引脚
                        UEdGraphPin* PropertyNamePin = ArraySortNode->GetPropertyNamePin();
                        GetDefault<UEdGraphSchema_K2>()->SetPinAutogeneratedDefaultValueBasedOnType(PropertyNamePin);
                    }
                }
            }
        }
        return nullptr;
    }
};
```

### 属性选择下拉菜单

```cpp
class SGraphPinArraySortPropertyName : public SGraphPinNameList
{
public:
    SLATE_BEGIN_ARGS(SGraphPinArraySortPropertyName) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const TArray<FString>& InNameList);

protected:
    // 刷新属性名称列表
    void RefreshNameList(const TArray<FString>& InNameList);

private:
    // 属性名称列表
    TArray<TSharedPtr<FName>> NameList;
};

void SGraphPinArraySortPropertyName::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const TArray<FString>& InNameList)
{
    RefreshNameList(InNameList);
    SGraphPinNameList::Construct(SGraphPinNameList::FArguments(), InGraphPinObj, NameList);
}

void SGraphPinArraySortPropertyName::RefreshNameList(const TArray<FString>& InNameList)
{
    NameList.Empty();
    for (const FString& PropName : InNameList)
    {
        TSharedPtr<FName> NamePropertyItem = MakeShareable(new FName(PropName));
        NameList.Add(NamePropertyItem);
    }
}
```

---

## 🔄 编译时展开实现

### ExpandNode函数

```cpp
void UK2Node_ArraySort::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    UEdGraphPin* OriginalArrayPin = GetTargetArrayPin();
    if (OriginalArrayPin == nullptr || OriginalArrayPin->LinkedTo.Num() == 0)
    {
        CompilerContext.MessageLog.Error(*LOCTEXT("K2Node_ArraySort_Error", "ArraySort must have a Array specified.").ToString(), this);
        BreakAllNodeLinks();
        return;
    }

    // 创建目标函数调用节点
    const FName DstFunctionName = GET_FUNCTION_NAME_CHECKED(UAwesomeFunctionLibrary, Array_SortV2);
    UK2Node_CallFunction* DstArraySortFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    DstArraySortFunction->FunctionReference.SetExternalMember(DstFunctionName, UAwesomeFunctionLibrary::StaticClass());
    DstArraySortFunction->AllocateDefaultPins();

    // 设置数组引脚类型
    const FEdGraphPinType& OriginalArrayPinType = OriginalArrayPin->PinType;
    UEdGraphPin* DstArrayPin = DstArraySortFunction->FindPinChecked(ArraySortHelper::TargetArrayPinName);
    FEdGraphPinType& DstArrayPinType = DstArrayPin->PinType;
    DstArrayPinType.PinCategory = OriginalArrayPinType.PinCategory;
    DstArrayPinType.PinSubCategory = OriginalArrayPinType.PinSubCategory;
    DstArrayPinType.PinSubCategoryObject = OriginalArrayPinType.PinSubCategoryObject;

    // 连接引脚
    CompilerContext.MovePinLinksToIntermediate(*OriginalArrayPin, *DstArrayPin);

    UEdGraphPin* DstPropertyNamePin = DstArraySortFunction->FindPinChecked(ArraySortHelper::PropertyNamePinName);
    CompilerContext.MovePinLinksToIntermediate(*GetPropertyNamePin(), *DstPropertyNamePin);

    UEdGraphPin* DstAscendingPin = DstArraySortFunction->FindPinChecked(ArraySortHelper::AscendingPinName);
    CompilerContext.MovePinLinksToIntermediate(*GetAscendingPinPin(), *DstAscendingPin);

    // 连接执行引脚
    CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *DstArraySortFunction->GetExecPin());
    UEdGraphPin* DstThenPin = DstArraySortFunction->GetThenPin();
    UEdGraphPin* OriginalThenPin = FindPinChecked(UEdGraphSchema_K2::PN_Then);
    CompilerContext.MovePinLinksToIntermediate(*OriginalThenPin, *DstThenPin);

    BreakAllNodeLinks();
}
```

---

## 🎯 关键技术特点总结

### 1. 反射系统的深度利用
- ✅ **TFieldIterator遍历** - 使用`TFieldIterator<FProperty>`遍历结构体/类的所有属性
- ✅ **FProperty系统** - 通过`FProperty`系统在运行时访问属性值
- ✅ **类型安全检查** - 支持多种基础数据类型的比较和验证
- ✅ **动态类型解析** - 运行时确定属性类型并执行相应的比较逻辑

### 2. CustomThunk函数的巧妙应用
- ✅ **任意类型支持** - 使用`CustomThunk`处理任意类型的数组
- ✅ **FScriptArrayHelper** - 通过`FScriptArrayHelper`操作数组元素
- ✅ **运行时类型检查** - 在执行时进行类型检查和转换
- ✅ **内存安全** - 正确处理指针和内存访问

### 3. 动态UI生成
- ✅ **实时属性发现** - 根据连接的数组类型动态生成属性选择列表
- ✅ **自定义引脚工厂** - 提供专门的下拉菜单UI组件
- ✅ **智能更新** - 实时更新可用属性选项
- ✅ **用户友好** - 直观的属性选择界面

### 4. 高效的排序算法
- ✅ **快速排序** - 使用经典的快速排序算法
- ✅ **原地排序** - 避免大对象拷贝，提高性能
- ✅ **双向支持** - 支持升序和降序排列
- ✅ **类型优化** - 针对不同属性类型优化比较逻辑

### 5. 架构设计优势
- ✅ **模块化设计** - K2Node、库函数、UI组件分离
- ✅ **扩展性强** - 易于添加新的属性类型支持
- ✅ **类型安全** - 编译时和运行时双重类型检查
- ✅ **性能优化** - 高效的内存访问和算法实现

---

## 💡 实现要点总结

### 核心创新点
1. **通用性** - 一个节点支持所有结构体和对象类型
2. **动态性** - 根据连接类型动态生成UI和功能
3. **类型安全** - 完善的类型检查和错误处理
4. **性能优化** - 高效的排序算法和内存管理

### 技术难点突破
1. **反射系统应用** - 深度利用UE的反射系统访问任意属性
2. **CustomThunk实现** - 处理任意类型的蓝图函数调用
3. **动态UI生成** - 根据类型信息动态创建用户界面
4. **内存安全管理** - 正确处理指针和内存访问

**总结：这个项目是一个非常优秀的K2Node实现范例，展示了如何创建真正通用、类型安全且用户友好的蓝图节点。它的设计思路和实现技巧对开发高质量的UE蓝图节点具有重要的参考价值！** 🚀
```