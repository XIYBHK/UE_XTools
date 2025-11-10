# XTools_SwitchLanguage 模块

## 概述

XTools_SwitchLanguage 是 XTools 插件的语言切换模块，提供运行时动态切换本地化语言的功能。该模块采用 XTools 前缀命名以避免与其他模块冲突。

## 功能特性

- **多语言支持**: 支持英语、中文、日语、韩语、法语、德语、西班牙语、俄语
- **自动检测**: 可自动检测并切换到系统首选语言
- **文化代码**: 支持通过文化代码（如 "zh-Hans", "en"）直接切换语言
- **语言历史**: 记录最近的语言切换历史
- **诊断工具**: 提供本地化系统诊断信息
- **蓝图友好**: 所有功能都通过蓝图函数库暴露

## 蓝图节点

### 核心功能
- **切换语言**: 切换到指定的枚举语言
- **切换语言（文化代码）**: 通过文化代码切换语言
- **重置为系统语言**: 恢复到系统首选语言

### 查询功能
- **获取当前语言**: 获取当前使用的语言枚举
- **获取当前文化代码**: 获取当前的文化代码
- **获取支持的语言列表**: 获取所有可用语言的详细信息
- **检查语言是否可用**: 检查指定语言是否可用

### 转换功能
- **语言枚举转文化代码**: 将枚举转换为文化代码
- **文化代码转语言枚举**: 将文化代码转换为枚举

### 系统功能
- **获取系统首选语言**: 获取系统首选的语言枚举
- **获取系统首选文化代码**: 获取系统首选的文化代码

### 历史管理
- **获取语言切换历史**: 获取最近的切换历史
- **清空语言切换历史**: 清空历史记录

### 诊断工具
- **获取本地化诊断信息**: 获取详细的系统诊断信息
- **刷新本地化资源**: 强制刷新本地化资源

## 使用示例

### 基本语言切换
```cpp
// 切换到中文
FLanguageSwitchResult Result = UXTools_SwitchLanguageLibrary::SwitchLanguage(ESupportedLanguage::Chinese);
if (Result.bSuccess)
{
    UE_LOG(LogTemp, Log, TEXT("语言切换成功: %s"), *Result.CultureName);
}
```

### 通过文化代码切换
```cpp
// 切换到日语
FLanguageSwitchResult Result = UXTools_SwitchLanguageLibrary::SwitchLanguageByCulture(TEXT("ja"));
```

### 获取当前语言信息
```cpp
ESupportedLanguage CurrentLang = UXTools_SwitchLanguageLibrary::GetCurrentLanguage();
FString CultureName = UXTools_SwitchLanguageLibrary::GetCurrentCultureName();
```

### 获取支持的语言列表
```cpp
TArray<FLanguageInfo> SupportedLanguages = UXTools_SwitchLanguageLibrary::GetSupportedLanguages();
for (const FLanguageInfo& LangInfo : SupportedLanguages)
{
    if (LangInfo.bIsAvailable)
    {
        UE_LOG(LogTemp, Log, TEXT("可用语言: %s (%s)"), 
            *LangInfo.DisplayName.ToString(), *LangInfo.CultureCode);
    }
}
```

## 支持的语言

| 枚举值 | 文化代码 | 显示名称 |
|--------|----------|----------|
| English | en | English |
| Chinese | zh-Hans | 中文 |
| Japanese | ja | 日本語 |
| Korean | ko | 한국어 |
| French | fr | Français |
| German | de | Deutsch |
| Spanish | es | Español |
| Russian | ru | Русский |
| Auto | - | Auto |

## 技术细节

### 模块依赖
- **XToolsCore** (版本兼容层和通用框架)
  - 提供统一的错误/日志处理
  - 跨版本兼容性支持
  - 通用宏定义
- **Core** (包含国际化功能和基础系统)
- **CoreUObject** (UObject 系统)
- **Engine** (引擎核心功能)

### 平台支持
- Windows 64位
- macOS
- Linux
- Android
- iOS

### API 前缀
- 类前缀: `XTools_SwitchLanguage` 或 `UXTools_SwitchLanguage`
- 函数前缀: `XTOOLS_SWITCHLANGUAGE_API`
- 宏定义: `WITH_XTOOLS_SWITCHLANGUAGE`

### 本地化资源
模块本身使用 LOCTEXT 进行本地化，支持通过 UE 本地化仪表板进行翻译。

## 命名规范

为了避免与参考模块 `SwitchLanguage` 产生冲突，本模块采用以下命名规范：

- **模块名**: `XTools_SwitchLanguage`
- **类名**: `UXTools_SwitchLanguageLibrary`, `FXTools_SwitchLanguageModule`
- **文件名**: `XTools_SwitchLanguage.*`
- **API 导出**: `XTOOLS_SWITCHLANGUAGE_API`
- **宏定义**: `WITH_XTOOLS_SWITCHLANGUAGE`

## 注意事项

1. **资源可用性**: 语言切换成功与否取决于对应的本地化资源是否可用
2. **运行时切换**: 切换后需要调用 `刷新本地化资源` 以确保 UI 更新
3. **历史记录**: 语言切换历史最多保存 10 条记录
4. **自动语言**: 选择 "Auto" 会自动切换到系统首选语言
5. **模块冲突**: 通过 XTools 前缀避免与其他同名模块冲突

## 故障排除

### 语言切换失败
- 检查本地化资源是否已正确部署
- 使用 `获取本地化诊断信息` 查看详细状态
- 确保目标文化代码在系统中可用

### UI 未更新
- 调用 `刷新本地化资源` 强制更新
- 检查 UI 组件是否使用了 FText 而非 FString
- 确保本地化资源已编译

### 编译错误
- 确保模块已在 XTools.uplugin 中正确注册
- 检查依赖模块是否正确配置
- 验证所有文件名和类名使用了正确的 XTools 前缀

## 版本信息

- 版本: 1.0.0
- 兼容 UE 版本: 5.3-5.6
- 依赖 XToolsCore: 1.9.1+
- 模块类型: Runtime
