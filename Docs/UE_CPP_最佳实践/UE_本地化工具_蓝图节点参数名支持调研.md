# UE 本地化工具对 C++ 蓝图节点参数名支持情况调研

> 调研日期：2025-01-19
> 适用版本：UE 5.3-5.7

## 核心结论

| 问题 | 答案 |
|------|------|
| Localization Dashboard 能否采集 C++ 元数据？ | **可以**（通过 Gather from Meta Data） |
| 采集的翻译能否在编辑器生效？ | **不能**（元数据是编译时常量） |
| 节点/引脚名称能否本地化？ | **可以**（需要使用 LOCTEXT 并配置 EditorSettings） |
| UPARAM DisplayName 能否本地化？ | **不能**（不是 FText 类型） |

## 详细分析

### 1. Localization Dashboard 的 "Gather from Meta Data" 功能

Localization Dashboard 提供了采集 C++ 元数据的功能，可以配置：
- **Meta Data Key**: 指定要采集的元数据键（如 `DisplayName`）
- **Text Namespace**: 指定输出的本地化命名空间
- **Text Key Pattern**: 定义生成本地化 Key 的模式

**关键问题**：虽然可以采集和翻译，但翻译后的文本**无法在编辑器中生效**。

原因是 `meta=(DisplayName="xxx")` 是**编译时常量**，存储在反射系统中，不是 `FText` 类型，因此不参与运行时的本地化查找。

### 2. 编辑器设置支持

UE 编辑器提供了以下设置（在 `EditorSettings.ini` 中）：

```ini
[Internationalization]
ShouldUseLocalizedPropertyNames=True      ; 属性名本地化
ShouldUseLocalizedNodeAndPinNames=True    ; 蓝图节点和引脚名本地化
ShouldUseLocalizedNumericInput=True       ; 数值输入本地化
```

但这依赖于**引擎内置的本地化资源**（Engine Targets），对于插件自定义内容效果有限。

### 3. UPARAM 和 UFUNCTION 元数据的限制

```cpp
// 这种写法的 DisplayName 无法被本地化系统运行时替换
UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Pool Size"))
static int32 GetPoolSize(UPARAM(DisplayName="Actor Class") TSubclassOf<AActor> ActorClass);
```

原因：
- `DisplayName` 是 UHT（Unreal Header Tool）在编译时处理的静态字符串
- 存储在 `UField` 的元数据 TMap 中，类型为 `FString` 而非 `FText`
- 运行时无法通过本地化系统进行动态替换

## 正确的实现方式

### 方式一：K2Node 中使用 LOCTEXT（推荐）

对于自定义 K2Node，可以在代码中使用 `LOCTEXT` 宏实现完整的本地化支持：

```cpp
#define LOCTEXT_NAMESPACE "MyK2Node"

FText UK2Node_MyCustomNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("NodeTitle", "My Custom Node");
}

FText UK2Node_MyCustomNode::GetTooltipText() const
{
    return LOCTEXT("NodeTooltip", "This is the detailed description of the node");
}

FText UK2Node_MyCustomNode::GetMenuCategory() const
{
    return LOCTEXT("NodeCategory", "XTools|Custom");
}

FText UK2Node_MyCustomNode::GetPinDisplayName(const UEdGraphPin* Pin) const
{
    if (Pin->PinName == TEXT("InputValue"))
    {
        return LOCTEXT("InputValuePin", "Input Value");
    }
    return Super::GetPinDisplayName(Pin);
}

#undef LOCTEXT_NAMESPACE
```

### 方式二：使用 NSLOCTEXT（无需定义命名空间）

```cpp
FText UK2Node_MyCustomNode::GetPinDisplayName(const UEdGraphPin* Pin) const
{
    if (Pin->PinName == TEXT("InputValue"))
    {
        return NSLOCTEXT("MyK2Node", "InputValuePin", "Input Value");
    }
    return Super::GetPinDisplayName(Pin);
}
```

### 方式三：使用 String Table（集中管理）

1. 创建 String Table 资产（Content Browser -> Miscellaneous -> String Table）
2. 在代码中引用：

```cpp
FText::FromStringTable(TEXT("/Game/Localization/MyStringTable"), TEXT("NodeTitle_Key"));
```

## 对 XTools 插件的建议

由于 XTools 已经采用**中文优先**策略，当前做法是最实用的：

```cpp
UFUNCTION(BlueprintCallable,
    meta = (DisplayName = "获取对象池大小", ToolTip = "返回对象池中的对象数量"))
static int32 GetPoolSize(UPARAM(DisplayName="对象类") TSubclassOf<AActor> ActorClass);
```

### 多语言支持方案对比

| 方案 | 适用场景 | 开发成本 | 维护成本 |
|------|----------|----------|----------|
| **自定义 K2Node + LOCTEXT** | 关键节点需要多语言 | 高 | 中 |
| **直接使用中文元数据** | 目标用户为中文用户 | 低 | 低 |
| **为每个语言打包不同插件** | 商业插件多语言发布 | 中 | 高 |
| **String Table 集中管理** | 大量文本需要翻译 | 中 | 中 |

### 推荐策略

1. **普通蓝图函数库**：保持当前的中文元数据方式，简单直接
2. **关键自定义节点（K2Node）**：如需多语言支持，使用 LOCTEXT 实现
3. **商业发布**：如需多语言版本，考虑为每个语言版本单独打包

## 参考资料

- Epic Games Japan 官方演示文档：Localization Dashboard 配置
- UE 论坛讨论：Gather from Meta Data 功能限制（2024年1月）
- UE 源码：`UField::GetMetaData()` 实现

## 总结

**UPARAM/UFUNCTION 的 DisplayName 元数据目前无法通过 UE 本地化系统实现运行时切换**。

如果需要完整的多语言支持，必须使用自定义 K2Node 并在代码中使用 `LOCTEXT`/`NSLOCTEXT` 宏来包装所有显示文本。

对于当前 XTools 的中文优先策略，保持现有做法是最合理的选择。
