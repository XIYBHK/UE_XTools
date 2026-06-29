# XTools 插件开发脚本工具集

本目录包含用于 XTools 插件开发和打包的自动化脚本工具。

## 📁 脚本列表

| 脚本名称 | 功能描述 | 类型 |
|---------|----------|------|
| `BuildPlugin-MultiUE.ps1` | 多版本UE插件自动化打包 | PowerShell |
| `Clean-UEPlugin.ps1` | 插件清理工具 | PowerShell |
| `ue_blueprint_export_flow.py` | UE蓝图复制/导出文本逻辑流梳理 | Python |
| `export_blueprint_asset_copy_text.py` | 通过 UE Editor 命令行从 Blueprint 资产导出节点复制文本 | UE Python |

## 🚀 使用方法

### 在 Scripts 目录下执行

当你在 `Scripts/` 目录下时，可以直接使用相对路径：

#### **1. 多版本自动打包**

```powershell
# 基础用法 - 自动探测所有可用UE版本
.\BuildPlugin-MultiUE.ps1

# 指定特定版本打包
.\BuildPlugin-MultiUE.ps1 -EngineRoots "D:\Program Files\Epic Games\UE_5.4","D:\Program Files\Epic Games\UE_5.5","F:\ProgramFiles\UE_5.6"

# 带实时日志的打包
.\BuildPlugin-MultiUE.ps1 -EngineRoots "D:\Program Files\Epic Games\UE_5.4","D:\Program Files\Epic Games\UE_5.5","F:\ProgramFiles\UE_5.6" -Follow

# 清理输出后重新打包
.\BuildPlugin-MultiUE.ps1 -CleanOutput -Follow

# 自定义输出目录
.\BuildPlugin-MultiUE.ps1 -OutputBase "E:\Plugin_Packages" -Follow
```

#### **2. 插件清理**

```powershell
# 使用PowerShell清理工具
.\Clean-UEPlugin.ps1
```

#### **3. 蓝图导出文本逻辑流梳理**

```powershell
# 打印 Markdown 摘要
python .\ue_blueprint_export_flow.py "..\Docs\ref\MarL_沿样条线移动v4.txt"

# 输出到文件
python .\ue_blueprint_export_flow.py "..\Docs\ref\MarL_沿样条线移动v4.txt" -o "..\Docs\ref\MarL_沿样条线移动v4.flow.md"

# 输出 JSON，便于后续脚本二次处理
python .\ue_blueprint_export_flow.py "..\Docs\ref\MarL_沿样条线移动v4.txt" --format json -o "..\Docs\ref\MarL_沿样条线移动v4.flow.json"
```

#### **4. 从真实 Blueprint 资产导出节点文本**

该脚本需要通过 UE Editor 的 PythonScriptCommandlet 执行，输入 Blueprint 对象路径，输出每个 Graph 的 UE copy-text 风格 `.md` 文件与 `manifest.json`。

```powershell
& "D:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\BaiduNetdiskDownload\跑酷\跑酷.uproject" `
  -run=pythonscript `
  -script="D:/UEProject/cppxtools/plugins/UE_XTools/Scripts/export_blueprint_asset_copy_text.py --asset /Game/_BPTools/Parkour/_BP/launcher/bullet/BP_Base_Bullet.BP_Base_Bullet --output-dir D:/UEProject/cppxtools/plugins/UE_XTools/Docs/ref/BP_Base_Bullet/from_uasset" `
  -unattended -nop4 -nosplash
```

注意：`-script` 内的 Windows 路径建议使用 `/`，避免反斜杠被 UE Python 命令行当作转义字符。

导出的 `.md` 可以继续交给 `ue_blueprint_export_flow.py`：

```powershell
python .\Scripts\ue_blueprint_export_flow.py ".\Docs\ref\BP_Base_Bullet\from_uasset\EventGraph.md"
```

`ue_blueprint_export_flow.py` 会区分入口节点与 Tunnel 边界：EventGraph/ConstructionScript 入口通常是 `K2Node_Event`、`K2Node_CustomEvent`、`K2Node_ComponentBoundEvent`、`K2Node_FunctionEntry`；`K2Node_Tunnel` 主要用于宏、折叠图、子图或编译中间图的输入输出边界。

### 在项目根目录下执行

当你在 `UE_XTools/` 根目录下时：

```powershell
# 多版本打包
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots "D:\Program Files\Epic Games\UE_5.4","D:\Program Files\Epic Games\UE_5.5","F:\ProgramFiles\UE_5.6" -Follow

