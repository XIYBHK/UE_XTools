# X_AssetEditor - 资产批处理工具

Editor 模块，仅 Win64。批量碰撞/材质/命名操作。

## KEY CLASSES

| 类 | 职责 |
|----|------|
| `X_ModuleRegistrationManager` | EditorSubsystem，模块注册管理 |
| `X_MenuExtensionManager` | EditorSubsystem，菜单扩展管理 |
| `UX_CollisionBlueprintLibrary` | 碰撞设置批量操作 |
| `UX_AssetNamingBlueprintLibrary` | 资产命名规范化 |

## FEATURES

- **碰撞批处理**: 批量设置碰撞类型、自动凸包生成、碰撞设置对话框
- **材质函数**: 批量应用/管理材质函数参数
- **命名规范化**: 自动前缀、变体命名、数字后缀规范化、冲突检测

## DEPENDENCIES

重度依赖 Editor 模块 (UnrealEd, ContentBrowser, AssetTools, PropertyEditor 等)。
打包时自动排除。

## GOTCHAS

- 仅 Win64 平台，Mac/Linux 不编译
- 必须在编辑器模式下使用，不可在 Runtime 调用
- 命名冲突检测使用 AssetRegistry 查询，大项目可能有延迟
