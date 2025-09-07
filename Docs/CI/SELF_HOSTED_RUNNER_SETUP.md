# GitHub Self-Hosted Runner 设置指南

本指南将帮助您在自己的Windows电脑上设置GitHub Self-Hosted Runner，让您的XTools插件可以在本地自动构建。

## 🎯 为什么使用Self-Hosted Runner？

✅ **零等待时间** - 直接使用您已安装的UE版本，无需下载  
✅ **完全控制** - 在您熟悉的环境中构建  
✅ **高性能** - 使用您电脑的全部性能  
✅ **零成本** - 不消耗GitHub Actions的分钟数  

## 📋 前置条件

在设置Runner之前，确保您的电脑已经：

- ✅ 安装了UE 5.3, 5.4, 5.5, 5.6（至少一个版本）
- ✅ 安装了Visual Studio（包含C++工作负载）
- ✅ PowerShell执行策略允许运行脚本
- ✅ 网络连接稳定

## ⚙️ 环境配置验证

### 您的实际安装环境

根据您的截图和配置，工作流已优化为以下路径：

**UE安装路径**（F盘）：
```
F:\Epic Games\UE_5.3\Engine\
F:\Epic Games\UE_5.4\Engine\
F:\Epic Games\UE_5.5\Engine\
F:\Epic Games\UE_5.6\Engine\
```

**VS2022安装路径**（E盘）：
```
E:\VisualStudio\2022\Common7\IDE\
```

### 路径验证命令

在继续之前，请验证您的安装：

```powershell
# 验证UE安装
Test-Path "F:\Epic Games\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat"  # 应返回True

# 验证VS2022安装
Test-Path "E:\VisualStudio\2022\Common7\IDE\devenv.exe"             # 应返回True

# 列出所有已安装的UE版本
Get-ChildItem "F:\Epic Games\" | Where-Object {$_.Name -like "UE_*"} | Select Name
```

如果任何命令返回 `False` 或出错，请检查您的安装路径。

## 🚀 快速设置步骤

### 第1步：获取Runner Token

1. 进入您的GitHub仓库：`https://github.com/YOUR_USERNAME/UE_XTools`
2. 点击 `Settings` 标签页
3. 在左侧菜单中点击 `Actions` > `Runners`
4. 点击绿色的 `New self-hosted runner` 按钮
5. 选择 `Windows` 和 `x64`
6. **复制显示的命令**（包含Token，后面需要用到）

### 第2步：在您的电脑上安装Runner

以**管理员权限**打开PowerShell，执行以下命令：

```powershell
# 1. 创建Runner目录
mkdir C:\actions-runner
cd C:\actions-runner

# 2. 下载Runner（使用GitHub页面显示的最新链接）
Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/v2.328.0/actions-runner-win-x64-2.328.0.zip -OutFile actions-runner-win-x64-2.328.0.zip

# 3. 解压
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory("$PWD\actions-runner-win-x64-2.328.0.zip", "$PWD")

# 4. 配置Runner（使用GitHub页面复制的完整命令）
.\config.cmd --url https://github.com/YOUR_USERNAME/UE_XTools --token YOUR_RUNNER_TOKEN_HERE

# 5. 测试运行
.\run.cmd
```

### 第3步：设置为Windows服务（推荐）

**注意：根据GitHub官方文档，服务安装必须在初始配置时完成。**

如果在第2步配置时没有选择安装为服务，**必须删除Runner并重新配置**：

```powershell
# 移除当前配置
.\config.cmd remove

# 重新配置（需要新的Token）
.\config.cmd --url https://github.com/YOUR_USERNAME/UE_XTools --token YOUR_NEW_TOKEN
# 在配置过程中选择 Y 来安装为服务

# 服务管理命令
# 查看服务状态
Get-Service actions.runner.*

# 启动服务
Start-Service actions.runner.YOUR_MACHINE_NAME.YOUR_RUNNER_NAME

# 停止服务
Stop-Service actions.runner.YOUR_MACHINE_NAME.YOUR_RUNNER_NAME
```

## 🔧 验证安装

### 检查Runner状态

1. 回到GitHub仓库的 `Settings` > `Actions` > `Runners`
2. 应该看到您的Runner显示为 `Online`（绿色圆点）

### 测试构建

1. 进入 `Actions` 标签页
2. 选择 `Build XTools Plugin (Simplified)` 工作流
3. 点击 `Run workflow`
4. 观察构建日志，应该看到路径检测成功：

```
🎯 Using UE installation at: F:\Epic Games\UE_5.6
🔧 Found VS2022 at: E:\VisualStudio\2022
✅ Found UE 5.6 at: F:\Epic Games\UE_5.6
```

5. 构建应该在您的电脑上成功运行并生成插件包

## 📁 推荐的目录结构

```
C:\actions-runner\          # GitHub Actions Runner
├── _work\                  # 工作目录（自动创建）
│   └── UE_XTools\         # 您的仓库会被克隆到这里
├── config.cmd
├── run.cmd
└── run-helper.cmd

F:\Epic Games\             # ✅ 您的实际UE安装目录
├── UE_5.3\
├── UE_5.4\
├── UE_5.5\
└── UE_5.6\

E:\VisualStudio\           # ✅ 您的实际VS2022安装目录
└── 2022\
    ├── Common7\
    │   └── IDE\
    └── VC\
```

