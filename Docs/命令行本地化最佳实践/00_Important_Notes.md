# UE 本地化重要注意事项

本文档集中了最容易遗漏或出错的关键配置点和注意事项。

**建议：首次配置前务必阅读本文档。**

---

## 1. 作用域配置（最重要！）

### 问题：Editor 插件采集不到文本

**原因**：Editor 类型插件的代码都在 `#if WITH_EDITOR` 宏内，默认会被跳过。

### 解决方法

检查 `.uplugin` 文件：

```json
"Modules": [{
    "Name": "MyPlugin",
    "Type": "Editor",    // ← 如果是 "Editor"
    "LoadingPhase": "PostEngineInit"
}]
```

**必须设置**：

```ini
[GatherTextStep0]
CommandletClass=GatherTextFromSource
ShouldGatherFromEditorOnlyData=True    # ← Editor 插件必须为 True
```

### UE 源码过滤逻辑

UE 在 `GatherTextFromSourceCommandlet.cpp:1159` 中的过滤逻辑：

```cpp
// 只有当不是编辑器专用数据，或者配置允许采集编辑器数据时才采集
if (!bIsEditorOnly || ShouldGatherFromEditorOnlyData)
{
    // 采集本地化文本
}
```

### 插件类型对照表

| 插件类型 | uplugin 中 Type | ShouldGatherFromEditorOnlyData | 说明 |
|----------|-----------------|--------------------------------|------|
| Editor | `"Type": "Editor"` | **必须为 `True`** | 工具栏、菜单、编辑器窗口 |
| Runtime | `"Type": "Runtime"` 或未指定 | `False` | 游戏运行时代码 |
| Developer | `"Type": "Developer"` | `True` | 开发工具 |

### 快速检查

如果采集后条目数为 0 或远少于预期：

```bash
# 1. 检查 uplugin 类型
# 2. 如果是 Editor 类型，确认配置了 ShouldGatherFromEditorOnlyData=True
# 3. 使用 -Preview 模式测试
```

---

## 2. 路径配置（最容易出错）

### 错误示例

```ini
# ❌ 绝对路径 - 不工作
SearchDirectoryPaths=D:/Projects/MyPlugin/Source

# ❌ 包含项目名前缀 - 不工作
SearchDirectoryPaths=MyProject/Plugins/MyPlugin/Source

# ❌ 使用反斜杠 - 可能不工作
SearchDirectoryPaths=Plugins\MyPlugin\Source
```

### 正确配置

```ini
# ✅ 相对路径，正斜杠
SearchDirectoryPaths=Plugins/MyPlugin/Source/Private
SearchDirectoryPaths=Plugins/MyPlugin/Source/Public

# ✅ 使用通配符
SearchDirectoryPaths=Plugins/MyPlugin/Source/*
```

### 路径基准点

所有路径相对于 **项目根目录**（`.uproject` 文件所在位置）：

```
D:/Projects/MyGame/              ← 基准点
├── MyGame.uproject
├── Config/
└── Plugins/
    └── MyPlugin/
        └── Source/              ← Plugins/MyPlugin/Source
```

### 路径标记

在 `SourcePath` 和 `DestinationPath` 中使用：

```ini
SourcePath=%LOCPROJECTROOT%Content/Localization/MyPlugin
DestinationPath=%LOCPROJECTROOT%Content/Localization/MyPlugin
```

| 标记 | 解析为 |
|------|--------|
| `%LOCPROJECTROOT%` | 项目根目录 |
| `%LOCENGINEROOT%` | 引擎根目录 |

---

## 3. 文本折叠模式（影响 PO 文件结构）

### 推荐配置

```ini
[CommonSettings]
LocalizedTextCollapseMode=IdenticalTextIdAndSource
POFormat=Unreal
```

### 模式对比

假设有两个条目：

```cpp
// 文件 A.cpp
NSLOCTEXT("MyNS", "Save", "Save")

// 文件 B.cpp
NSLOCTEXT("MyNS", "Save", "Save")
```

