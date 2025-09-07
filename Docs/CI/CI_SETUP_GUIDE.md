# XTools 插件 GitHub CI/CD 完整指南

基于您的现有PowerShell脚本，我为您创建了多个GitHub CI/CD工作流选项，支持UE 5.3-5.6和多平台构建。

## 🎯 工作流选择指南

### 方案对比

| 工作流 | 复杂度 | 平台支持 | UE版本 | 推荐场景 |
|--------|--------|----------|--------|----------|
| **Simple** | ⭐ | Win64 | 5.3-5.6 | 个人开发、快速验证 |
| **Complete** | ⭐⭐⭐ | Win/Linux/Mac | 5.3-5.6 | 商业项目、完整发布 |
| **Docker** | ⭐⭐ | Linux | 5.3-5.6 | 标准化环境、CI优化 |
| **Self-Hosted** | ⭐⭐ | 全平台 | 自定义 | 企业环境、性能要求高 |

## 🚀 推荐实施路径

### 阶段1：快速验证 (1-2天)
```bash
# 1. 启用简化版工作流
cp .github/workflows/build-plugin-simple.yml .github/workflows/build.yml

# 2. 提交代码测试
git add .github/workflows/build.yml
git commit -m "Add CI workflow"
git push origin main
```

### 阶段2：功能完善 (1周)
- 确认基础构建稳定后
- 根据需要启用完整版工作流
- 配置多平台构建环境

### 阶段3：生产就绪 (2周)
- 优化构建性能
- 配置自动发布
- 添加测试和质量检查

## 📋 各方案详细说明

### 1. 简化版工作流 (推荐首选)

**文件**: `.github/workflows/build-plugin-simple.yml`

**优势**:
- ✅ 配置简单，即开即用
- ✅ 基于您现有的PowerShell脚本逻辑
- ✅ 支持UE 5.3-5.6版本矩阵构建
- ✅ 无需额外环境配置

**使用条件**:
- 主要面向Windows用户
- 使用GitHub hosted runners
- 需要预安装UE (或使用自托管运行器)

**立即可用命令**:
```powershell
# 本地测试类似逻辑
cd Scripts
.\BuildPlugin-MultiUE.ps1 -Follow
```

### 2. 完整版工作流 (生产推荐)

**文件**: `.github/workflows/build-xtools-plugin.yml`

**优势**:
- ✅ 完整的多平台支持 (Windows/Linux/Mac)
- ✅ 动态构建矩阵配置
- ✅ 自动创建GitHub Releases
- ✅ 详细的构建报告

**使用场景**:
- 需要发布到多个平台
- 有完善的版本管理需求
- 团队协作开发

**配置要求**:
```yaml
# 根据您的实际安装路径，工作流会自动检测以下位置:
# 优先检测路径（您的实际安装）:
#   F:/Epic Games/UE_5.3
#   F:/Epic Games/UE_5.4  
#   F:/Epic Games/UE_5.5
#   F:/Epic Games/UE_5.6
#
# 备用路径:
#   D:/Program Files/Epic Games/UE_X.X
#   C:/Program Files/Epic Games/UE_X.X
#
# VS2022路径检测:
#   E:/VisualStudio/2022 (您的实际安装)
#   C:/Program Files/Microsoft Visual Studio/2022/*
```

### 3. Docker方案 (CI优化)

**文件**: `.github/workflows/build-plugin-docker.yml`

**优势**:
- ✅ 环境一致性高
- ✅ 无需预配置UE环境
- ✅ 资源利用率高

**限制**:
- ⚠️ 需要UE Docker镜像 (官方需要认证)
- ⚠️ 主要支持Linux构建
- ⚠️ 首次运行可能较慢

**Docker镜像选项**:
```bash
# 选项1: 构建自定义镜像
docker build -t ue5-builder:5.6 .

# 选项2: 使用社区镜像
# ghcr.io/epicgames/unreal-engine:5.6-dev (需要Epic账号授权)

# 选项3: 自托管镜像仓库
```

### 4. 自托管运行器方案 (企业级)

**优势**:
- ✅ 完全控制构建环境
- ✅ 性能最优
- ✅ 支持全平台
- ✅ 可以预安装所有UE版本

**配置步骤**:
```bash
# 1. 设置自托管运行器
# 在服务器上安装 GitHub Actions Runner

# 2. 安装UE环境
# 安装UE 5.3, 5.4, 5.5, 5.6到标准位置

# 3. 配置标签
# 在GitHub中添加runner标签: ue-builder
```

