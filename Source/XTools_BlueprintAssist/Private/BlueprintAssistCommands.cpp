// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistCommands.h"

#include "BlueprintAssistGlobals.h"

#define LOCTEXT_NAMESPACE "BlueprintAssistCommands"

void FBACommandsImpl::RegisterCommands()
{
	UI_COMMAND(
		OpenContextMenu,
		"打开蓝图创建菜单",
		"为选中的引脚打开蓝图创建菜单",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Tab));

	UI_COMMAND(
		ReplaceNodeWith,
		"替换节点",
		"打开蓝图创建菜单以替换当前节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::H));

	UI_COMMAND(
		RenameSelectedNode,
		"重命名选中节点",
		"重命名图表中选中的变量、宏或函数",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::F2));

	UI_COMMAND(
		EditNodeComment,
		"编辑节点注释",
		"编辑选中节点的注释气泡文本",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::F2));

	UI_COMMAND(
		FormatNodes,
		"格式化节点",
		"自动定位所有连接的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::F));

	UI_COMMAND(
		FormatNodes_Selectively,
		"选择性格式化节点",
		"仅格式化选中的节点,如果只选中1个节点,则格式化右侧的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::F));

	UI_COMMAND(
		FormatNodes_Helixing,
		"使用螺旋格式化节点",
		"强制使用螺旋设置并格式化节点",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		FormatNodes_LHS,
		"使用左侧格式化节点",
		"强制使用左侧设置并格式化节点",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		DeleteAndLink,
		"删除并保持连接",
		"删除链中A-B-C的节点B并连接A-C",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::Delete));
	
	UI_COMMAND(
		CutAndLink,
		"剪切并保持连接",
		"剪切链中A-B-C的节点B并连接A-C",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::X));

	UI_COMMAND(
		LinkNodesBetweenWires,
		"在连线间链接节点",
		"将选中的节点插入到高亮显示的连线之间",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Q));

	UI_COMMAND(
		ConnectUnlinkedPins,
		"连接未链接的引脚",
		"尝试将任何未链接的引脚连接到附近的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Q));

	UI_COMMAND(
		LinkToHoveredPin,
		"链接到悬停的引脚",
		"将选中的引脚链接到悬停的引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Q));

	UI_COMMAND(
		StraightenHoveredPin,
		"拉直悬停的引脚",
		"拉直悬停或选中的引脚",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		SplitPin,
		"拆分当前引脚",
		"拆分选中或悬停的引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::Q));

	UI_COMMAND(
		RecombinePin,
		"重新组合引脚",
		"重新组合选中或悬停的引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Control, EKeys::Q));

	UI_COMMAND(
		FormatAllEvents,
		"格式化所有事件",
		"重新定位图表中的所有自定义事件",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::R));

	UI_COMMAND(
		ToggleContext,
		"切换上下文",
		"切换当前上下文(BP创建菜单、WBP IsVariable、BP选中节点纯度)",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::T));

	UI_COMMAND(
		SelectNodeUp,
		"选中上方节点",
		"选中当前节点上方的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Up));
	UI_COMMAND(
		SelectNodeDown,
		"选中下方节点",
		"选中当前节点下方的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Down));
	UI_COMMAND(
		SelectNodeLeft,
		"选中左侧节点",
		"选中当前节点左侧的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Left));
	UI_COMMAND(
		SelectNodeRight,
		"选中右侧节点",
		"选中当前节点右侧的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Right));

	UI_COMMAND(
		ExpandNodeSelection,
		"扩展节点选择",
		"将节点选择扩展到下一个逻辑块",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		ExpandSelectionLeft,
		"向左扩展选择",
		"将节点选择扩展到悬停节点左侧的所有节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::Z));

	UI_COMMAND(
		ExpandSelectionRight,
		"向右扩展选择",
		"将节点选择扩展到悬停节点右侧的所有节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::X));

	UI_COMMAND(
		ShiftCameraUp,
		"向上移动相机",
		"向上移动相机",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::Up));
	UI_COMMAND(
		ShiftCameraDown,
		"向下移动相机",
		"向下移动相机",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::Down));
	UI_COMMAND(
		ShiftCameraLeft,
		"向左移动相机",
		"向左移动相机",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::Left));
	UI_COMMAND(
		ShiftCameraRight,
		"向右移动相机",
		"向右移动相机",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Shift, EKeys::Right));

	UI_COMMAND(
		SwapNodeLeft,
		"向左交换节点",
		"与左侧链接的节点交换",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Left));

	UI_COMMAND(
		SwapNodeRight,
		"向右交换节点",
		"与右侧链接的节点交换",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Right));

	UI_COMMAND(
		SwapConnectionUp,
		"向上交换引脚连接",
		"与上方下一个匹配的引脚交换链接或值",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Up));
	UI_COMMAND(
		SwapConnectionDown,
		"向下交换引脚连接",
		"与下方下一个匹配的引脚交换链接或值",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Down));

	UI_COMMAND(
		GoToInGraph,
		"跳转到图表中的符号",
		"跳转到当前图表中的符号",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::G));

	UI_COMMAND(
		OpenWindow,
		"打开窗口",
		"打开窗口菜单",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::K));

	UI_COMMAND(
		DuplicateNodeForEachLink,
		"复制变量节点",
		"为每个链接创建节点的副本",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::V));

	UI_COMMAND(
		MergeSelectedNodes,
		"合并选中的节点",
		"合并选中的节点,保持链接",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::M));

	UI_COMMAND(
		RefreshNodeSizes,
		"刷新节点大小",
		"重新计算选中节点的大小(如果没有选中节点,则刷新所有节点)",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::R));

	UI_COMMAND(
		EditSelectedPinValue,
		"编辑选中的引脚值",
		"编辑当前选中引脚的值",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::E));

	UI_COMMAND(
		DisconnectNodeExecution,
		"断开选中节点的执行",
		"断开选中节点上的所有执行引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::D));

	UI_COMMAND(
		DisconnectPinLink,
		"断开引脚链接",
		"断开选中的引脚或悬停的连线",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::D));

	UI_COMMAND(
		DisconnectAllNodeLinks,
		"断开选中节点的链接",
		"断开选中节点上的所有链接",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::D));

	UI_COMMAND(
		ZoomToNodeTree,
		"缩放到节点树",
		"缩放以适应与当前选中节点连接的所有节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Equals));

	UI_COMMAND(
		GetContextMenuForPin,
		"获取选中引脚的上下文菜单操作",
		"获取当前选中引脚的上下文菜单操作",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::M));
	UI_COMMAND(
		GetContextMenuForNode,
		"获取选中节点的上下文菜单操作",
		"获取当前选中节点的上下文菜单操作",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::M));

	UI_COMMAND(
		SelectPinUp,
		"选中上方引脚",
		"选中当前选中引脚上方的引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Up));
	UI_COMMAND(
		SelectPinDown,
		"选中下方引脚",
		"选中当前选中引脚下方的引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Down));
	UI_COMMAND(
		SelectPinLeft,
		"选中左侧引脚",
		"选中当前选中引脚左侧的引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Left));
	UI_COMMAND(
		SelectPinRight,
		"选中右侧引脚",
		"选中当前选中引脚右侧的引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Right));

	UI_COMMAND(
		FocusSearchBoxMenu,
		"搜索框菜单",
		"打开一个菜单允许你聚焦当前窗口的搜索框",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		VariableSelectorMenu,
		"变量选择器菜单",
		"打开一个菜单允许你选择变量",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::G));

	UI_COMMAND(
		AddSymbolMenu,
		"创建符号菜单",
		"打开一个菜单允许你创建符号",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::A));

	UI_COMMAND(
		EditDetailsMenu,
		"编辑详情菜单",
		"打开一个菜单允许你编辑当前变量详情",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::E));

	UI_COMMAND(
		LinkPinMenu,
		"链接引脚菜单",
		"打开一个菜单允许你链接到图表上的另一个引脚",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::L));

	UI_COMMAND(
		TabSwitcherMenu,
		"标签切换器菜单",
		"打开一个菜单允许你切换标签",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::Tab));