| 模式 | 结果 |
|------|------|
| `IdenticalTextIdAndSource` | 合并为 1 条 PO 条目 ✅ |
| `IdenticalNamespaceAndSource` | 合并为 1 条 PO 条目 |

如果源文本不同：

```cpp
NSLOCTEXT("MyNS", "Save", "Save File")
NSLOCTEXT("MyNS", "Save", "Save Project")
```

| 模式 | 结果 |
|------|------|
| `IdenticalTextIdAndSource` | 生成 2 条独立 PO 条目 ✅ |
| `IdenticalNamespaceAndSource` | 合并为 1 条（可能冲突）❌ |

### 错误配置的后果

使用 `IdenticalNamespaceAndSource` 可能导致：
- 同一键名、不同源文本的条目被强制合并
- 翻译时无法区分上下文
- 编译时出现冲突

---

## 4. 缓存控制（调试时重要）

### SkipGatherCache 配置

```ini
[GatherTextStep1]    # 资产采集
CommandletClass=GatherTextFromAssets
SkipGatherCache=False    # 默认：使用缓存，快速
                         # 调试时：设为 True，强制完整加载
```

### 何时设置为 True

- 怀疑缓存过时导致采集不完整
- 资产结构发生重大变化后
- 调试采集问题时

### 性能对比

| 值 | 速度 | 准确性 |
|----|------|--------|
| `False` | 快 | 通常足够 |
| `True` | 慢（约 2-5 倍） | 更准确 |

---

## 5. 编译验证（防止翻译错误）

### 推荐配置

```ini
[GatherTextStep0]
CommandletClass=GenerateTextLocalizationResource
bSkipSourceCheck=false              # 不跳过源检查
bValidateFormatPatterns=true        # 验证占位符 ✅
bValidateSafeWhitespace=true        # 验证空白字符 ✅
```

### 格式化占位符验证

```po
# ✅ 正确 - 保留占位符
msgid "Processing {0} items"
msgstr "正在处理 {0} 项"

# ❌ 错误 - 删除占位符（编译时会报错）
msgid "Processing {0} items"
msgstr "正在处理 项"
```

### 常见占位符

| 占位符 | 用途 |
|--------|------|
| `{0}`, `{1}` | FText::Format |
| `%s` | FString 格式化 |
| `%d`, `%f` | 数字格式化 |

---

## 6. 命名空间约定

### C++ 代码规范

```cpp
// ✅ 正确 - 使用命名空间
#define LOCTEXT_NAMESPACE "MyPlugin"
FText MyText = LOCTEXT("SaveKey", "Save Game");
#undef LOCTEXT_NAMESPACE

// ❌ 错误 - 不使用命名空间
FText MyText = FText::FromString("Save Game");

// ❌ 错误 - 使用通用命名空间（可能冲突）
#define LOCTEXT_NAMESPACE "Editor"
```

### 命名空间建议

| 插件名 | 推荐命名空间 | 示例 |
|--------|--------------|------|
| MyPlugin | `MyPlugin` | `LOCTEXT_NAMESPACE "MyPlugin"` |
| MyPlugin.SubModule | `MyPlugin.SubModule` | `LOCTEXT_NAMESPACE "MyPluginSubModule"` |
| FMyModule | `FMyModule` | 用于模块级通知 |

### 避免冲突

```cpp
// ❌ 可能冲突
#define LOCTEXT_NAMESPACE "UI"
#define LOCTEXT_NAMESPACE "Editor"
#define LOCTEXT_NAMESPACE "Game"

// ✅ 使用插件前缀
#define LOCTEXT_NAMESPACE "MyPluginUI"
#define LOCTEXT_NAMESPACE "MyPluginEditor"
```

---

## 7. 文件编码

### 必须使用 UTF-8

```po
# PO 文件头部必须声明
"Content-Type: text/plain; charset=UTF-8\n"
```

### 检查编码

```bash
# Linux/Mac
file -i Content/Localization/Game/en/Game.po

# Windows PowerShell
Get-Content Content\Localization\Game\en\Game.po | Select-String -Pattern "charset"
```

### 编辑器选择

