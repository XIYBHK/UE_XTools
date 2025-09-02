## UE 插件本地化指南

目标：代码最小改动，让文本/蓝图节点按语言切换；支持打包运行。

### 一、代码规范（LOCTEXT + FText）
- 对外文本统一用 FText；在 .cpp 用 LOCTEXT：
```cpp
#define LOCTEXT_NAMESPACE "L10NBPLibrary"
const FText Welcome = LOCTEXT("WelcomeMessage", "Welcome to the L10N Plugin!");
const FText Greeting = FText::Format(
  LOCTEXT("UserGreeting", "Hello {UserName}, have a great day!"),
  FFormatNamedArguments{{TEXT("UserName"), FText::FromString(UserName)}});
#undef LOCTEXT_NAMESPACE
```
- Key 稳定唯一；带参数用 FText::Format；避免把 FText 持久转 FString。

##### LOCTEXT 与 NSLOCTEXT 区别
- LOCTEXT：依赖当前文件用 `#define LOCTEXT_NAMESPACE "NS"` 指定命名空间。
- NSLOCTEXT：在调用处显式传入命名空间，适合头文件/静态初始化/跨文件场景。
```cpp
#define LOCTEXT_NAMESPACE "L10NBPLibrary"
FText A = LOCTEXT("WelcomeMessage", "Welcome to the L10N Plugin!");
#undef LOCTEXT_NAMESPACE

FText B = NSLOCTEXT("L10NBPLibrary", "WelcomeMessage", "Welcome to the L10N Plugin!");
```
> 两者都会被本地化仪表板收集；保持命名空间与Key稳定即可。

 

#### 蓝图库/导出宏（示例）
```cpp
UCLASS()
class L10N_API UL10NBPLibrary : public UBlueprintFunctionLibrary
{
public:
  GENERATED_BODY()

  UFUNCTION(BlueprintPure, Category="L10N Localization",
            meta=(DisplayName="Get Plugin Info"))
  static FText GetPluginInfo();
};
```
```cpp
// Public/L10N.h（Windows 简化）
// 无需手动定义 L10N_API，UBT 会为模块自动生成 <ModuleName>_API 宏。
// 直接在类/函数前使用 L10N_API 即可：class L10N_API UL10NBPLibrary ...
```
```csharp
// L10N.Build.cs
PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine"});
```

#### 蓝图节点本地化
- UFUNCTION/参数 DisplayName/ToolTip 保持英文，交给仪表板翻译：
```cpp
UFUNCTION(BlueprintCallable, Category="L10N Localization",
          meta=(DisplayName="Show Status Message"))
static FText ShowStatusMessage(int32 Count, const FText& ItemType);
```
> 编辑器偏好设置 → 区域与语言：勾选“使用本地化的蓝图节点和引脚命名”。

##### 高级技巧（可选）
- Keywords 也会被收集，可加中英文提升可搜索性：
```cpp
UFUNCTION(..., meta=(DisplayName="Execute Sample function",
                     Keywords="L10N sample test 示例 测试"))
```
- Category 可用管道符提供双语分类名：
```cpp
UFUNCTION(BlueprintCallable, Category="L10NTesting|L10N测试")
```
- ToolTip 会被收集并可翻译：
```cpp
UFUNCTION(..., meta=(DisplayName="Get Plugin Info",
                     ToolTip="Show plugin info"))
```

### 二、本地化仪表板流程
1. 窗口 → 开发者工具 → 本地化仪表板；创建目标 `L10N`（目录：`Plugins/L10N/Content/Localization/L10N`）。
2. 设置 NativeCulture=`en`，Cultures=`zh-Hans`；收集文本 → 翻译（或导出CSV/PO）→ 编译文本。
3. 生成 `zh-Hans/L10N.locres`（运行时使用）。

常见产物：`*.manifest/*.archive/*.locmeta`（编辑阶段） + `*.locres`（运行时）。

### 三、打包/运行时
运行时仅需 `.locres`。放在规范路径会在 Cook（资源预处理）与打包时自动包含，并在启动时自动加载：