#if BA_UE_VERSION_OR_LATER(5, 4)
	UI_COMMAND(
		OpenFileMenu,
		"打开文件菜单",
		"按名称搜索文件的菜单",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Tilde));

	UI_COMMAND(
		FindInFilesMenu,
		"在文件中查找菜单",
		"在文件中搜索属性的菜单",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::F));
#endif

	UI_COMMAND(
		ToggleNode,
		"切换节点",
		"切换选中节点的禁用状态,需要在编辑器首选项中设置'允许显式禁用不纯节点'",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Slash));

	UI_COMMAND(
		CreateRerouteNode,
		"创建重路由节点",
		"从当前选中的引脚(或选中的重路由节点)创建重路由节点",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		OpenBlueprintAssistHotkeySheet,
		"打开Blueprint Assist快捷键表",
		"打开一个菜单显示Blueprint Assist插件的所有命令和快捷键",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::F1));

	UI_COMMAND(
		ToggleFullscreen,
		"切换全屏",
		"切换当前窗口的全屏状态",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::Enter));

	UI_COMMAND(
		SwitchWorkflowMode,
		"切换工作流模式",
		"打开一个菜单允许你切换工作流模式",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::O));

	UI_COMMAND(
		OpenAssetCreationMenu,
		"打开资产创建菜单",
		"打开一个菜单允许你创建新资产",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Control, EKeys::N));

	UI_COMMAND(
		FocusGraphPanel,
		"聚焦图表面板",
		"如果图表面板已打开,将键盘聚焦设置到图表面板",
		EUserInterfaceActionType::Button,
		FInputChord());

	UI_COMMAND(
		OpenBlueprintAssistDebugMenu,
		"打开Blueprint Assist调试菜单",
		"打开blueprint assist调试菜单,显示关于资产编辑器、图表等的信息",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift | EModifierKey::Alt, EKeys::F12));

	UI_COMMAND(
		FocusSearchBox,
		"聚焦搜索框",
		"将键盘聚焦设置到当前标签的搜索框",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::F));

	UI_COMMAND(
		GoToParentClassDefinition,
		"跳转到父类定义",
		"在Unreal或代码编辑器中导航到当前资产的父类",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::B));

	UI_COMMAND(
		ToggleLockNode,
		"切换锁定节点",
		"锁定图表上的节点,使Blueprint Assist格式化器忽略它",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::L));

	UI_COMMAND(
		GroupNodes,
		"组合节点",
		"将图表上选中的节点组合,使它们一起移动",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::G));

	UI_COMMAND(
		UngroupNodes,
		"解组节点",
		"解组图表上选中的节点",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Control, EKeys::G));

	UI_COMMAND(
		ToggleNodeAdvancedDisplay,
		"切换节点高级显示",
		"切换节点的高级显示以显示隐藏的引脚(主要用于print string)",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt | EModifierKey::Control,  EKeys::A));

	UI_COMMAND(
		GoForwardInTabHistory,
		"在标签历史中前进",
		"聚焦历史中的下一个标签(仅限蓝图图表),不要使用CTRL重新绑定!",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::End));

	UI_COMMAND(
		GoBackInTabHistory,
		"在标签历史中后退",
		"聚焦历史中的上一个标签(仅限蓝图图表),不要使用CTRL重新绑定!",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Alt, EKeys::Home));

	UI_COMMAND(
		SaveAndFormat,
		"保存并格式化",
		"运行全部格式化命令并保存当前图表",
		EUserInterfaceActionType::Button,
		FInputChord());
}

void FBACommands::Register()
{
	UE_LOG(LogBlueprintAssist, Log, TEXT("Registered BlueprintAssist Commands"));
	FBACommandsImpl::Register();
}

const FBACommandsImpl& FBACommands::Get()
{
	return FBACommandsImpl::Get();
}

void FBACommands::Unregister()
{
	UE_LOG(LogBlueprintAssist, Log, TEXT("Unregistered BlueprintAssist Commands"));
	return FBACommandsImpl::Unregister();
}

#undef LOCTEXT_NAMESPACE
