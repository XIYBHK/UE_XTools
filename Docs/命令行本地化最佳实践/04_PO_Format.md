# PO 文件格式规范

GNU gettext PO（Portable Object）文件格式说明和翻译规范。

## 文件位置

```
Content/Localization/Game/
├── en/
│   └── Game.po      # 英文（源语言）
├── zh/
│   └── Game.po      # 中文翻译
└── ja/
    └── Game.po      # 日文翻译
```

---

## PO 文件结构

### 完整示例

```po
# Game English Translation
# Copyright (C) 2026
# This file is distributed under the same license as the Game package.
# FIRST AUTHOR <EMAIL@ADDRESS>, 2026.
#
msgid ""
msgstr ""
"Project-Id-Version: Game\n"
"Report-Msgid-Bugs-To: \n"
"POT-Creation-Date: 2026-01-21 10:30+0000\n"
"PO-Revision-Date: 2026-01-21 10:30+0000\n"
"Last-Translator: \n"
"Language-Team: Chinese (Simplified)\n"
"Language: zh\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"
"Plural-Forms: nplurals=1; plural=0;\n"

#. Key:	SaveButton
#. SourceLocation:	Source/Private/MainMenu.cpp(42)
#: Source/Private/MainMenu.cpp(42)
msgctxt "MyGame,SaveButton"
msgid "Save Game"
msgstr "保存游戏"

#: Source/Private/MainMenu.cpp(55)
msgctxt "MyGame,LoadButton"
msgid "Load Game"
msgstr "加载游戏"

#: Source/Private/HUD.cpp(100)
msgctxt "MyGame,HealthDisplay"
msgid "Health: {0}/{1}"
msgstr "生命值: {0}/{1}"
```

---

## 字段说明

### 头部元数据

| 字段 | 说明 | 示例 |
|------|------|------|
| `Project-Id-Version` | 项目名称 | `Game 1.0` |
| `POT-Creation-Date` | 模板创建日期 | `2026-01-21 10:30+0000` |
| `PO-Revision-Date` | 翻译修订日期 | `2026-01-21 10:30+0000` |
| `Language` | 语言代码 | `zh` |
| `Content-Type` | 内容类型和编码 | `text/plain; charset=UTF-8` |
| `Plural-Forms` | 复数形式规则 | `nplurals=2; plural=(n != 1);` |

### 条目字段

| 字段 | 说明 | 示例 |
|------|------|------|
| `#.` | 键名和位置注释 | `Key: SaveButton` |
| `#:` | 源代码引用 | `Source/Private/File.cpp(42)` |
| `#.` | 开发者注释 | `Tooltip for the save button` |
| `msgctxt` | 上下文（命名空间,键名） | `MyGame,SaveButton` |
| `msgid` | 原文（源语言） | `Save Game` |
| `msgid_plural` | 原文复数形式 | `Save Files` |
| `msgstr[0]` | 译文（单数） | `保存游戏` |
| `msgstr[1]` | 译文（复数） | `保存文件` |

---

## UE 特定格式

### msgctxt 格式

UE 使用 `命名空间,键名` 格式：

```po
msgctxt "MyGame,SaveButton"
msgid "Save"
msgstr "保存"
```

### 带参数的文本

```po
msgctxt "MyGame,ItemCount"
msgid "You have {0} items"
msgstr "你有 {0} 个物品"
```

**重要**：保留占位符 `{0}`, `{1}`, 等等。

### 换行符

```po
msgctxt "MyGame,MultiLineText"
msgid "Line 1\nLine 2\nLine 3"
msgstr "第一行\n第二行\n第三行"
```

### 带格式化的文本

```po
msgctxt "MyGame,PercentFormat"
msgid "Progress: {0}%"
msgstr "进度：{0}%"
```

---

## 翻译规范

### 基本规则

1. **保留上下文**：不要修改 `msgctxt` 的值
2. **保留占位符**：保留所有 `{0}`, `{1}`, `%s`, `%d` 等
3. **保留换行符**：使用 `\n` 保留换行
4. **保持长度**：译文长度尽量接近原文（考虑 UI 空间）

### 占位符示例