- 项目：`<Project>/Content/Localization/<Target>/<Culture>/*.locres`
- 插件：`<Plugin>/Content/Localization/<Target>/<Culture>/*.locres`

若你的资源位于非标准位置，或被 Cook 规则排除，可在 `Build.cs` 显式声明（可选）：
```csharp
RuntimeDependencies.Add("$(PluginDir)/Content/Localization/L10N/**.locres");
```

加载与覆盖顺序（摘自 `TextLocalizationManager.cpp` 的合并策略）：
- 引擎 < 插件 < 项目（后加载者覆盖先加载者）。

### 四、UI 本地化示例（Slate / UMG）

#### 4.1 Slate（最小示例）
```cpp
#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "L10N_Slate"

class SL10NDemo : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SL10NDemo) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        ChildSlot
        [
            SNew(STextBlock)
            .Text( LOCTEXT("SlateHello", "Hello from Slate!") )
        ];
    }
};

// 需要跨文件/无命名空间时可用：
static const FText WindowTitle = NSLOCTEXT("L10NPlugin", "WindowTitle", "Localization Demo");

#undef LOCTEXT_NAMESPACE
```

要点：
- Slate 文本一律用 `FText`；在控件属性的 `.Text()` 传 `LOCTEXT/NSLOCTEXT`。
- 一样会被“本地化仪表板”收集，编译后随当前语言显示。

#### 4.2 UMG（最小示例）
```cpp
// 在 UUserWidget 子类中（C++）：
UCLASS()
class UL10NTitleWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Title; // 直接用 FText
};

// 赋值示例（C++创建时）：
UL10NTitleWidget* W = CreateWidget<UL10NTitleWidget>(GetWorld(), WidgetClass);
W->Title = LOCTEXT("UMGTitle", "Welcome to L10N!");

// 或在蓝图中将 TextBlock 的 Text 设置为本地化文本/或绑定到 Title
```

UMG 收集要点：
- 在仪表板“收集文本”时勾选“从资产中收集文本”（Gather Text from Assets）。
- UMG 里的 TextBlock/Text 属性会被自动收集；蓝图变量若用 `FText` 也会收集。

测试切换语言（运行时）
```cpp
FInternationalization::Get().SetCurrentCulture(TEXT("zh-Hans"));
```

### 五、快速排错
- 只见模板节点：关编辑器 → 删项目/插件 `Binaries/Intermediate` → 右键 .uproject 生成工程 → VS Rebuild → 重开。
- 文本未收集：确认使用 LOCTEXT、Gather 包含源目录；不要用 FString 拼接文本。
- 翻译错位：在仪表板修正条目后“编译文本”，或导出 CSV 修改再导入。

### 六、源码参考（UE5.3）
> 以下文件在 `Engine/Source/`

- 运行时核心与资源加载（Core/Internationalization）
  - `Runtime/Core/Public/Internationalization/Text.h`（FText、LOCTEXT/NSLOCTEXT 宏接口）
  - `Runtime/Core/Public/Internationalization/Internationalization.h`（FInternationalization）
  - `Runtime/Core/Public/Internationalization/TextLocalizationManager.h`
  - `Runtime/Core/Public/Internationalization/TextLocalizationResource.h`
  - `Runtime/Core/Private/Internationalization/Text.cpp`
  - `Runtime/Core/Private/Internationalization/Internationalization.cpp`
  - `Runtime/Core/Private/Internationalization/TextLocalizationManager.cpp`（加载/合并 .locres）
  - `Runtime/Core/Private/Internationalization/TextLocalizationResource.cpp`

- 文本收集/编译命令（Editor/UnrealEd/Commandlets）
  - `GatherTextFromSourceCommandlet.cpp`
  - `GatherTextFromMetadataCommandlet.cpp`
  - `GenerateGatherManifestCommandlet.cpp`
  - `GenerateGatherArchiveCommandlet.cpp`
  - `GenerateLocalizationResourcesCommandlet.cpp`

- 编辑器面板（Localization Dashboard）
  - `Engine/Plugins/Editor/LocalizationDashboard/Source/LocalizationDashboard/`





