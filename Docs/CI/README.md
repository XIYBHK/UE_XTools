# XTools Plugin GitHub CI/CD 配置

本文档详细说明了如何为 XTools UE插件设置和使用基于**Self-Hosted Runner**的 GitHub CI/CD 工作流。

## 📁 文件结构

```
.github/
├── workflows/
│   ├── build-xtools-plugin.yml      # 完整版CI工作流 (三平台支持)
│   ├── build-plugin-simple.yml      # 简化版CI工作流 (三平台支持)  
│   └── build-plugin-docker.yml      # Docker版CI工作流 (可选)
└── README.md                        # 本文档
```

## 🚀 快速开始

### ⚡ 前提条件：设置Self-Hosted Runner

**重要**：所有工作流都需要在您的本地电脑上运行，首先必须设置GitHub Self-Hosted Runner！

#### 快速设置步骤：
```powershell
# 1. 以管理员权限打开PowerShell
mkdir C:\actions-runner; cd C:\actions-runner

# 2. 按照GitHub页面提示下载和配置Runner
# 进入 Settings > Actions > Runners > New self-hosted runner

# 3. 设置为Windows服务（推荐）
.\svc.cmd install
.\svc.cmd start
```

### 选项1: 简化版工作流 (推荐开始使用) ⭐

**支持UE 5.3-5.6三平台构建**（Win64+Linux+Mac）：

1. ✅ 确保Self-Hosted Runner正在运行
2. ✅ 在GitHub Actions页面手动触发 `Build XTools Plugin (Simplified)`
3. ✅ 构建将在您的电脑上自动执行，生成三平台插件

### 选项2: 完整版工作流 (生产环境推荐)

**支持自定义参数的完整构建**：

1. ✅ 确保Self-Hosted Runner正在运行
2. ✅ 手动触发 `Build XTools Plugin`，可配置：
   - UE版本组合（5.3,5.4,5.5,5.6）
   - 平台组合（Win64+Linux+Mac）
   - 清理构建选项
3. ✅ 推送版本标签（如v1.0.0）自动创建GitHub Release

## ⚙️ Self-Hosted Runner 环境要求

### 本地电脑配置

#### Windows 环境（必需）
```powershell
# 核心软件
- Windows 10/11 (推荐)
- Visual Studio 2019/2022 (包含C++工作负载)
- Unreal Engine 5.3, 5.4, 5.5, 5.6 (至少一个版本)

# 支持的UE安装路径（按优先级检测）
1. D:\Program Files\Epic Games\UE_X.X\     # 优先
2. C:\Program Files\Epic Games\UE_X.X\     # 标准
3. F:\ProgramFiles\UE_X.X\                 # 备用

# ⚡ 关键：启用UE交叉编译支持
在Epic Launcher安装UE时，必须勾选：
✅ Linux Cross-Compilation Support

# PowerShell执行策略
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# 验证交叉编译工具链
ls "C:\UnrealToolchains\*"  # 应该看到clang相关文件
```

#### 硬件推荐
```
CPU: 8核+ (UE构建CPU密集)
内存: 16GB+ (推荐32GB)
存储: SSD (构建速度显著提升)
网络: 稳定的宽带连接
```

### GitHub Actions Runner
```powershell
# Runner安装位置
C:\actions-runner\

# 推荐设置为Windows服务
.\svc.cmd install    # 安装服务
.\svc.cmd start      # 启动服务
.\svc.cmd status     # 检查状态

# 服务会在系统重启后自动启动
```

## 🔧 Self-Hosted Runner 工作流详解

### 触发条件

**重要变更**：为避免意外触发，所有工作流都改为**仅手动触发**：

1. **手动触发**: GitHub Actions页面点击 "Run workflow"（主要方式）
2. **版本标签**: 推送标签如 `v1.0.0` 自动创建Release（仅完整版）

### 三平台构建支持 🌍

所有工作流现在都支持**一条命令构建三平台**，类似UE商城插件：
```powershell
# 核心构建命令
RunUAT.bat BuildPlugin -TargetPlatforms=Win64+Linux+Mac
```

### 构建参数

#### 简化版工作流 (`build-plugin-simple.yml`)
- **UE版本**: 5.3, 5.4, 5.5, 5.6（矩阵构建，4个版本并行）
- **平台**: Win64+Linux+Mac（三平台一次性构建）
- **配置**: Development
- **输出**: `XTools-UE_X.X-AllPlatforms.zip`（包含三平台）

