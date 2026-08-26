# UE 5.3-5.8 多版本插件打包

本文是 XTools 的当前多版本打包操作手册。历史 CI 事故、个人路径和一次性构建编号不在长期文档中保留。

## 1. 推荐入口

在插件根目录执行：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 -Follow
```

脚本自动查找已安装的 UE 5.3-5.8，也可以显式传入引擎根目录：

```powershell
.\Scripts\BuildPlugin-MultiUE.ps1 -EngineRoots @(
    '<UE_5.3>'
    '<UE_5.4>'
    '<UE_5.5>'
    '<UE_5.6>'
    '<UE_5.7>'
    '<UE_5.8>'
) -Follow
```

## 2. 脚本参数

| 参数 | 用途 |
| --- | --- |
| `-EngineRoots` | 指定一个或多个 UE 安装根目录 |
| `-OutputBase` | 设置每个版本包的输出根目录 |
| `-TargetPlatforms` | 设置 BuildPlugin 目标平台 |
| `-StrictIncludes` | 在支持的版本启用严格 include 校验，默认开启 |
| `-NoHostProject` | 传递 UAT 的无宿主项目选项 |
| `-CleanOutput` | 构建前清理该版本的已验证输出目录 |
| `-Follow` | 实时输出构建日志 |

脚本默认输出到插件仓库上级的 `Plugin_Packages` 目录，并按 `XTools-UE_5.x` 分版本保存。

## 3. StrictIncludes 边界

脚本在 UE 5.4-5.8 启用 `-StrictIncludes`，用于发现隐式包含和模块边界问题。

UE 5.3 的 BuildPlugin 对引擎 UHT include 路径处理不同，脚本会跳过 `-StrictIncludes` 并输出提示。这是脚本的显式兼容策略，不代表 UE 5.3 可以依赖隐式包含；源码仍应遵守 IWYU。

## 4. BuildPlugin 验证范围

UAT `BuildPlugin` 会建立临时宿主并验证插件目标。对本插件，发布前至少关注：

- UnrealEditor Development：Runtime、Editor、UncookedOnly 和 UHT；
- UnrealGame Development：Runtime 依赖边界；
- UnrealGame Shipping：Shipping 宏、测试裁剪和运行时链接；
- Win64 包内容：描述文件、二进制、资源和必要配置；
- UE 5.3-5.8 每个版本的结果与日志。

本地单一 Editor Target 通过不能替代 BuildPlugin。

## 5. 输出检查

每个版本完成后检查：

1. 脚本退出码为 0；
2. 汇总表中该版本为成功；
3. 输出目录属于本次运行，而非旧产物；
4. 插件描述文件仍声明正确模块与依赖；
5. `Binaries` 和平台目录存在；
6. 日志没有被忽略的 UHT、IWYU、弃用或链接错误。

如需归档，将脚本输出目录作为整体压缩；不要混用不同提交或不同 UE 版本的二进制。

## 6. CI 矩阵

正式发布由 `.github/workflows/build-plugin-optimized.yml` 的 UE 5.3-5.8 矩阵裁决。标签构建前应确保：

- `XTools.uplugin`、版本宏和更新日志一致；
- 本地受影响 Target 与自动化测试通过；
- `git diff --check` 通过；
- 没有未解释的生成文件、测试资产或临时报告进入提交。

单版本出现故障时先定位具体引擎差异，不要全局恢复已证明无用的依赖。

## 7. 常见故障

### 7.1 IWYU / StrictIncludes

症状：较新版本找不到类型、模板或函数。

处理：

1. 找到符号的拥有头文件；
2. 在实际使用文件中显式 include；
3. 确认该头所属模块已在 `.Build.cs` 声明；
4. 重新运行最低失败版本和最高支持版本。

不要通过加入大型聚合头或无关模块掩盖缺失 include。

### 7.2 Runtime 引用 Editor API

症状：Editor 通过，Game/Shipping 失败。

处理：把 K2Node、资产编辑器、UnrealEd/Slate 编辑器逻辑移动到 Editor/UncookedOnly 模块；Runtime 只暴露运行时数据和函数。

`#if WITH_EDITOR` 只能裁剪同一模块内允许存在的编辑器辅助代码，不能修复错误的模块依赖方向。

### 7.3 模块依赖缺失

编译错误通常指向头文件，链接错误通常指向导出符号。两者都要根据符号归属添加最小依赖：

- 公共头暴露的类型通常需要 Public 依赖；
- 只在 `.cpp` 使用的类型通常使用 Private 依赖；
- 不因名称相似就加入 `Kismet`、`GraphEditor` 等大型编辑器模块。

### 7.4 DLL 或产物被占用

正式打包前关闭 Editor、Live Coding 和使用目标目录的进程。不要在无法确认目标的情况下递归删除中间目录。

### 7.5 旧产物污染

为每次验证使用明确输出目录，必要时使用脚本的 `-CleanOutput`。清理前确认解析后的目录位于预期输出根下。

### 7.6 跨版本 API 差异

把适配集中在 `Source/XToolsCore/Public/XToolsVersionCompat.h` 或最小调用点，不复制整套实现。参见 [跨版本条件编译](UE_跨版本条件编译_问题与解决方案.md)。

## 8. 推荐发布验证顺序

1. 最低支持版本 Editor Development；
2. 受影响的自动化测试前缀；
3. Game Development 与 Shipping；
4. 本地多版本 BuildPlugin 预检；
5. 标签触发 UE 5.3-5.8 CI 矩阵；
6. 核对 Release 资产对应提交和版本。

本地没有安装全部引擎时，可以依赖 CI 完成剩余矩阵，但必须明确本地实际覆盖的版本。