## ⚡ 使用流程

设置完成后，您的完整工作流程是：

```bash
# 1. 在本地开发插件
# 编辑代码...

# 2. 提交到GitHub
git add .
git commit -m "更新插件功能"
git push origin main

# 3. 手动触发构建（在GitHub网页上）
# Actions > Build XTools Plugin (Simplified) > Run workflow

# 4. 等待构建完成（在您的电脑上运行）
# 构建完成后下载Artifacts

# 5. 创建发布版本（可选）
git tag v1.0.0
git push origin v1.0.0
# 会自动创建GitHub Release
```

## 🛠️ 故障排除

### 常见问题

**1. Runner无法连接到GitHub**
```powershell
# 检查网络连接
Test-NetConnection github.com -Port 443

# 检查防火墙设置
# 确保允许actions-runner.exe访问网络
```

**2. UE版本找不到**
```powershell
# 检查UE安装路径（按优先级顺序）
Get-ChildItem "F:\Epic Games\" | Where-Object {$_.Name -like "UE_*"}        # ✅ 您的实际安装路径
Get-ChildItem "D:\Program Files\Epic Games\" | Where-Object {$_.Name -like "UE_*"}  # 备用路径
Get-ChildItem "C:\Program Files\Epic Games\" | Where-Object {$_.Name -like "UE_*"}  # 标准路径

# 检查VS2022安装路径
Get-ChildItem "E:\VisualStudio\2022\" -ErrorAction SilentlyContinue        # ✅ 您的实际安装路径
Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\" -ErrorAction SilentlyContinue

# 工作流已自动配置了路径检测逻辑，会按优先级检测您的实际安装路径
```

**3. PowerShell执行策略错误** ⚡ 
```
PSSecurityException: UnauthorizedAccess
running scripts is disabled on this system
```

**解决方案（三种选择）：**

**方案A：工作流级别解决（推荐）** ✅
```yaml
# 在每个PowerShell步骤中直接指定执行策略：
- name: Setup UE Environment
  shell: pwsh -ExecutionPolicy Bypass {0}
  run: |
    # 您的PowerShell脚本内容...
    Write-Host "✅ PowerShell running with Bypass execution policy"
```

**方案B：自托管运行器全局设置**
```powershell
# 以管理员权限运行PowerShell，一次性设置
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope LocalMachine -Force

# 验证设置
Get-ExecutionPolicy -List
```

**方案C：服务用户设置**
```powershell
# 为运行Runner服务的用户设置
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force
```

**⚠️ 注意：我们的工作流已经采用方案A自动解决此问题。**

**4. 构建权限错误**
```powershell
# 确保Runner服务有足够权限访问UE安装目录
# 可能需要以管理员权限运行Runner服务
```

### 日志位置

- **Runner日志**: `C:\actions-runner\_diag\`
- **构建日志**: GitHub Actions页面查看
- **UE构建日志**: 工作流会显示详细输出

## 🔒 安全考虑

### Runner安全

- ✅ Runner只在您的私有网络中运行
- ✅ 只能访问您授权的仓库
- ✅ 所有代码在您自己的电脑上执行

### 推荐设置

```powershell
# 1. 设置专用用户账户（可选）
# 为Runner创建专门的Windows用户账户

# 2. 限制Runner权限
# 确保Runner只能访问必要的文件和目录

# 3. 定期更新Runner
# GitHub会定期发布Runner更新
```

## 📊 性能优化

### 构建加速

1. **SSD存储**: 确保工作目录在SSD上
2. **充足内存**: 推荐16GB+内存
3. **多核CPU**: UE构建会使用多核并行编译

### 磁盘空间管理

```powershell
# 定期清理构建产物
Remove-Item "C:\actions-runner\_work\*" -Recurse -Force

# 设置自动清理（在工作流中已包含）
```

## 🔄 维护和更新

### 定期维护

1. **每月检查Runner状态**
2. **更新UE版本**时，验证工作流仍正常工作
3. **定期清理**构建缓存和临时文件

### Runner更新

```powershell
# 停止服务
Stop-Service actions.runner.YOUR_MACHINE_NAME.YOUR_RUNNER_NAME

# 更新Runner（下载新版本并替换）
# 重新配置和启动

# 启动服务
Start-Service actions.runner.YOUR_MACHINE_NAME.YOUR_RUNNER_NAME
```

---

## 🎉 完成！

设置完成后，您就可以享受**在自己电脑上的GitHub Actions自动化构建**了！

- 🔥 **即时构建** - 无需等待下载UE
- 🎯 **三平台支持** - Win64+Linux+Mac一次构建
- 📦 **自动发布** - 推送标签自动创建Release
- 💯 **零成本** - 完全免费使用

有任何问题，请检查GitHub Actions的构建日志或参考故障排除部分。