#### 完整版工作流 (`build-xtools-plugin.yml`)
手动触发时可配置参数：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ue_versions` | `5.3,5.4,5.5,5.6` | UE版本（逗号分隔）|
| `platforms` | `Win64+Linux+Mac` | 平台（+号连接）|  
| `clean_build` | `true` | 清理构建缓存 |

#### Docker工作流 (`build-plugin-docker.yml`)
- 可选的容器化构建方案
- 同样运行在Self-Hosted Runner上
- 适合需要隔离环境的场景

### 构建产物 📦

构建完成后会生成**三平台合并产物**：

```
XTools-UE_X.X-AllPlatforms/
├── XTools.uplugin           # 插件描述文件
├── Binaries/                # 三平台二进制文件
│   ├── Win64/              # Windows平台
│   │   └── UE4Editor-XTools.dll
│   ├── Linux/              # Linux平台
│   │   └── libUE4Editor-XTools.so
│   ├── Mac/                # Mac平台 (如果启用)
│   │   └── libUE4Editor-XTools.dylib
│   └── ThirdParty/         # 第三方运行时库 (如果有)
│       ├── Win64/          # Windows第三方DLL
│       ├── Linux/          # Linux第三方SO
│       └── Mac/            # Mac第三方DYLIB
├── Content/                 # 内容资源
├── Resources/              # 插件资源
├── Source/                 # 源代码 (如果包含)
└── ThirdParty/             # 第三方库完整包 (如果有)
    ├── LibraryName/        # 第三方库目录
    │   ├── Include/        # 头文件
    │   ├── Lib/           # 静态库文件
    │   │   ├── Win64/     # Windows .lib文件
    │   │   ├── Linux/     # Linux .a文件  
    │   │   └── Mac/       # Mac .a文件
    │   └── bin/           # 动态库文件
    │       ├── Win64/     # Windows .dll文件
    │       ├── Linux/     # Linux .so文件
    │       └── Mac/       # Mac .dylib文件
    └── [其他第三方库...]
```

**下载的Artifacts**:
- `XTools-UE_5.3-AllPlatforms.zip` 🎯
- `XTools-UE_5.4-AllPlatforms.zip` 🎯  
- `XTools-UE_5.5-AllPlatforms.zip` 🎯
- `XTools-UE_5.6-AllPlatforms.zip` 🎯

每个ZIP都是**完整的三平台插件**，可在任意平台使用！

### 第三方库支持说明 📚

**BuildPlugin 自动处理第三方库**：
- ✅ **静态库**：自动链接到插件二进制文件中
- ✅ **动态库**：复制到 `Binaries/ThirdParty/` 对应平台目录
- ✅ **头文件**：保留在 `ThirdParty/LibraryName/Include/` 
- ✅ **跨平台支持**：每个平台的库文件都会正确包含（如果存在）
- ✅ **运行时依赖**：动态库会随插件一起分发
- ⚠️ **平台限制**：某些第三方库可能只支持特定平台

**插件中第三方库的标准结构**：
```
YourPlugin/
├── Source/
│   └── YourPlugin/
│       └── YourPlugin.Build.cs  # 在这里配置第三方库
└── ThirdParty/
    └── SomeLibrary/
        ├── Include/            # 头文件
        └── Lib/               # 预编译库
            ├── Win64/         # Windows库文件
            ├── Linux/         # Linux库文件
            └── Mac/           # Mac库文件
```

**XTools实际案例：CoACD第三方库配置**：
```csharp
// X_AssetEditor.Build.cs - 实际的第三方库配置示例
public class X_AssetEditor : ModuleRules
{
    public X_AssetEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        // ... 基本配置 ...

        // ✅ UE最佳实践：第三方DLL运行时依赖配置
        string CoACDDllPath = Path.Combine(PluginDirectory, "ThirdParty", "CoACD", "DLL", "lib_coacd.dll");
        if (File.Exists(CoACDDllPath))
        {
            // 添加运行时依赖，确保DLL被正确打包和部署
            RuntimeDependencies.Add(CoACDDllPath);
            
            // 添加延迟加载，符合UE性能最佳实践
            PublicDelayLoadDLLs.Add("lib_coacd.dll");
            
            // 标记为第三方库，启用特殊处理
            PublicDefinitions.Add("WITH_COACD_DLL=1");
        }
        else
        {
            // 开发时警告
            PublicDefinitions.Add("WITH_COACD_DLL=0");
        }
    }
}
```

**通用的跨平台第三方库配置模板**：
```csharp
// 通用模板 - 支持多平台第三方库
public class YourPlugin : ModuleRules
{
    public YourPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        // 添加第三方库包含路径
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ThirdParty", "SomeLibrary", "Include"));
        
