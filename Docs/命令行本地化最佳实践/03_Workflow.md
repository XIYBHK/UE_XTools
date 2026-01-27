# UE 本地化工作流程

完整的本地化工作流程，包括增量更新、添加新语言、版本控制策略。

## 完整流程图

```
C++ 源码 (LOCTEXT/NSLOCTEXT 宏)
        ↓
┌───────────────────────────────────────┐
│ 1. 采集 (GatherText)                  │
│    生成 .manifest 和 .archive          │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ 2. 导出 (Export)                       │
│    生成 .po 翻译模板                    │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ 3. 翻译                                 │
│    使用 PO 编辑器填充 msgstr            │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ 4. 导入 (Import) - 可选                │
│    更新 .archive（如使用外部翻译）      │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ 5. 编译 (Compile)                      │
│    生成 .locres 运行时资源              │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ 6. 运行时使用                           │
│    FInternationalization::SetCulture   │
└───────────────────────────────────────┘
```

---

## 首次本地化

### 步骤 1: 准备源码

确保 C++ 代码中使用本地化宏：

```cpp
// 定义命名空间
#define LOCTEXT_NAMESPACE "MyNamespace"

// 简单文本
FText MyText = LOCTEXT("MyKey", "Hello World");

// 带参数的文本
FText FormatText = FText::Format(
    LOCTEXT("FormatKey", "Processing {0} items"),
    FText::AsNumber(42)
);

// 或使用宏定义（跨文件）
const FText CommandLabel = NSLOCTEXT("MyNamespace", "CommandLabel", "Open Window");

// 结束命名空间
#undef LOCTEXT_NAMESPACE
```

### 步骤 2: 执行采集

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Gather.ini" -Unattended
```

**输出**：
- `Content/Localization/Game/Game.manifest`
- `Content/Localization/Game/Game.archive`
- `Content/Localization/Game/Game.csv`（统计）
- `Content/Localization/Game/Game_Conflicts.txt`（冲突）

### 步骤 3: 导出 PO 模板

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Export.ini" -Unattended
```

**输出**：
- `Content/Localization/Game/en/Game.po`

### 步骤 4: 翻译 PO 文件

1. 创建目标语言目录：`Content/Localization/Game/zh/`
2. 复制 PO 文件：`cp en/Game.po zh/Game.po`
3. 使用 PO 编辑器翻译

### 步骤 5: 编译资源

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Compile.ini" -Unattended
```

**输出**：
- `Content/Localization/Game/zh/Game.locres`

### 步骤 6: 运行时使用

```cpp
// 切换语言
FInternationalization::Get().SetCurrentCulture(TEXT("zh"));

// 获取本地化文本（自动使用当前语言）
FText LocalizedText = NSLOCTEXT("MyNamespace", "MyKey", "Fallback Text");
```

---

## 增量更新工作流

当源码中添加新的本地化文本时，保留已有翻译。

### 步骤 1: 备份现有翻译

```bash
# Windows
copy Content\Localization\Game\zh\Game.po Content\Localization\Game\zh\Game.po.bak

# Linux/Mac
cp Content/Localization/Game/zh/Game.po Content/Localization/Game/zh/Game.po.bak
```

### 步骤 2: 重新采集

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Gather.ini" -Unattended
```

### 步骤 3: 导出更新后的 PO 模板

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Export.ini" -Unattended
```

### 步骤 4: 合并翻译

使用 PO 编辑工具（如 Poedit）：

1. 打开新生成的 `en/Game.po`
2. 使用"从旧文件合并翻译"功能
3. 选择备份的 `Game.po.bak`
4. 保存合并后的 PO 文件到 `zh/Game.po`

**手动合并方法**：
- 新增条目：填充新的 `msgstr`
- 修改条目：检查并更新翻译
- 删除条目：PO 编辑器会标记为"已过时"

### 步骤 5: 编译

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Compile.ini" -Unattended
```

---

## 添加新语言

### 步骤 1: 更新所有配置

在 `Gather.ini`、`Export.ini`、`Compile.ini` 中添加：

```ini
[CommonSettings]
NativeCulture=en
CulturesToGenerate=en
CulturesToGenerate=zh
CulturesToGenerate=ja    ; 新增日语
```

