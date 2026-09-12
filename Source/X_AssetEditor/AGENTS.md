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
- **蓝图 AI 导出**: 总目录/资产入口按图提供 ReadPack v2 伪代码、内容清单与按需证据，`05_Query.py` 离线读取预算受限的入口子图；目录按完整资产路径摘要隔离，`90_Full/` 保留原 JSON、`.ai.md`、`.md`。`XBlueprintReadPack::Build` 纯消费 JSON，设计兼容 UE 5.3–5.8，不依赖 5.8 MCP。格式及回归入口见 [蓝图 AI 逻辑导出](../../Docs/开发文档/蓝图AI逻辑导出.md)
- **资产移动**: 递归收集 `/Game` 硬软包依赖，支持扁平与分类目录；入口、分类规则和验证范围见 [资产移动与按类型归类](../../Docs/开发文档/资产扁平移动.md)

## DEPENDENCIES

重度依赖 Editor 模块 (UnrealEd, ContentBrowser, AssetTools, PropertyEditor 等)。
打包时自动排除。

## GOTCHAS

- 仅 Win64 平台，Mac/Linux 不编译
- 必须在编辑器模式下使用，不可在 Runtime 调用
- 命名冲突检测使用 AssetRegistry 查询，大项目可能有延迟
- 资产移动统一走 `IAssetTools::RenameAssets` 与 `FixupReferencers`；只修复本次重定向器，不直接删除旧资产文件。执行可能部分成功，须检查结果和未保存引用包，不能承诺普通撤销恢复磁盘移动
- 资产移动的源、目标与递归依赖均限制在 `/Game`；引擎/插件依赖保留原位，显式选中时跳过并报告，混选继续处理项目资产；全选外部资产时提示无可移动项目资产。目录 UI 使用 `CreatePathPicker` 与 `/Game` 允许列表，蓝图接口保留相同后端检查
- UE 5.4+ 原生重定向器报告需要交互；无界面模式保留待修复重定向器并报告。回归测试组：`XTools.AssetEditor.Flatten`
