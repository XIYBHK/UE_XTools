# UE 本地化常见问题

故障排除和常见问题解答。

---

## 采集相关问题

### Q: 采集后没有找到任何文本？

**检查项**：

1. **路径配置是否正确**
   ```ini
   # ✅ 正确 - 相对于项目根目录
   SearchDirectoryPaths=Source/Private
   SearchDirectoryPaths=Plugins/MyPlugin/Source/Private

   # ❌ 错误 - 绝对路径
   SearchDirectoryPaths=D:/Projects/Game/Source

   # ❌ 错误 - 包含项目名前缀
   SearchDirectoryPaths=Game/Source/Private
   ```

2. **文件过滤器是否匹配**
   ```ini
   FileNameFilters=*.cpp
   FileNameFilters=*.h
   FileNameFilters=*.hpp
   ```

3. **是否使用了本地化宏**
   ```cpp
   // ✅ 正确
   #define LOCTEXT_NAMESPACE "MyNamespace"
   FText MyText = LOCTEXT("MyKey", "Hello");
   #undef LOCTEXT_NAMESPACE

   // ❌ 错误 - 直接使用 FString
   FString MyText = "Hello";
   ```

4. **Editor 插件配置**
   ```ini
   # Editor 类型插件必须设置
   ShouldGatherFromEditorOnlyData=True
   ```

5. **查看日志**
   ```
   LogGatherTextFromSourceCommandlet: Warning: The GatherTextFromSource commandlet couldn't find any source files.
   ```

---

### Q: 采集到条目数量与预期不符？

**可能原因**：

1. **Editor-only 数据被过滤**
   - 检查 `ShouldGatherFromEditorOnlyData` 设置
   - Editor 插件应设为 `True`

2. **路径包含未采集的目录**
   ```ini
   # 确保所有需要的目录都已添加
   SearchDirectoryPaths=Source/Private
   SearchDirectoryPaths=Source/Public
   SearchDirectoryPaths=Plugins/MyPlugin/Source/Private
   ```

3. **排除规则过于宽泛**
   ```ini
   # ❌ 过于宽泛
   ExcludePathFilters=Source/Private/*

   # ✅ 更精确
   ExcludePathFilters=Source/Private/ThirdParty/*
   ```

---

### Q: 如何只采集特定类型的文本？

使用 `-GatherType` 参数：

```bash
# 只采集源码
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -GatherType=Source -Unattended

# 只采集资产
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -GatherType=Asset -Unattended
```

---

## 导出相关问题

### Q: PO 文件导出后是空的？

**检查项**：

1. **是否执行了采集步骤**
   - Export 依赖 Gather 生成的 `.manifest` 和 `.archive`

2. **文件名是否匹配**
   ```ini
   # Gather.ini
   ManifestName=Game.manifest
   ArchiveName=Game.archive

   # Export.ini - 必须与 Gather.ini 一致
   ManifestName=Game.manifest
   ArchiveName=Game.archive
   ```

3. **确认导出设置**
   ```ini
   [GatherTextStep0]
   CommandletClass=InternationalizationExport
   bExportLoc=true    # 必须为 true
   ```

---

### Q: 导出的 PO 文件中源代码位置注释缺失？

检查配置：

```ini
ShouldAddSourceLocationsAsComments=true    # 必须为 true
```

---

### Q: PO 文件格式不正确？

确保使用正确的 `LocalizedTextCollapseMode`：

```ini
# UE 4.15+ 推荐使用
LocalizedTextCollapseMode=IdenticalTextIdAndSource
POFormat=Unreal
```

---

## 编译相关问题

### Q: 编译后翻译没有生效？

**检查项**：

1. **PO 文件位置**
   ```
   # ✅ 正确
   Content/Localization/Game/zh/Game.po

   # ❌ 错误
   Content/Localization/Game/en/Game.po
   ```

2. **语言代码配置**
   ```ini
   [CommonSettings]
   CulturesToGenerate=zh    # 必须包含目标语言
   ```

3. **运行时文化设置**
   ```cpp
   // 必须在代码中设置当前文化
   FInternationalization::Get().SetCurrentCulture(TEXT("zh"));
   ```

4. **资源文件是否生成**
   ```
   # 检查 .locres 文件是否存在
   Content/Localization/Game/zh/Game.locres
   ```

5. **PO 文件编码**
   - 必须是 UTF-8 编码
   - 不能使用 UTF-16 或其他编码