# 插件清理
.\Scripts\Clean-UEPlugin.ps1
```

## 📋 BuildPlugin-MultiUE.ps1 详细参数

### 基本参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `-PluginUplugin` | string | `../XTools.uplugin` | 插件描述文件路径 |
| `-EngineRoots` | string[] | 自动探测 | UE安装根目录列表 |
| `-OutputBase` | string | `../../Plugin_Packages` | 输出根目录 |
| `-TargetPlatforms` | string | `Win64` | 目标平台 |

### 控制开关

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `-StrictIncludes` | switch | `$true` | 启用严格包含检查 |
| `-NoHostProject` | switch | `$true` | 不使用HostProject |
| `-CleanOutput` | switch | `$false` | 打包前清空输出目录 |
| `-Follow` | switch | `$false` | 实时跟踪打包日志 |

### 常用命令组合

#### **开发测试场景**
```powershell
# 快速单版本测试
.\BuildPlugin-MultiUE.ps1 -EngineRoots "D:\Program Files\Epic Games\UE_5.4"

# 带日志的调试打包
.\BuildPlugin-MultiUE.ps1 -EngineRoots "D:\Program Files\Epic Games\UE_5.4" -Follow -CleanOutput
```

#### **发布场景**
```powershell
# 完整多版本发布打包
.\BuildPlugin-MultiUE.ps1 -CleanOutput -Follow

# 指定版本发布打包
.\BuildPlugin-MultiUE.ps1 -EngineRoots "D:\Program Files\Epic Games\UE_5.4","D:\Program Files\Epic Games\UE_5.5","F:\ProgramFiles\UE_5.6" -CleanOutput -Follow
```

#### **自定义输出**
```powershell
# 输出到指定目录
.\BuildPlugin-MultiUE.ps1 -OutputBase "E:\MyPlugins\Releases" -Follow

# 输出到网络路径
.\BuildPlugin-MultiUE.ps1 -OutputBase "\\Server\Shared\Plugins" -Follow
```

## 🎯 自动探测的UE版本

脚本会自动探测以下位置的UE安装：

```
D:\Program Files\Epic Games\UE_5.4
D:\Program Files\Epic Games\UE_5.5  
D:\Program Files\Epic Games\UE_5.6
F:\ProgramFiles\UE_5.6
```

## 📂 输出结构

打包完成后，会在输出目录生成以下结构：

```
Plugin_Packages/
├── XTools-UE_5.4/
│   ├── XTools.uplugin
│   ├── Binaries/
│   ├── Content/
│   └── Resources/
├── XTools-UE_5.5/
│   └── ...
└── XTools-UE_5.6/
    └── ...
```

## ⚠️ 注意事项

1. **权限要求**：需要 PowerShell 执行权限
2. **路径空格**：包含空格的路径需要用引号包围
3. **网络路径**：支持UNC网络路径输出
4. **并行安全**：不同版本可以并行打包
5. **错误处理**：单个版本失败不会影响其他版本

## 🔧 故障排除

### 常见问题

**PowerShell执行策略错误**
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

**UE版本未找到**
```powershell
# 手动指定完整路径
.\BuildPlugin-MultiUE.ps1 -EngineRoots "C:\UE_5.4\Engine"
```

**输出目录权限错误**
```powershell
# 使用管理员权限运行PowerShell
# 或者更改输出目录到有权限的位置
.\BuildPlugin-MultiUE.ps1 -OutputBase "D:\MyOutput"
```

## 📊 示例输出

成功打包的输出示例：
```
=== Summary ===

Engine Output  
------ ------
UE_5.4 E:\Plugin_Packages\XTools-UE_5.4
UE_5.5 E:\Plugin_Packages\XTools-UE_5.5  
UE_5.6 E:\Plugin_Packages\XTools-UE_5.6
```

---

**📝 提示**：推荐使用 `-Follow` 参数来实时查看打包进度，特别是在调试构建问题时。
