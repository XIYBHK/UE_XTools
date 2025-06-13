UE 编辑器扩展核心技术概览
版本：UE 5.x

本文档旨在提供一个关于Unreal Engine编辑器扩展和C++插件开发中常用核心技术的概览。它将K2Node开发置于更广阔的编辑器编程背景下。

1. 插件与模块化 (.uplugin, Build.cs)
核心概念: UE的功能通过模块(Module)组织，多个模块可以打包成一个插件(Plugin)。

.uplugin 文件: 定义插件的元数据，包括它包含的模块、加载时机（如 Editor, Runtime）和依赖项。

Build.cs 文件: 每个模块都有一个 Build.cs 文件，用C#编写，用于控制该模块的编译过程。

PublicDependencyModuleNames: 公开依赖，会传递给依赖此模块的其他模块。

PrivateDependencyModuleNames: 私有依赖，仅在此模块内部使用。

if (Target.bBuildEditor): 关键实践。使用此条件块来添加仅编辑器需要的模块依赖（如 UnrealEd, BlueprintGraph, Slate），从而确保运行时模块保持轻量。

2. 自定义蓝图节点 (K2Node)
用途: 创建具有复杂逻辑、动态引脚和自定义UI的蓝图节点。

关键技术:

生命周期管理: AllocateDefaultPins, ReconstructNode。

编译时转换: ExpandNode 将自身转译为底层函数调用。

高级技巧: 利用C++反射动态调整节点行为，使用CustomThunk支持泛型，通过Slate自定义UI。

在生态中的位置: K2Node是连接C++后端逻辑与蓝图前端用户界面的最强大桥梁之一。

3. Slate UI框架
用途: 构建Unreal Editor的所有用户界面，从窗口、面板到按钮和引脚。

核心概念:

声明式语法: 使用 SNew 和 SAssignNew 以类似代码的方式构建UI层级。

Widgets: SButton, STextBlock, SListView, SComboBox 等是构成UI的基本组件。

SCompoundWidget: 用于将多个简单Widget组合成一个可复用的复杂控件。

应用场景:

创建独立的编辑器工具窗口 (FGlobalTabmanager::Get()->RegisterNomadTabSpawner)。

为K2Node创建自定义的引脚控件 (SGraphPin)。

自定义Actor或资产的详情面板。

4. 详情面板自定义 (Details Customization)
用途: 修改和扩展选中Actor或资产后在“细节”面板中显示的属性UI。

核心接口:

IDetailCustomization: 用于自定义整个类的详情面板布局。可以添加全新的分类、按钮和自定义逻辑。

IPropertyTypeCustomization: 用于自定义特定属性类型（如一个FVector或你自定义的结构体FMyStruct）的显示方式。

5. 自定义资产与AssetTools
用途: 创建全新的资产类型，并使其能够被内容浏览器识别和操作。

核心接口: FAssetTypeActions_Base

GetName(): 资产的显示名称。

GetSupportedClass(): 此Action对应的UObject类。

GetAssetColor(): 在内容浏览器中显示的颜色。

OpenAssetEditor(): 双击资产时触发的操作。

GetActions(): 定义右键菜单中的自定义操作。

实现流程: 创建 FAssetTypeActions_Base 的子类，并在编辑器模块启动时通过 IAssetTools::RegisterAssetTypeActions 进行注册。

6. 编辑器子系统 (UEditorSubsystem)
用途: 提供一种官方推荐的方式来创建具有编辑器生命周期的单例服务。

优势: 自动初始化和反初始化，易于访问 (GEditor->GetEditorSubsystem<UMyEditorSubsystem>())，避免了手动管理全局单例的麻烦。

应用场景: 管理全局编辑器状态、注册回调、提供工具服务等。

7. Component Visualizer
用途: 在编辑器视口中为UActorComponent绘制自定义的Gizmo或调试信息。

核心接口: FComponentVisualizer

DrawVisualization(): 每帧在视口中绘制。

HandleInputDelta(): 处理Gizmo的拖拽。

VisProxyHandleClick(): 处理Gizmo的点击。

应用场景: 为自定义的寻路组件绘制路径，为触发器组件显示其范围等。

8. Commandlets
用途: 创建可以通过命令行执行的、无UI的批量处理工具。

核心概念: 创建 UCommandlet 的子类，并重写 Main 函数。

启动方式: UEEditor.exe <Project> -run=<CommandletName> [args...]

应用场景: 批量导入/导出资源、数据验证、离线烘焙、生成项目报告等。

9. 自动化测试 (AutomationSpec)
用途: 为插件和编辑器功能编写单元测试和功能测试。

框架: 使用BDD（行为驱动开发）风格的Describe/It语法。

关键宏:

ADD_LATENT_AUTOMATION_COMMAND: 用于处理需要等待（如加载地图）的异步测试流程。

运行: 通过Session Frontend中的Automation标签页运行测试。