---

### Q: 编译时提示格式化占位符错误？

**错误示例**：
```
Error: Translation '翻译文本' does not match format pattern '{0} items'
```

**解决方法**：保留占位符

```po
# ❌ 错误
msgid "{0} items"
msgstr "个项目"

# ✅ 正确
msgid "{0} items"
msgstr "{0} 个项目"
```

---

## 命令执行问题

### Q: 命令执行时崩溃？

**常见原因**：

1. **插件冲突**
   ```bash
   # 禁用可能冲突的插件
   -DisablePlugins=XTools
   -DisablePlugins=OtherPlugin
   ```

2. **项目配置损坏**
   - 尝试在编辑器中先测试本地化面板
   - 重新生成项目文件

3. **引擎版本不匹配**
   - 确保命令行使用的引擎版本与项目一致

---

### Q: 命令执行速度很慢？

**优化方法**：

1. **跳过着色器编译**
   ```bash
   -NoShaderCompile
   ```

2. **使用缓存**
   ```ini
   SkipGatherCache=False    # 使用缓存（默认）
   ```

3. **指定采集类型**
   ```bash
   -GatherType=Source    # 只采集源码，跳过资产
   ```

---

## 路径相关问题

### Q: 如何处理带空格的路径？

**方法 1**：使用引号

```bash
-config="Config/Localization/My Config.ini"
```

**方法 2**：避免空格

```
# 不推荐
Plugins/My Plugin/Source

# 推荐
Plugins/MyPlugin/Source
```

**方法 3**：使用通配符

```ini
SearchDirectoryPaths=Plugins/My*/Source/*
```

---

### Q: 相对路径基准是什么？

所有路径都相对于**项目根目录**（`.uproject` 文件所在位置）：

```
D:/Projects/Game/
├── Game.uproject          # 基准点
├── Config/Localization/   # Config/Localization/
├── Source/                # Source/
└── Plugins/               # Plugins/
```

---

## 版本控制问题

### Q: 哪些文件应该提交到版本控制？

| 提交 | 路径 |
|------|------|
| ✅ | `Config/Localization/*.ini` |
| ✅ | `Content/Localization/*/*.manifest` |
| ✅ | `Content/Localization/*/*.archive` |
| ✅ | `Content/Localization/*/*/*.po` |
| ❌ | `Content/Localization/*/*/*.locres` |
| ❌ | `Content/Localization/*/*.csv` |
| ❌ | `Content/Localization/*/*_Conflicts.txt` |

---

### Q: .gitignore 配置

```gitignore
# UE 本地化生成的临时文件
**/Content/Localization/*/*/*.locres
**/Content/Localization/*/*.csv
**/Content/Localization/*/*_Conflicts.txt
```

---

## 调试技巧

### Q: 如何预览而不写入文件？

