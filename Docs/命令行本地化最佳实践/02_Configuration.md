# UE 本地化配置文件详解

所有配置文件的详细说明和参数解释。

## 配置文件概述

| 配置文件 | 用途 | Commandlet | 阶段 |
|----------|------|------------|------|
| `Gather.ini` | 采集文本 | GatherTextFromSource/Assets/Metadata | 1 |
| `Export.ini` | 导出 PO | InternationalizationExport | 2 |
| `Import.ini` | 导入 PO | InternationalizationExport (bImportLoc) | 4 |
| `Compile.ini` | 编译资源 | GenerateTextLocalizationResource | 5 |

---

## 1. 采集配置 (Gather.ini)

### CommonSettings

```ini
[CommonSettings]
; 输出路径（使用路径标记）
SourcePath=%LOCPROJECTROOT%Content/Localization/Game
DestinationPath=%LOCPROJECTROOT%Content/Localization/Game

; 生成的文件名
ManifestName=Game.manifest
ArchiveName=Game.archive

; 原始语言
NativeCulture=en

; 要生成的语言（采集阶段可以只指定原始语言）
CulturesToGenerate=en
```

### GatherTextStep0 - 从源码采集

```ini
[GatherTextStep0]
CommandletClass=GatherTextFromSource

; 搜索目录（相对于项目根目录）
SearchDirectoryPaths=Source/Private
SearchDirectoryPaths=Source/Public

; 排除路径
ExcludePathFilters=Config/Localization/*

; 文件扩展名
FileNameFilters=*.cpp
FileNameFilters=*.h
FileNameFilters=*.hpp

; 是否采集编辑器专用数据
ShouldGatherFromEditorOnlyData=True    ; Editor 插件: True
                                       ; Runtime: False
```

### GatherTextStep1 - 从资产采集

```ini
[GatherTextStep1]
CommandletClass=GatherTextFromAssets

; 包含的资产路径（使用 /Game/ 前缀）
IncludePathFilters=/Game/*
IncludePathFilters=/Game/MyFolder/*

; 排除路径
ExcludePathFilters=Content/Localization/*

; 资产文件类型
PackageFileNameFilters=*.uasset;*.umap

; 是否采集编辑器专用数据
ShouldGatherFromEditorOnlyData=True

; 是否跳过缓存
SkipGatherCache=False    ; False: 快速（默认）
                        ; True: 强制完整加载，调试时使用
```

### GatherTextStep2 - 从元数据采集

```ini
[GatherTextStep2]
CommandletClass=GatherTextFromMetadata
IncludePathFilters=/Game/*
```

### 生成步骤

```ini
[GatherTextStep3]
CommandletClass=GenerateGatherManifest

[GatherTextStep4]
CommandletClass=GenerateGatherArchive

[GatherTextStep5]
CommandletClass=GenerateTextLocalizationReport
bWordCountReport=True
WordCountReportName=Game.csv
bConflictReport=True
ConflictReportName=Game_Conflicts.txt
```

---

## 2. 导出配置 (Export.ini)

```ini
[CommonSettings]
SourcePath=%LOCPROJECTROOT%Content/Localization/Game
DestinationPath=%LOCPROJECTROOT%Content/Localization/Game
ManifestName=Game.manifest
ArchiveName=Game.archive
PortableObjectName=Game.po
NativeCulture=en
CulturesToGenerate=en

[GatherTextStep0]
CommandletClass=InternationalizationExport
bExportLoc=true

; PO 文件格式
POFormat=Unreal              ; Unreal: UE 标准格式（推荐）
                            ; Crowdin: Crowdin 平台格式

; 文本折叠模式
LocalizedTextCollapseMode=IdenticalTextIdAndSource
                            ; IdenticalTextIdAndSource: 推荐
                            ; IdenticalNamespaceAndSource: 旧模式

; 导出选项
ShouldPersistCommentsOnExport=true        ; 保留注释
ShouldAddSourceLocationsAsComments=true    ; 添加源代码位置
```

---

## 3. 导入配置 (Import.ini)

