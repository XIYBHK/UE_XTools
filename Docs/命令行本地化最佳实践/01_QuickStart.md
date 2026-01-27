# UE 本地化命令行 - 快速开始

快速参考指南，包含常用命令和参数说明。

## 快速命令

### 三步完成本地化

```bash
# 1. 采集文本
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Gather.ini" -Unattended -NoShaderCompile

# 2. 导出 PO 模板
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Export.ini" -Unattended -NoShaderCompile

# 3. 编译资源（翻译完成后）
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Compile.ini" -Unattended -NoShaderCompile
```

### 基础命令格式

```bash
UnrealEditor-Cmd.exe "<项目路径>.uproject" -run=GatherText -config="<配置文件路径>" [选项]
```

---

## 命令行参数

### 常用参数

| 参数 | 说明 |
|------|------|
| `-run=GatherText` | 运行本地化 Commandlet |
| `-config=` | 配置文件路径（相对或绝对） |
| `-Unattended` | 无头模式，不显示 UI |
| `-NoShaderCompile` | 跳过着色器编译，加快速度 |

### 可选参数

| 参数 | 说明 |
|------|------|
| `-DisablePlugins=插件名` | 禁用可能冲突的插件 |
| `-Preview` | 预览模式，不写入文件，用于测试 |
| `-EnableSCC` | 启用版本控制，允许检出文件 |
| `-DisableSCCSubmit` | 禁止向版本控制提交更改 |
| `-GatherType=` | 指定采集类型：`All`（默认）、`Source`、`Asset`、`Metadata` |

### 示例

```bash
# 只采集源码
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -GatherType=Source -Unattended

# 预览模式测试
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Gather.ini" -Preview -Unattended
```

---

## 路径标记

配置文件中使用的路径标记：

| 标记 | 解析为 | 示例 |
|------|--------|------|
| `%LOCPROJECTROOT%` | 项目根目录 | `D:/Projects/MyGame/` |
| `%LOCENGINEROOT%` | 引擎根目录 | `D:/UE_5.3/` |

**使用示例**：
```ini
SourcePath=%LOCPROJECTROOT%Content/Localization/Game
```

---

## 语言代码

标准语言代码列表：

| 语言 | 代码 | 目录 |
|------|------|------|
| 英语（美国） | `en` | `en/` |
| 简体中文 | `zh` | `zh/` |
| 繁体中文 | `zh-Hant` | `zh-Hant/` |
| 日语 | `ja` | `ja/` |
| 韩语 | `ko` | `ko/` |
| 法语 | `fr` | `fr/` |
| 德语 | `de` | `de/` |
| 西班牙语 | `es` | `es/` |

**运行时切换语言**：
```cpp
FInternationalization::Get().SetCurrentCulture(TEXT("zh"));
```

---

## 插件类型与配置

### Editor 类型插件

**特征**：uplugin 中 `"Type": "Editor"`

**关键配置**：
```ini
ShouldGatherFromEditorOnlyData=True
```

**原因**：Editor 插件的所有代码都在 `#if WITH_EDITOR` 宏内，必须设置此项才能采集。

### Runtime 类型插件

**特征**：uplugin 中 `"Type": "Runtime"` 或未指定

**关键配置**：
```ini
ShouldGatherFromEditorOnlyData=False
```

---

## 文件结构

### 输出目录结构

```
Content/Localization/Game/
├── Game.manifest           # 清单文件（提交到版本控制）
├── Game.archive            # 存档文件（提交到版本控制）
├── Game.csv                # 统计报告（临时）
├── Game_Conflicts.txt      # 冲突报告（临时）
└── en/
    ├── Game.po             # 翻译文件（提交到版本控制）
    └── Game.locres         # 运行时资源（不提交，可重新生成）
```

---

## 快速检查清单

执行采集前检查：

- [ ] 配置文件路径正确
- [ ] `SearchDirectoryPaths` 使用相对路径
- [ ] Editor 插件设置了 `ShouldGatherFromEditorOnlyData=True`
- [ ] 源码中使用了 `LOCTEXT` 或 `NSLOCTEXT` 宏
- [ ] 项目文件路径正确

执行编译前检查：

- [ ] PO 文件已翻译
- [ ] PO 文件在正确的语言目录下
- [ ] `CulturesToGenerate` 包含目标语言
- [ ] PO 文件编码为 UTF-8