## 🔧 环境配置指南

### GitHub Hosted Runners 配置

如果使用GitHub提供的运行器，需要在每次构建时安装UE：

```yaml
# 选项1: 缓存UE安装（根据您的实际安装路径）
- name: Cache UE Installation
  uses: actions/cache@v4
  with:
    path: |
      F:\Epic Games\
      E:\VisualStudio\2022\
    key: ue-${{ matrix.ue_version }}-windows-f-drive

# 选项2: 使用预构建镜像
# (需要自定义runners或第三方服务)
```

### 自托管运行器配置

推荐的服务器配置：
```
操作系统: Windows Server 2019/2022
CPU: 8核以上
内存: 32GB以上  
存储: 1TB SSD
网络: 高带宽连接

预装软件:
- Visual Studio 2022 (C++ workload) - 推荐安装到E:\VisualStudio\2022\
- Unreal Engine 5.3, 5.4, 5.5, 5.6 - 推荐安装到F:\Epic Games\UE_X.X\
- Git
- PowerShell 7+
```

## 📊 构建产物对比

### 与您现有脚本的对应关系

```
您的本地脚本输出:
Plugin_Packages/
├── XTools-UE_5.4/
├── XTools-UE_5.5/
└── XTools-UE_5.6/

CI工作流输出:
Artifacts/
├── XTools-UE_5.3-Win64.zip
├── XTools-UE_5.4-Win64.zip
├── XTools-UE_5.5-Win64.zip
└── XTools-UE_5.6-Win64.zip
```

### 自动发布功能

当推送版本标签时 (如 `v1.0.0`)：
```bash
# 自动创建GitHub Release，包含:
XTools-v1.0.0-UE_5.3-Win64.zip
XTools-v1.0.0-UE_5.4-Win64.zip
XTools-v1.0.0-UE_5.5-Win64.zip
XTools-v1.0.0-UE_5.6-Win64.zip
XTools-v1.0.0-All-Platforms.zip  # 完整包
```

## ⚙️ 路径配置重要说明

### 工作流自动路径检测

我们的工作流已经根据您的实际安装环境进行了优化配置：

**UE安装路径检测优先级**：
1. 🎯 `F:\Epic Games\UE_X.X\` - **您的实际安装路径（最高优先级）**
2. 📁 `D:\Program Files\Epic Games\UE_X.X\` - 备用路径
3. 📁 `C:\Program Files\Epic Games\UE_X.X\` - 标准路径

**VS2022安装路径检测优先级**：
1. 🎯 `E:\VisualStudio\2022\` - **您的实际安装路径（最高优先级）**
2. 📁 `C:\Program Files\Microsoft Visual Studio\2022\Community\`
3. 📁 `C:\Program Files\Microsoft Visual Studio\2022\Professional\`
4. 📁 `C:\Program Files\Microsoft Visual Studio\2022\Enterprise\`

### 路径配置验证

构建开始时，您会在日志中看到：
```
🎯 Using UE installation at: F:\Epic Games\UE_5.6
🔧 Found VS2022 at: E:\VisualStudio\2022
```

如果路径检测失败，请确认：
- ✅ UE和VS的安装路径正确
- ✅ 工作流文件中的路径检测逻辑包含您的安装路径
- ✅ Self-Hosted Runner有权限访问这些目录

## 🎯 实施建议

### 立即开始
1. **复制简化版工作流**到您的仓库
2. **提交并推送**，观察构建结果
3. **根据构建日志**调整配置

### 后续优化
1. **监控构建时间**，优化性能瓶颈
2. **添加测试步骤**，确保插件质量
3. **配置通知**，及时了解构建状态

### 问题排查
如遇到问题，请检查：
1. **UE安装路径**是否正确
2. **插件文件**是否在预期位置
3. **构建日志**中的详细错误信息

## 📞 支持

如有问题，可以：
1. 查看 `.github/README.md` 中的详细文档
2. 检查GitHub Actions的构建日志
3. 参考您现有的 `Scripts/README.md` 文档
4. 对比本地PowerShell脚本的成功执行

---

**总结**: 建议从简化版工作流开始，验证基础功能后再根据需要升级到完整版。这样可以最快实现CI/CD目标，同时保持与您现有构建脚本的一致性。