```ini
[CommonSettings]
SourcePath=%LOCPROJECTROOT%Content/Localization/Game
DestinationPath=%LOCPROJECTROOT%Content/Localization/Game
ManifestName=Game.manifest
ArchiveName=Game.archive
PortableObjectName=Game.po
NativeCulture=en
CulturesToGenerate=zh    ; 指定要导入的目标语言

[GatherTextStep0]
CommandletClass=InternationalizationExport
bImportLoc=true
LocalizedTextCollapseMode=IdenticalTextIdAndSource
```

---

## 4. 编译配置 (Compile.ini)

```ini
[CommonSettings]
SourcePath=%LOCPROJECTROOT%Content/Localization/Game
DestinationPath=%LOCPROJECTROOT%Content/Localization/Game
ManifestName=Game.manifest
ArchiveName=Game.archive
ResourceName=Game.locres
NativeCulture=en
CulturesToGenerate=zh

[GatherTextStep0]
CommandletClass=GenerateTextLocalizationResource

; 验证选项
bSkipSourceCheck=false              ; 跳过源检查
bValidateFormatPatterns=true        ; 验证占位符 {0}, %s 等
bValidateSafeWhitespace=true        ; 验证空白字符
```

---

## 高级配置选项

### LocalizedTextCollapseMode（文本折叠模式）

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| `IdenticalTextIdAndSource` | 折叠相同命名空间+键和源文本的条目 | UE 4.15+（推荐） |
| `IdenticalNamespaceAndSource` | 折叠相同命名空间和源文本的条目 | 旧项目兼容 |

**示例**：
```cpp
// 两个相同条目
NSLOCTEXT("MyNS", "Save", "Save")    // File A.cpp
NSLOCTEXT("MyNS", "Save", "Save")    // File B.cpp
```
- `IdenticalTextIdAndSource`：合并为一条 PO 条目
- `IdenticalNamespaceAndSource`：合并为一条 PO 条目

```cpp
// 键相同但源文本不同
NSLOCTEXT("MyNS", "Save", "Save File")
NSLOCTEXT("MyNS", "Save", "Save Project")
```
- `IdenticalTextIdAndSource`：生成两条独立 PO 条目（推荐）
- `IdenticalNamespaceAndSource`：合并为一条（可能冲突）

### POFormat

| 格式 | msgctxt 内容 | msgid 内容 |
|------|--------------|------------|
| `Unreal` | 命名空间,键 | 源文本 |
| `Crowdin` | 空 | 命名空间,键 |

### SkipGatherCache

| 值 | 作用 | 性能 |
|----|------|------|
| `false` | 使用缓存 | 快速 |
| `true` | 强制完整加载 | 慢，但更准确 |

**何时设置为 `true`**：
- 怀疑缓存过时
- 资产结构重大变化后
- 调试采集问题

### ShouldGatherFromEditorOnlyData

控制是否采集 `#if WITH_EDITOR` 宏内的文本。

UE 源码过滤逻辑：
```cpp
if (!bIsEditorOnly || ShouldGatherFromEditorOnlyData)
{
    // 采集文本
}
```

| 插件类型 | 设置 | 说明 |
|----------|------|------|
| Editor | `True` | 采集所有编辑器 UI 文本 |
| Runtime | `False` | 只采集运行时文本 |
| Developer | `True` | 开发工具通常在编辑器中使用 |

---

## 配置文件依赖关系

```
Gather (采集)
    ↓ 生成 .manifest 和 .archive
Export (导出 PO)
    ↓ 读取 .manifest 和 .archive，生成 .po
Import (导入 PO)
    ↓ 读取 .po，更新 .archive
Compile (编译)
    ↓ 读取 .manifest 和 .archive，生成 .locres
```

**重要**：每个步骤依赖前一步骤生成的文件。

---

## 常见配置错误

| 错误 | 原因 | 解决方法 |
|------|------|----------|
| 采集 0 条 | 路径使用绝对值或项目名前缀 | 使用相对路径 `Plugins/MyPlugin/Source` |
| 采集 0 条 | Editor 插件未设置 `ShouldGatherFromEditorOnlyData` | 设置为 `True` |
| PO 导出为空 | 未执行采集 | 先运行 Gather |
| 编译后翻译无效 | PO 文件在 en 目录而非 zh 目录 | 移动到正确语言目录 |
| 编译后翻译无效 | `CulturesToGenerate` 未包含目标语言 | 添加语言代码 |
