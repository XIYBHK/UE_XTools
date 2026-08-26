# XTools 脚本

本目录包含插件打包、清理、预设资产生成和 Blueprint 文本分析工具。命令默认从插件根目录执行；在 `Scripts/` 目录执行时去掉前缀 `Scripts\`。

## 脚本清单

| 脚本 | 用途 |
| --- | --- |
| `BuildPlugin-MultiUE.ps1` | UE 5.3-5.8 多版本 BuildPlugin |
| `Clean-UEPlugin.ps1` | 清理插件生成目录 |
| `ue插件清理/clean_ue_plugin.bat` | 压缩包或批量清理入口 |
| `InvokePresetAssetTool.ps1` | 生成/验证曲线、缩放和 CameraShake 预设 |
| `GeneratePresetLibraryAssets.py` | 普通预设资产 UE Python 生成器 |
| `GenerateEasingCurveAssets.py` | 数学缓动曲线 UE Python 生成器 |
| `PresetLibraryAssets.json` | 预设资产数据源 |
| `export_blueprint_asset_copy_text.py` | 从 Blueprint 资产导出节点 copy-text |
| `ue_blueprint_export_flow.py` | 将 Blueprint copy-text 整理为 Markdown/JSON 流程 |

## 多版本打包

自动探测脚本支持的 UE 5.3-5.8 安装：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 -Follow
```

显式指定引擎目录：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots @(
    '<UE_5.3>'
    '<UE_5.8>'
) -Follow
```

自定义输出目录并清理本次目标：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 `
    -OutputBase '<PackageOutput>' `
    -CleanOutput `
    -Follow
```

主要参数：

| 参数 | 默认行为 |
| --- | --- |
| `-PluginUplugin` | 当前插件的 `XTools.uplugin` |
| `-EngineRoots` | 未指定时检查脚本维护的候选安装目录 |
| `-OutputBase` | 插件仓库上级的 `Plugin_Packages` |
| `-TargetPlatforms` | `Win64` |
| `-StrictIncludes` | 默认开启；UE 5.3 由脚本显式跳过 |
| `-NoHostProject` | 默认开启 |
| `-CleanOutput` | 默认关闭 |
| `-Follow` | 默认关闭 |

详见 [UE 5.3-5.8 多版本插件打包](../Docs/打包相关/UE_多版本插件打包_问题与解决方案.md)。

## 插件清理

```powershell
.\Scripts\Clean-UEPlugin.ps1
```

压缩包或批量清理使用：

```text
Scripts/ue插件清理/clean_ue_plugin.bat
```

执行清理前确认目标是插件目录或明确的打包输出目录。不要对项目根、磁盘根或未知路径执行递归清理。

## 预设资产

```powershell
# 普通曲线、缩放和 CameraShake
.\Scripts\InvokePresetAssetTool.ps1 -Set PresetLibrary -Mode Generate
.\Scripts\InvokePresetAssetTool.ps1 -Set PresetLibrary -Mode Validate

# 数学缓动曲线
.\Scripts\InvokePresetAssetTool.ps1 -Set Easing -Mode Generate
.\Scripts\InvokePresetAssetTool.ps1 -Set Easing -Mode Validate

# 全部预设
.\Scripts\InvokePresetAssetTool.ps1 -Set All -Mode Generate -Force
```

非默认环境显式传入路径：

```powershell
.\Scripts\InvokePresetAssetTool.ps1 `
    -Set PresetLibrary `
    -Mode Validate `
    -EngineRoot '<EngineRoot>' `
    -ProjectFile '<Project>\Project.uproject'
```

`-Force` 只能与 `Generate` 组合。脚本在项目 `Saved/Logs` 下生成独立日志，并检查完成标记。数据格式和扩展方式见 [曲线与镜头晃动预设生成工具链](../Docs/开发文档/曲线与镜头晃动预设生成工具链.md)。

## Blueprint 资产导出

通过 UE 的 Python Script Commandlet 导出真实 Blueprint Graph：

```powershell
$editorExe = '<EngineRoot>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments = @(
    '<Project>\Project.uproject'
    '-run=pythonscript'
    '-script=<Plugin>/Scripts/export_blueprint_asset_copy_text.py --asset /Game/Path/BP_Example.BP_Example --output-dir <Output>'
    '-unattended'
    '-nop4'
    '-nosplash'
)

& $editorExe @arguments
if ($LASTEXITCODE -ne 0) {
    throw 'Blueprint export failed'
}
```

`-script` 参数中的 Windows 路径建议使用 `/`，避免 UE Python 命令行把反斜杠解释为转义。

将导出内容整理为流程摘要：

```powershell
python .\Scripts\ue_blueprint_export_flow.py '<Output>\EventGraph.md'
python .\Scripts\ue_blueprint_export_flow.py '<Output>\EventGraph.md' --format json -o '<Output>\EventGraph.flow.json'
```

分析器会区分 Event/Function 入口与 `K2Node_Tunnel` 宏、折叠图和子图边界。

## 故障处理

- 找不到 UE：使用 `-EngineRoots` 或 `-EngineRoot` 显式传入安装根目录。
- 找不到项目：使用 `-ProjectFile` 传入 `.uproject` 绝对路径。
- DLL 被占用：关闭 Editor 和 Live Coding 后重新执行。
- PowerShell 参数被拆分：把每个 native 参数放入数组，使用 `& $exe @args`。
- 输出目录无权限：选择当前用户可写目录，不以管理员权限掩盖错误目标。
