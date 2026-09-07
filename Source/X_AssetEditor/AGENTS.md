# X_AssetEditor - 资产批处理工具

Editor 模块，仅 Win64。批量碰撞/材质/命名操作。

## KEY CLASSES

| 类 | 职责 |
|----|------|
| `X_ModuleRegistrationManager` | EditorSubsystem，模块注册管理 |
| `X_MenuExtensionManager` | EditorSubsystem，菜单扩展管理 |
| `UX_CollisionBlueprintLibrary` | 碰撞设置批量操作 |
| `UX_AssetNamingBlueprintLibrary` | 资产命名规范化 |
| `UX_AssetFlattenLibrary` | 资产及依赖扁平移动、按类型归类移动，共用引用修复流程 |

## FEATURES

- **碰撞批处理**: 批量设置碰撞类型、自动凸包生成、碰撞设置对话框
- **材质函数**: 批量应用/管理材质函数参数
- **命名规范化**: 自动前缀、变体命名、数字后缀规范化、冲突检测
- **资产移动**: 递归收集 `/Game` 硬软包依赖，支持扁平与分类目录；入口、分类规则和验证范围见 [资产移动与按类型归类](../../Docs/开发文档/资产扁平移动.md)

## DEPENDENCIES

重度依赖 Editor 模块 (UnrealEd, ContentBrowser, AssetTools, PropertyEditor 等)。
打包时自动排除。

## GOTCHAS

- 仅 Win64 平台，Mac/Linux 不编译
- 必须在编辑器模式下使用，不可在 Runtime 调用
- 命名冲突检测使用 AssetRegistry 查询，大项目可能有延迟
- 资产移动统一走 `IAssetTools::RenameAssets` 与 `FixupReferencers`；只修复本次重定向器，不直接删除旧资产文件。执行可能部分成功，须检查结果和未保存引用包，不能承诺普通撤销恢复磁盘移动
- UE 5.4+ 原生重定向器报告需要交互；无界面模式保留待修复重定向器并报告。回归测试组：`XTools.AssetEditor.Flatten`
