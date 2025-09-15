LOCTEXT：## UE 插件本地化指南

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

### 七、Keywords 中英混合规范（推荐）

目的：在保持 DisplayName/Category/ToolTip 英文以利于统一本地化的前提下，通过 Keywords 同时保留中英文，提升蓝图节点的搜索体验。

- 基本原则
  - DisplayName、Category、ToolTip 使用英文；中文仅放入 Keywords。
  - Keywords 采用英文逗号分隔，英文在前、中文在后，例如：`"sort,distance,actor,index, 排序,距离,Actor,索引"`。
  - 每个节点建议 4–10 个关键词，覆盖核心动词/名词；避免堆砌同义词。
  - 参数 DisplayName 一律用英文；必要时将中文同义词加入 Keywords 便于搜索。
  - 高频中文关键词建议统一短词：排序/夹角/方位角/资产/命名/碰撞/凸包/复杂度/随机/PRD/流/权重/严格 等。

- 模板示例
```cpp
UFUNCTION(BlueprintCallable, Category="XTools|Sort|Actor",
          meta=(DisplayName="Sort Actors By Distance",
                Keywords="sort,distance,actor,index, 排序,距离,Actor,索引"))
static void SortActorsByDistance(const TArray<AActor*>& Actors,
                                 const FVector& Location,
                                 bool bAscending,
                                 bool b2DDistance,
                                 TArray<AActor*>& SortedActors,
                                 TArray<int32>& OriginalIndices,
                                 TArray<float>& SortedDistances);
```

- 推荐关键词词库（按模块）
  - XTools：actors/attach/parent/topmost（获取/附加/父级/顶层）、bezier/curve/point（贝塞尔/曲线/点）、geometry/grid/sample（静态网格/内部/点阵）。
  - RandomShuffles：random/sample/weighted/shuffle/strict/PRD/stream（随机/采样/加权/打乱/严格/流）。
  - Sort：sort/angle/distance/axis/azimuth（排序/夹角/距离/坐标/方位角）。
  - Collision：collision/convex/complexity/remove/batch（碰撞/凸包/复杂度/移除/批量）。
  - AssetNaming：asset/prefix/class/rename/validate（资产/前缀/类名/重命名/校验）。

- 质量检查清单
  - 每个对外 UFUNCTION 都设置了 Keywords（中英混合，英文在前）。
  - 参数 DisplayName 已统一英文；所需中文仅在 Keywords 补充。
  - 蓝图环境已勾选“使用本地化的蓝图节点和引脚命名”。

### 八、代码扫描辅助脚本（find_l10n_targets.py）

用途：快速定位源码中需要本地化处理/英文化的 UI 文本与对话内容，生成 JSON/Markdown 清单以便批量修复。

运行（示例）：
```bash
python find_l10n_targets.py \
  --root "X_Toolkit/Source" \
  --out-json "X_Toolkit/Docs/l10n_targets.json" \
  --out-md   "X_Toolkit/Docs/l10n_targets.md"
```

输出说明：
- JSON：数组，每项含 `file`、`line`、`kind`、`code`。
- Markdown：按文件分组列出命中行，适合人工浏览。

命中类型（kind）说明（节选）：
- `FTextFromString_TEXT`/`FTextFromString`/`WindowTitleFromString`：FText::FromString/窗口标题的直接字面量。
- `MessageDialog`/`NotificationInfo`：对话框/通知的调用点（见“误报抑制”规则）。
- `ChineseString_TEXT`/`ChineseString`：包含中文字符的字面量（非日志）。
- `LOCTEXT_ContainsChinese`/`NSLOCTEXT_ContainsChinese`：已包裹但内容仍为中文（需英文化再交给翻译）。
- `Missing_LOCTEXT_NAMESPACE`：文件使用了 LOCTEXT/NSLOCTEXT，但缺少 `#define/#undef LOCTEXT_NAMESPACE`。

误报抑制与忽略规则（已内置）：
- 跳过日志行：含 `UE_LOG(` 的行不检测。
- 跳过注释：`//` 行与 `/* ... */` 块注释内的内容不检测。
- 跳过蓝图元数据 Keywords：包含 `Keywords=` 或 `Keywords(` 的行不检测（按规范保留中英混合，供搜索）。
- MessageDialog/NotificationInfo 精细检测：仅当调用上下文存在未包裹的字面量字符串（如 `TEXT("...")` 或 `"..."`）且附近未出现 `LOCTEXT/NSLOCTEXT` 时才上报。

推荐工作流：
1) 运行脚本生成 `l10n_targets.json/md` 清单。
2) 按清单修复（英文化并用 `LOCTEXT/NSLOCTEXT` 包裹；日志保持英文常量，不包裹；Keywords 保持中英混合）。
3) 复扫直至清单为 0。
4) 打开 Localization Dashboard 执行 Gather Text/Compile，验证 UI 文本均被采集；编译并抽测 UI。