使用预览模式：

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -Preview -Unattended
```

预览模式特点：
- 不写入任何文件
- 输出所有警告和错误
- 自动禁用 SCC 提交

---

### Q: 如何查看详细的采集日志？

1. **查看控制台输出**
   ```bash
   # Windows
   UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -Unattended > log.txt

   # Linux/Mac
   UnrealEditor-Cmd "Game.uproject" -run=GatherText -config="Gather.ini" -Unattended 2>&1 | tee log.txt
   ```

2. **查找关键信息**
   ```
   LogGatherTextFromSourceCommandlet: Processing files in: D:/Game/Source/Private/*
   LogGatherTextFromSourceCommandlet: Found 150 entries
   LogGatherTextCommandlet: Successfully gathered 150 words from 30 entries.
   ```

---

### Q: 如何验证采集结果？

检查生成的文件：

```bash
# 检查 manifest
cat Content/Localization/Game/Game.manifest | head -20

# 检查统计报告
cat Content/Localization/Game/Game.csv

# 检查冲突报告
cat Content/Localization/Game/Game_Conflicts.txt

# 检查 PO 文件
cat Content/Localization/Game/en/Game.po | head -30
```

**预期输出（CSV）**：
```csv
Date/Time,Word Count,en,zh
2026.01.21-10:30:07,124,124,0
```

---

## 增量更新问题

### Q: 重新采集后翻译丢失？

**原因**：直接覆盖了 PO 文件

**正确流程**：
1. 备份现有 PO 文件
2. 重新采集并导出
3. 使用 PO 编辑器合并翻译
4. 不要直接复制覆盖

---

### Q: 如何处理冲突报告？

**冲突报告示例**：
```
CONFLICT: Namespace "MyGame", Key "Save"
  Source 1: "Save File" (Source/A.cpp:10)
  Source 2: "Save Project" (Source/B.cpp:20)
```

**解决方法**：
1. 在源码中使用不同的键名
2. 或接受冲突，为同一键的不同上下文提供不同翻译

---

## 多语言问题

### Q: 如何添加新语言？

1. 更新所有配置文件的 `CulturesToGenerate`
2. 导出 PO 模板
3. 复制并翻译
4. 编译资源

---

### Q: 不同语言的文本长度差异很大导致 UI 问题？

**解决方法**：

1. **在源代码中使用更短的文本**
2. **调整 UI 布局以适应长文本**
3. **为不同语言使用不同的 UI 布局**
4. **在 PO 文件中添加注释说明长度限制**

---

## 性能问题

### Q: 大型项目采集很慢？

**优化建议**：

1. **分模块采集**
   ```ini
   # 为不同模块创建不同的配置文件
   ```

2. **并行处理**
   ```bash
   # 同时采集多个模块
   UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather_ModuleA.ini" -Unattended &
   UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather_ModuleB.ini" -Unattended &
   ```

3. **使用缓存**
   ```ini
   SkipGatherCache=False
   ```

4. **跳过资产采集**（如果不需要）
   ```bash
   -GatherType=Source
   ```

---

## 其他问题

### Q: 翻译中可以使用变量吗？

可以，使用 FText::Format：

```cpp
// C++ 代码
#define LOCTEXT_NAMESPACE "MyGame"
FText Message = FText::Format(
    LOCTEXT("WelcomeMessage", "Welcome, {0}!"),
    FText::FromString(PlayerName)
);
#undef LOCTEXT_NAMESPACE
```

```po
# PO 文件
msgctxt "MyGame,WelcomeMessage"
msgid "Welcome, {0}!"
msgstr "欢迎，{0}！"
```

---

### Q: 如何处理本地化图片/音频？

UE 本地化系统只处理文本。对于资源本地化：

1. **使用不同路径**
   ```cpp
   FString ImagePath = FInternationalization::Get().GetCurrentCulture() == TEXT("zh")
       ? TEXT("/Game/Images/Logo_ZH")
       : TEXT("/Game/Images/Logo_EN");
   ```

2. **使用 localized 资源目录**
   ```
   Content/Images/
   ├── EN/
   │   └── Logo.png
   └── ZH/
       └── Logo.png
   ```

---

## 已知限制

### Q: UPROPERTY 的 ToolTip 可以使用 LOCTEXT 吗？

**答**：**不可以**。这是 UE 的架构限制。

```cpp
// ❌ 编译错误
UPROPERTY(EditAnywhere, meta=(ToolTip=LOCTEXT("Key", "Text")))
FString MyProperty;

// ✅ 使用字符串字面量
UPROPERTY(EditAnywhere, meta=(ToolTip="Tooltip text"))
FString MyProperty;
```

**原因**：UHT（Unreal Header Tool）在 C++ 预处理器之前运行，无法展开宏。

**注意**：
- ToolTip 字符串仍会被采集系统收集
- 但翻译在编辑器中**不会生效**（仅运行时 UI 翻译有效）
- 推荐做法：Editor 插件设置页保持 ToolTip 为英文

相关讨论：
- [How can the display name of a property be localized](https://forums.unrealengine.com/t/how-can-the-display-name-of-a-property-be-localized/1636077)
- [How to disable Localization gathering of ToolTips](https://forums.unrealengine.com/t/how-to-disable-localization-gathering-of-tooltips-and-properties/325636)

---

## 获取帮助

### UE 官方资源

- [Localization Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/localization-tools)
- [Localization Commandlets](https://dev.epicgames.com/documentation/en-us/unreal-engine/localization-commandlets)
- [AnswerHub](https://answerhub.unrealengine.com/)

### 源码位置

```
Engine/Source/Editor/UnrealEd/Private/Commandlets/
├── GatherTextCommandlet.cpp
├── GatherTextFromSourceCommandlet.cpp
├── InternationalizationExportCommandlet.cpp
└── GenerateTextLocalizationResourceCommandlet.cpp
```