        // 根据平台配置库文件
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string DllPath = Path.Combine(PluginDirectory, "ThirdParty", "SomeLibrary", "DLL", "SomeLibrary.dll");
            if (File.Exists(DllPath))
            {
                RuntimeDependencies.Add(DllPath);
                PublicDelayLoadDLLs.Add("SomeLibrary.dll");
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string SoPath = Path.Combine(PluginDirectory, "ThirdParty", "SomeLibrary", "Lib", "Linux", "libSomeLibrary.so");
            if (File.Exists(SoPath))
            {
                PublicAdditionalLibraries.Add(SoPath);
                RuntimeDependencies.Add("$(BinaryOutputDir)/libSomeLibrary.so", SoPath);
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string DylibPath = Path.Combine(PluginDirectory, "ThirdParty", "SomeLibrary", "Lib", "Mac", "libSomeLibrary.dylib");
            if (File.Exists(DylibPath))
            {
                PublicAdditionalLibraries.Add(DylibPath);
                RuntimeDependencies.Add("$(BinaryOutputDir)/libSomeLibrary.dylib", DylibPath);
            }
        }
    }
}
```

### 实际的XTools构建产物 📦

基于您的CoACD第三方库，实际的构建产物结构为：
```
XTools-UE_X.X-AllPlatforms/
├── XTools.uplugin
├── Binaries/
│   ├── Win64/                    # Windows平台完整支持
│   │   ├── UE4Editor-XTools.dll
│   │   ├── UE4Editor-X_AssetEditor.dll
│   │   └── lib_coacd.dll         # CoACD第三方库
│   ├── Linux/                    # Linux平台（CoACD功能受限）
│   │   ├── libUE4Editor-XTools.so
│   │   └── libUE4Editor-X_AssetEditor.so
│   └── Mac/                      # Mac平台（CoACD功能受限）
│       ├── libUE4Editor-XTools.dylib
│       └── libUE4Editor-X_AssetEditor.dylib
├── Content/
├── Resources/
└── ThirdParty/
    └── CoACD/
        ├── CREDITS.md            # 第三方库归属信息
        ├── LICENSE               # MIT许可证
        └── DLL/
            └── lib_coacd.dll     # 仅Windows版本可用
```

**跨平台兼容性说明**：
- 🟢 **Windows**: 完整功能，包含CoACD智能凸包分解
- 🟡 **Linux/Mac**: 基础功能正常，CoACD功能在代码中优雅降级
- ✅ **条件编译**: 使用 `WITH_COACD_DLL` 宏确保跨平台兼容

## 📊 Self-Hosted Runner 使用示例

### 完整工作流程

1. **开发阶段**: 在本地开发插件，使用 `Scripts\BuildPlugin-MultiUE.ps1` 快速测试
2. **构建阶段**: 在GitHub页面手动触发CI构建，**在您的电脑上自动执行**
3. **验证阶段**: 下载三平台构建产物进行跨平台测试
4. **发布阶段**: 推送版本标签，自动创建包含所有平台的GitHub Release

### 手动触发构建

在 GitHub 仓库页面：
1. 进入 `Actions` 标签页
2. 选择工作流：
   - `Build XTools Plugin (Simplified)` 🚀 推荐日常使用
   - `Build XTools Plugin` ⚙️ 需要自定义参数时
3. 点击 `Run workflow` 按钮
4. 配置构建参数（完整版支持）
5. 点击绿色 `Run workflow` 确认
6. **构建立即在您的本地电脑开始执行！**

### 实时监控

由于构建在本地运行，您可以：
- 👀 在GitHub查看构建日志
- 💻 在本地电脑看到实际构建过程  
- 🔧 构建失败时立即本地调试
- ⚡ 享受无需下载UE的超快构建速度

### 下载构建产物

构建完成后：
1. 进入对应的构建任务页面
2. 向下滚动到 `Artifacts` 部分  
3. 下载需要的平台/版本组合
4. 解压到UE项目的 `Plugins` 目录

## 🔍 故障排除

### 常见问题

#### 1. Self-Hosted Runner 离线
**错误**: `No online and idle self-hosted runners available`
**解决**: 
```powershell
# 检查Runner服务状态
cd C:\actions-runner
.\svc.cmd status

# 重启服务
.\svc.cmd stop
.\svc.cmd start

# 检查GitHub页面Runner状态是否为绿色"Online"
```

#### 2. UE版本找不到
**错误**: `UE X.X not found in any of these locations`
**解决**:
```powershell
# 检查UE安装路径
ls "D:\Program Files\Epic Games\" | findstr UE_
ls "C:\Program Files\Epic Games\" | findstr UE_
ls "F:\ProgramFiles\" | findstr UE_

