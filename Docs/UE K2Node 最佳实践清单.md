UE K2Node 最佳实践清单
版本：UE5.3+

本清单提供了一系列快速、可操作的最佳实践，用于在开发和审查自定义 UK2Node 时进行核对。

1. 生命周期与结构
[ ] 生命周期函数: 正确使用 AllocateDefaultPins, ReconstructNode, ReallocatePinsDuringReconstruction, 和 ExpandNode。

[ ] 编辑器代码隔离: 所有K2Node、Slate UI和编辑器相关逻辑都必须包裹在 #if WITH_EDITOR 宏内。

[ ] 模块依赖: 在 .Build.cs 中，编辑器模块（UnrealEd, BlueprintGraph 等）只在 Target.bBuildEditor == true 时添加。

2. 引脚管理
[ ] 不手动清空 Pins 数组: 绝不调用 Pins.Empty()。始终依赖 ReconstructNode 来管理引脚的重建。

[ ] 使用 ReconstructNode: 对于任何需要刷新引脚的场景（类型变化、用户选项更改），调用 ReconstructNode()，而不是自己实现刷新逻辑。它会自动处理撤销/重做和连线迁移。

[ ] RestoreSplitPins 和 RewireOldPinsToNewPins: 在 ReallocatePinsDuringReconstruction 中，先调用 Super 或 RestoreSplitPins，再创建新引脚，以确保结构体引脚（如Vector）的子引脚连接得以保留。

[ ] 引脚名称常量: 使用 static const FName 或 struct 来管理引脚名称，避免在代码中硬编码字符串。

3. 编译 (ExpandNode)
[ ] 错误处理: 在 ExpandNode 的开头就检查所有必要的输入连接。如果验证失败，使用 CompilerContext.MessageLog.Error() 报告错误，并调用 BreakAllNodeLinks() 后立即 return。

[ ] 中间节点: 始终使用 CompilerContext.SpawnIntermediateNode 来创建函数调用节点。

[ ] 移动连接: 使用 CompilerContext.MovePinLinksToIntermediate() 来转移引脚连接，而不是手动 MakeLinkTo。

[ ] 清理节点: 在 ExpandNode 的末尾，必须调用 BreakAllNodeLinks() 清理自身，因为它已被中间节点替换。

[ ] BlueprintCallable: ExpandNode 调用的底层 UFUNCTION 必须是 BlueprintCallable，即使它看起来像 Pure。否则执行链无法连接。

4. UI/UX (用户体验)
[ ] 分类: GetMenuCategory() 应使用清晰的层级，如 "MyPlugin|Utilities|Array"。

[ ] 标题和提示: 实现 GetNodeTitle() 和 GetTooltipText()，并为每个引脚设置 PinToolTip。

[ ] 本地化: 所有面向用户的字符串（标题、提示、错误信息）都应使用 LOCTEXT 宏。

[ ] 图标: 实现 GetCornerIcon()，使用 FEditorStyle::GetBrush() 提供一个已注册的编辑器图标，增加辨识度。

[ ] 默认值: 为输入引脚提供合理的默认值。

5. 健壮性与兼容性
[ ] 事务 (FScopedTransaction): 任何修改节点状态并触发 ReconstructNode 的操作都应包裹在 FScopedTransaction 中，以支持撤销和重做。

[ ] 空指针检查: 在所有代码路径中，对指针（尤其是 UEdGraphPin*）进行严格的有效性检查。

[ ] 版本兼容: 如果重命名了引脚，重写 GetRedirectPinNames() 以维护向后兼容性。如果重命名了类或函数，使用 CoreRedirects。

[ ] 性能: 对于可能耗时的编辑器操作（如遍历大量属性），使用 FScopedSlowTask 显示进度条，防止编辑器假死。

6. 枚举与元数据
[ ] BlueprintType: 用于蓝图引脚的 UENUM 必须标记为 BlueprintType。

[ ] DisplayName: 为枚举值添加 UMETA(DisplayName="...") 以提高在UI中的可读性。

[ ] 运行时可用性: 确保枚举类型定义在运行时模块中，而不是Editor-only模块，以便 ExpandNode 生成的节点可以在运行时访问它。