| 编辑器 | 是否推荐 |
|--------|----------|
| Poedit | ✅ 推荐 |
| VS Code | ✅ 推荐 |
| Windows 记事本 | ❌ 不推荐（可能错误编码） |

---

## 8. 增量更新注意事项

### 常见错误

直接覆盖 PO 文件导致翻译丢失：

```bash
# ❌ 错误 - 覆盖导致翻译丢失
cp en/Game.po zh/Game.po

# ✅ 正确 - 使用备份和合并
cp zh/Game.po zh/Game.po.bak
# 重新采集导出...
# 使用 PO 编辑器合并翻译
```

### 正确的增量更新流程

1. 备份现有 PO 文件
2. 重新采集并导出
3. 使用 PO 编辑器合并翻译（不要直接覆盖）
4. 检查新增/修改/删除的条目
5. 编译资源

---

## 9. 配置文件依赖关系

### 必须按顺序执行

```
Gather → Export → 翻译 → Import → Compile
   ↓        ↓                   ↓
.manifest  .po              .archive
   ↓        ↓                   ↓
.archive  (翻译)            .locres
```

### 文件名必须一致

```ini
# Gather.ini
ManifestName=MyGame.manifest
ArchiveName=MyGame.archive

# Export.ini - 必须与 Gather.ini 一致
ManifestName=MyGame.manifest    # ✅ 必须相同
ArchiveName=MyGame.archive      # ✅ 必须相同

# 如果不一致，导出会失败或生成空文件
```

---

## 10. 版本控制 .gitignore

### 忽略生成的临时文件

```gitignore
# UE 本地化生成的临时文件
**/Content/Localization/*/*/*.locres
**/Content/Localization/*/*.csv
**/Content/Localization/*/*_Conflicts.txt
```

### 应提交的文件

```gitignore
# 提交这些文件
Config/Localization/*.ini
Content/Localization/*/*.manifest
Content/Localization/*/*.archive
Content/Localization/*/*/*.po
```

---

## 快速检查清单

配置本地化前，确保检查以下项目：

### 采集配置 (Gather.ini)

- [ ] Editor 插件：`ShouldGatherFromEditorOnlyData=True`
- [ ] Runtime 插件：`ShouldGatherFromEditorOnlyData=False`
- [ ] 路径使用相对路径，正斜杠
- [ ] 路径基准于项目根目录
- [ ] 文件过滤器包含 `*.cpp`, `*.h`, `*.hpp`

### 导出配置 (Export.ini)

- [ ] `LocalizedTextCollapseMode=IdenticalTextIdAndSource`
- [ ] `POFormat=Unreal`
- [ ] `ManifestName` 和 `ArchiveName` 与 Gather.ini 一致

### 编译配置 (Compile.ini)

- [ ] `bValidateFormatPatterns=true`
- [ ] `bValidateSafeWhitespace=true`
- [ ] `CulturesToGenerate` 包含所有目标语言

### PO 文件

- [ ] 文件编码为 UTF-8
- [ ] 保留了所有占位符 `{0}`, `%s` 等
- [ ] 文件在正确的语言目录（`zh/` 而非 `en/`）

### 代码规范

- [ ] UPROPERTY meta 使用字符串字面量，不使用 LOCTEXT
- [ ] 运行时 UI 文本使用 LOCTEXT 包裹
- [ ] 设置页 ToolTip 保持英文（已知限制）

---

## 常见错误速查

| 症状 | 可能原因 | 解决方法 |
|------|----------|----------|
| 采集 0 条 | 路径错误 | 使用相对路径 |
| 采集 0 条 | Editor 插件未配置 | 设置 `ShouldGatherFromEditorOnlyData=True` |
| PO 导出为空 | 未执行采集 | 先运行 Gather |
| PO 导出为空 | 文件名不匹配 | 检查 ManifestName/ArchiveName |
| 编译后翻译无效 | PO 文件位置错误 | 移动到正确语言目录 |
| 编译后翻译无效 | 语言未配置 | 添加到 `CulturesToGenerate` |
| 编译报错 | 占位符丢失 | 检查 PO 文件中的 `{0}` 等 |
| 命令崩溃 | 插件冲突 | 使用 `-DisablePlugins` |
| 编码错误 | 非 UTF-8 | 转换为 UTF-8 |
| UHT 编译错误 | UPROPERTY meta 使用 LOCTEXT | 使用字符串字面量（已知限制） |