# 确保至少有一个版本在支持的路径中
```

#### 3. 交叉编译失败
**错误**: Linux或Mac平台构建失败
**解决**:
```powershell
# 检查交叉编译工具链
ls "C:\UnrealToolchains\"

# 如果为空，在Epic Launcher中：
# 1. 选择UE版本 > Options > Target Platforms
# 2. 勾选 "Linux Cross-Compilation Support"
# 3. 重新安装或添加组件
```

#### 3. 构建失败
**错误**: 插件编译错误
**解决**:
- 检查插件代码是否兼容目标UE版本
- 查看详细的构建日志定位具体错误
- 确认所有依赖项都已正确配置

#### 4. 产物为空
**错误**: 构建成功但无产物上传
**解决**:
- 检查输出路径配置
- 验证构建过程是否实际完成
- 查看文件权限和路径映射

### 调试技巧

1. **启用详细日志**:
   ```yaml
   env:
     UAT_VERBOSE: true
     UE_LOG_LEVEL: Verbose
   ```

2. **保留构建目录**:
   ```yaml
   - name: Upload Build Logs
     if: failure()
     uses: actions/upload-artifact@v4
     with:
       name: build-logs
       path: |
         **/Saved/Logs/
         **/Intermediate/
   ```

3. **SSH调试**:
   对于复杂问题，可以使用 `action-tmate` 进行远程调试：
   ```yaml
   - name: Setup tmate session
     if: failure()
     uses: mxschmitt/action-tmate@v3
   ```

## 🎯 Self-Hosted Runner 最佳实践

### 1. Runner 管理
```powershell
# 推荐设置为Windows服务
.\svc.cmd install      # 安装服务
.\svc.cmd start        # 启动服务
.\svc.cmd status       # 检查状态

# 服务会随系统启动，无需手动管理
```

### 2. 性能优化
- **SSD存储**: 确保Runner工作目录在SSD (`C:\actions-runner\_work\`)
- **充足内存**: 推荐16GB+，UE构建内存密集
- **多核CPU**: UE会自动利用所有可用核心
- **稳定网络**: 保持与GitHub的连接稳定

### 3. 磁盘空间管理
```powershell
# 构建产物会自动清理，但可定期手动清理
Remove-Item "C:\actions-runner\_work\*" -Recurse -Force

# 监控磁盘使用
Get-PSDrive C | Select-Object Used,Free
```

### 4. 版本发布流程
```bash
# 推荐的发布流程
1. 本地开发测试完成
2. 手动触发GitHub构建验证
3. 下载产物进行最终测试
4. 推送版本标签触发自动发布：
   git tag v1.0.0
   git push origin v1.0.0
5. GitHub自动创建包含三平台的Release
```

## 📈 监控和维护

### 构建监控
1. 启用GitHub Actions的邮件通知
2. 配置Slack/Discord集成接收构建状态
3. 定期检查构建日志和性能指标

### 定期维护
1. 更新UE版本支持
2. 优化构建时间和资源使用
3. 更新依赖的GitHub Actions版本

---

## 💡 Self-Hosted Runner 使用提示

### 首次设置
1. **先验证本地构建**: 运行 `Scripts\BuildPlugin-MultiUE.ps1` 确认三平台构建正常
2. **分步测试**: 先设置Runner → 测试简化版工作流 → 确认成功后使用完整版
3. **检查工具链**: 确认Linux交叉编译支持已安装

### 日常使用
- **构建前确认**: 足够的磁盘空间和内存
- **避免冲突**: 构建期间避免在本地同时运行重型任务
- **网络稳定**: 构建期间保持网络连接
- **及时清理**: 定期清理构建缓存

### 与本地脚本的协作
- **互补使用**: Self-Hosted Runner适合正式发布，本地脚本适合快速迭代
- **完全一致**: 使用相同的构建逻辑和参数
- **灵活选择**: 根据需要选择手动本地构建或自动化CI构建

### 优势总结
✅ **零等待时间** - 使用本地预装的UE环境  
✅ **三平台支持** - Win64+Linux+Mac一次性构建完成  
✅ **完全可控** - 在您自己的安全环境中执行  
✅ **即时调试** - 构建失败可立即本地排查  
✅ **专业发布** - 自动生成符合UE商城标准的三平台插件包  

---

**🎉 现在您拥有了专业级的UE插件CI/CD流程！**

如有问题，请查看GitHub Actions的详细构建日志或提交Issue。