| 原文 | 正确翻译 | 错误翻译 |
|------|----------|----------|
| `Processing {0} items` | `正在处理 {0} 项` | `正在处理 项`（缺少占位符） |
| `Progress: {0}%` | `进度：{0}%` | `进度：100%`（硬编码数值） |
| `{0} of {1} files` | `{1} 个文件中的第 {0} 个` | `0 of 1 files`（删除占位符） |

### 复数形式

```po
msgid "File"
msgid_plural "Files"
msgstr[0] "文件"
msgstr[1] "多个文件"
```

**复数规则**（根据语言不同）：

| 语言 | nplurals | plural 表达式 |
|------|----------|--------------|
| 英语 | 2 | `(n != 1)` |
| 中文 | 1 | `0` |
| 日语 | 1 | `0` |
| 法语 | 2 | `(n > 1)` |
| 俄语 | 3 | `n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 \|\| n%100>=20) ? 1 : 2` |

### 模糊匹配

不确定的翻译可标记为：

```po
#, fuzzy
msgid "Source text"
msgstr "翻译..."
```

**注意**：`# ,fuzzy` 条目不会被编译，完成后需移除此标记。

### 开发者注释

添加给翻译者的说明：

```po
#. This is a tooltip for the main menu button
#. Keep it short as it will be displayed on hover
#: Source/Private/MainMenu.cpp(42)
msgctxt "MyGame,MenuTooltip"
msgid "Click to open menu"
msgstr "点击打开菜单"
```

---

## PO 文件操作

### 创建新语言 PO 文件

```bash
# 复制源语言 PO 文件
cp Content/Localization/Game/en/Game.po Content/Localization/Game/zh/Game.po

# 编辑头部信息
# - 修改 Language: zh
# - 修改 Language-Team: Chinese (Simplified)
```

### 合并 PO 文件

使用 Poedit：

1. 打开新的 PO 文件
2. Catalog → Update from POT file
3. 选择旧的 PO 文件
4. Poedit 会合并翻译并标记变更

使用命令行（msgmerge）：

```bash
msgmerge --update old.po new.pot
```

### 验证 PO 文件

```bash
# 检查格式错误
msgfmt --check Game.po

# 统计翻译进度
msgfmt --statistics Game.po
# 输出: 150 translated messages, 3 fuzzy translations, 2 untranslated messages.
```

---

## 推荐的 PO 编辑工具

### Poedit（推荐）

- **平台**：Windows / macOS / Linux
- **特点**：
  - 自动维护翻译记忆库
  - 内置拼写检查
  - 支持自动翻译集成
  - 直观的 GUI

### VS Code + PO 插件

- **插件**：`gettext-po` 或 `i18n-ally`
- **特点**：
  - 轻量级
  - 与 Git 集成
  - 支持多种文件格式

### OmegaT

- **平台**：Java（跨平台）
- **特点**：
  - 开源免费
  - 强大的翻译记忆
  - 支持多种文件格式

### 在线平台

| 平台 | 说明 | 网址 |
|------|------|------|
| **Crowdin** | 企业级翻译管理 | crowdin.com |
| **Loco** | 简洁的翻译管理 | localise.biz |
| **POEditor** | 在线 PO 编辑器 | poeditor.com |

---

## 常见问题

### Q: PO 文件中的特殊字符如何处理？

```po
# 引号
msgid "Click \"OK\" to continue"
msgstr "点击"确定"继续"

# 反斜杠
msgid "Path: C:\\Games"
msgstr "路径：C:\\游戏"

# 换行
msgid "Line 1\nLine 2"
msgstr "第一行\n第二行"
```

### Q: 翻译包含 HTML 标签怎么办？

```po
msgid "Click <b>here</b> to continue"
msgstr "点击<b>这里</b>继续"
```

**原则**：保留 HTML 标签，只翻译文本内容。

### Q: 如何处理过长的译文？

如果译文比原文长很多导致 UI 问题：

1. 尝试简化译文
2. 或在源代码中使用更短的原文
3. 或调整 UI 布局以适应长文本

### Q: msgstr 为空是什么意思？

`msgstr ""` 表示该条目尚未翻译。

```po
msgid "To be translated"
msgstr ""    # 空字符串 = 未翻译
```

### Q: 如何跳过某些条目的翻译？

保留空字符串即可：

```po
msgid "Developer Debug Message"
msgstr ""    # 留空不翻译
```