### 步骤 2: 导出新语言 PO 模板

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Export.ini" -Unattended
```

### 步骤 3: 翻译

1. 将 `en/Game.po` 复制到 `ja/Game.po`
2. 翻译 `ja/Game.po`

### 步骤 4: 编译新语言资源

```bash
UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Config/Localization/Compile.ini" -Unattended
```

---

## 多语言同步

| 情况 | 处理方法 |
|------|----------|
| **源码新增文本** | 重新采集 → 导出 → 合并翻译 → 编译 |
| **源码修改文本** | 重新采集 → 导出 → 检查冲突报告 → 更新翻译 |
| **源码删除文本** | 重新采集 → 导出 → PO 编辑器标记为"已过时" |
| **修改已有翻译** | 直接编辑 PO 文件 → 重新编译 |

---

## 版本控制策略

### 应提交的文件

| 文件类型 | 路径 | 说明 |
|----------|------|------|
| 配置文件 | `Config/Localization/*.ini` | 本地化配置 |
| 源码 | `Source/**/*.cpp` | 包含 LOCTEXT 宏 |
| Manifest | `Content/Localization/*/*.manifest` | 本地化清单 |
| Archive | `Content/Localization/*/*.archive` | 本地化存档 |
| PO 文件 | `Content/Localization/*/*/*.po` | 翻译文件 |

### 不应提交的文件

| 文件类型 | 路径 | 原因 |
|----------|------|------|
| 编译资源 | `Content/Localization/*/*/*.locres` | 可重新生成 |
| CSV 报告 | `Content/Localization/*/*.csv` | 临时统计 |
| 冲突报告 | `Content/Localization/*/*_Conflicts.txt` | 临时报告 |

### .gitignore 配置

```gitignore
# UE 本地化生成的临时文件
**/Content/Localization/*/*/*.locres
**/Content/Localization/*/*.csv
**/Content/Localization/*/*_Conflicts.txt
```

### 团队协作工作流

```
开发者 A（英文）                    开发者 B（中文）
     │                                   │
     ├─ 添加新 LOCTEXT                    │
     │                                   │
     ├─ 运行 Gather + Export             │
     │                                   │
     ├─ 提交 en/Game.po                  │
     │                                   │
     │                                   ├─ 拉取更新
     │                                   │
     │                                   ├─ 复制 en → zh
     │                                   │
     │                                   ├─ 翻译 zh/Game.po
     │                                   │
     │                                   ├─ 提交 zh/Game.po
     │                                   │
     ├─ 拉取更新                         │
     │                                   │
     └─ 运行 Compile                    │
```

---

## PO 文件编码

**重要**：PO 文件必须使用 UTF-8 编码。

### 检查编码

```bash
# Linux/Mac
file -i Content/Localization/Game/en/Game.po

# Windows PowerShell
Get-Content Content\Localization\Game\en\Game.po | Select-String -Pattern "charset"
```

### 转换编码

```bash
# 如果不是 UTF-8，转换编码
iconv -f GBK -t UTF-8 input.po > output.po
```

### 编辑器建议

- **推荐**：Poedit、VS Code、Sublime Text
- **不推荐**：Windows 记事本（可能使用错误的编码）

---

## 翻译记忆库维护

使用专业工具维护翻译记忆，加速后续翻译：

| 工具 | 类型 | 说明 |
|------|------|------|
| **Poedit** | 桌面软件 | 自动维护翻译记忆库 |
| **OmegaT** | 桌面软件 | 开源翻译记忆工具 |
| **Crowdin** | 在线平台 | 团队协作翻译 |
| **Loco** | 在线平台 | 简洁的翻译管理 |

---

## CI/CD 集成

### GitHub Actions 示例

```yaml
name: Localization

on:
  push:
    paths:
      - 'Source/**'
      - 'Config/Localization/**'

jobs:
  gather:
    runs-on: [self-hosted, windows]
    steps:
      - name: Gather Text
        run: |
          & 'D:\UE\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
            'D:\Projects\Game.uproject' `
            -run=GatherText `
            -config='D:\Projects\Config\Localization\Gather.ini' `
            -Unattended -NoShaderCompile

  export:
    needs: gather
    runs-on: [self-hosted, windows]
    steps:
      - name: Export PO
        run: |
          & 'D:\UE\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
            'D:\Projects\Game.uproject' `
            -run=GatherText `
            -config='D:\Projects\Config\Localization\Export.ini' `
            -Unattended -NoShaderCompile
```

---

## 常见工作流问题

### Q: 重新采集后翻译丢失？

确保：
1. 在翻译前备份了 PO 文件
2. 使用了合并功能而非覆盖
3. 重新导出后立即合并

### Q: 多人同时翻译同一语言如何处理？

建议：
1. 使用在线协作平台（Crowdin、Loco）
2. 或按模块拆分 PO 文件
3. 定期同步合并

### Q: 如何批量更新多个语言？

```bash
# 导出所有语言的 PO
for lang in en zh ja ko; do
    UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Export_$lang.ini" -Unattended
done

# 编译所有语言
for lang in en zh ja ko; do
    UnrealEditor-Cmd.exe "Game.uproject" -run=GatherText -config="Compile_$lang.ini" -Unattended
done
```