---

## 调试技巧

### 预览模式

```bash
# 不写入文件，只检查配置
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -Preview -Unattended
```

### 查看日志

```bash
# 输出到文件
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -Unattended > log.txt 2>&1

# 查找关键信息
grep "Found.*entries" log.txt
grep "Error" log.txt
```

### 验证结果

```bash
# 检查统计报告
cat Content/Localization/Game/Game.csv

# 检查 PO 文件
head -50 Content/Localization/Game/en/Game.po
```

---

## 11. 已知限制：UPROPERTY Meta 无法使用 LOCTEXT

### 限制描述

UPROPERTY 的 meta 字段（如 ToolTip、DisplayName）**不能**使用 LOCTEXT/NSLOCTEXT 宏：

```cpp
// ❌ 错误 - UHT 无法解析，导致编译失败
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General",
    meta = (ToolTip = LOCTEXT("Key", "Tooltip text")))  // 编译错误
FString MyProperty;

// ✅ 正确 - 使用字符串字面量
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General",
    meta = (ToolTip = "Tooltip text"))
FString MyProperty;
```

### 错误示例

尝试使用 LOCTEXT 会产生以下错误：

```
Error: Found '(' when expecting ',' or ')' while parsing Variable specifiers
```

### 根本原因

这是 UE 的架构限制：

1. **UHT 运行时机**：Unreal Header Tool（UHT）在 C++ 预处理器**之前**运行
2. **宏无法展开**：UHT 无法展开 LOCTEXT/NSLOCTEXT 等宏
3. **语法解析限制**：UHT 的解析器只能识别字符串字面量

### 自动采集行为

虽然不能使用 LOCTEXT，但这些字符串**仍然会被自动采集**：

- `ToolTip` 字段内容会被采集
- `DisplayName` meta 字段内容会被采集
- 采集时使用属性名称作为键

### 工作区限制

**重要**：即使 ToolTip 被采集和翻译，**翻译在编辑器中不会生效**。

这是 UE 编辑器的已知行为：
- 编辑器中的 Details 面板使用原始字符串字面量
- 翻译资源对编辑器 UI 不生效（仅对游戏运行时 UI 有效）

### 推荐做法

1. **Editor 插件设置页**：保持 ToolTip 为英文
   ```cpp
   UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General",
       meta = (ToolTip = "The base endpoint to use for the Ollama API"))
   FString OllamaEndpoint;
   ```

2. **游戏运行时 UI**：使用 LOCTEXT
   ```cpp
   // 运行时显示的文本使用 LOCTEXT，可以被本地化
   #define LOCTEXT_NAMESPACE "MyPlugin"
   FText DisplayText = LOCTEXT("EndpointLabel", "Ollama Endpoint");
   #undef LOCTEXT_NAMESPACE
   ```

### 参考资源

- [论坛讨论：How can the display name of a property be localized](https://forums.unrealengine.com/t/how-can-the-display-name-of-a-property-be-localized/1636077)
- [论坛讨论：How to disable Localization gathering of ToolTips](https://forums.unrealengine.com/t/how-to-disable-localization-gathering-of-tooltips-and-properties/325636)

### 总结

| 场景 | 是否可以使用 LOCTEXT | 翻译是否生效 |
|------|---------------------|-------------|
| UPROPERTY meta (ToolTip/DisplayName) | ❌ 不能 | ❌ 不生效（即使被采集） |
| UPROPERTY 非字段 | ❌ 不能 | - |
| UFUNCTION 调用的 FText | ✅ 可以 | ✅ 生效（运行时） |
| UMG Widget 文本 | ✅ 可以 | ✅ 生效（运行时） |

---

*本文档随 UE 版本更新可能需要调整。基于 UE 5.3*