已知限制：
- 脚本针对源码扫描，不解析资产（UMG/蓝图）文件；资产文本请通过 Localization Dashboard 的资产收集功能处理。
- 对于通过 `FString` 拼接/多行组合的文本，检测基于启发式，建议优先改为 `LOCTEXT+FText::Format`。
 对于通过 `FString` 拼接/多行组合的文本，检测基于启发式，建议优先改为 `LOCTEXT+FText::Format`。

### 九、插件本地化配置步骤（X_ToolkitTest 示例）

以下以插件 `X_Toolkit` 的本地化目标 `X_ToolkitTest` 为例，给出从资源放置到插件加载与打包包含的最小化流程。

1) 放置运行时资源（.locres）
- 将 `Content/Localization/X_ToolkitTest` 目录（包含 `zh/X_ToolkitTest.locres` 以及编辑阶段产物）置于插件路径：
  - `<Plugin>/Content/Localization/X_ToolkitTest/`
  - 示例：`Plugins/X_Toolkit/Content/Localization/X_ToolkitTest/zh/X_ToolkitTest.locres`

2) 在插件描述文件声明目标与加载策略
- 编辑 `X_Toolkit.uplugin`，添加：
```json
"LocalizationTargets": [
  {
    "Name": "X_ToolkitTest",
    "LoadingPolicy": "Always"
  }
]
```
- 说明：
  - Name：对应已创建的本地化目标名称（此处为 `X_ToolkitTest`）。
  - LoadingPolicy：加载策略，可为 `Always`、`Game`、`Editor` 等。纯运行时优先 `Always`/`Game`；编辑器专用建议 `Editor` 以避免运行时占用内存。

3) 确保打包阶段包含 .locres（可选但推荐）
- 若担心自定义 Cook 规则导致遗漏，可在一个运行时模块（如 `XTools`）的 `Build.cs` 中添加：
```csharp
RuntimeDependencies.Add("$(PluginDir)/Content/Localization/X_ToolkitTest/**.locres");
```

4) 在本地化仪表板中编译与验证
- 打开 Localization Dashboard：选择目标 `X_ToolkitTest` → 采集文本（Gather）→ 翻译/导入 → 编译文本（Compile）。
- 启动编辑器并切换语言为中文（`zh-Hans`），验证界面文本是否切换；必要时运行一次打包/Cook，检查输出包中含：
  - `Plugins/X_Toolkit/Content/Localization/X_ToolkitTest/zh/X_ToolkitTest.locres`

提示：资源加载合并顺序为 引擎 < 插件 < 项目；项目同名目标可覆盖插件文本。若插件包含编辑器与运行时的多套文本，可分别创建目标并在 `LoadingPolicy` 上区分（`Editor` 与 `Game/Always`）。

重要注意（语言热切换与原生语言）
- 引擎在切换语言时会为“每一种语言”加载对应的 `.locres`，包括原生语言（例如 `en`）。如果只存在 `zh/X_ToolkitTest.locres` 而缺少 `en/X_ToolkitTest.locres`，则从中文切回英文时不会刷新，表现为必须重启编辑器才生效。
- 解决：为原生语言也生成 `.locres`。
  - 方式 A：在 Localization Dashboard 目标的编译设置中勾选“为原生语言生成 LocRes”（Generate LocRes for Native Culture）。
  - 方式 B：将 `en` 加入文化列表并执行 Compile。
  - 验证输出：`Plugins/X_Toolkit/Content/Localization/X_ToolkitTest/en/X_ToolkitTest.locres` 存在。
  - 之后在编辑器设置切换 `中文(简体)` ↔ `English` 应可无重启热切换（前提：界面文本使用 `FText`/`LOCTEXT` 且未在 UI 中长期缓存 `FString`）。

### 十、元数据收集设置（Blueprint 元数据的本地化）

要让 `UFUNCTION/UPROPERTY` 的元数据文本（如节点标题与 Tooltip）被采集与编译，请在 Localization Dashboard 的目标设置中开启“从元数据收集”并配置键名：

- 必选开关
  - Gather from Source（从源代码收集）
  - Gather from MetaData（从元数据收集）
- MetaData Keys（常用可采集键名）
  - DisplayName（函数/属性/参数显示名）
  - ToolTip（工具提示）
  - Category（分类名）
  - Keywords（关键字，便于搜索）
  - FriendlyName（如使用）
  - ShortToolTip/LongToolTip（如使用）
- Include/Exclude
  - Include Paths 覆盖：`Plugins/X_Toolkit/Source`
  - Exclude Paths 排除：`Binaries/`, `Intermediate/`

工作流
1) 勾选上述开关与键名 → 保存目标设置。
2) Gather → 翻译/导入 → Compile（确保 en 与 zh 均生成 `.locres`）。
3) 在编辑器切换语言验证蓝图节点标题/Tooltip 是否随语言热切换。

说明：UFUNCTION/UPROPERTY 中的 `meta=(DisplayName=..., ToolTip=...)` 为“元数据字符串”，不需要 `LOCTEXT` 包裹；只要开启“从元数据收集”且列入键名，便会被采集并进入 `.locres`。
