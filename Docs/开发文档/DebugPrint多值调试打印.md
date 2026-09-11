# DebugPrint 多值调试打印

在蓝图菜单搜索 `DebugPrint` 或 `调试打印`，分类为 `XTools|调试`。点击节点的添加输入按钮，可把不同类型的值接到同一节点，默认输出 `名称 = 值 | 名称 = 值`。

## 使用

- 每个值独立推断类型，名称取自连接的输出引脚；函数返回值使用来源节点标题。
- 高级引脚控制名称显示、逐行显示、分隔符、屏幕/日志输出、颜色和持续时间。
- `覆盖键` 为 `None` 时追加屏幕消息；设置同一个非空键时覆盖该键的屏幕消息。日志仍按调用记录。
- 右键值引脚可移除，至少保留一个输入。删除中间输入不会改变其他输入的内部身份。断线后保留已确定类型和默认值；新建输入可以重新接其他类型。
- 支持 UE 反射属性的文本导出，包括基本值、结构体和容器。对象输出引用文本，不递归遍历对象。执行引脚和委托不接受为调试值。
- 无连接且未确定类型的输入输出空文本。复杂类型使用 UE 属性文本格式，不调用用户自定义字符串转换函数；容器输出顺序遵循其原生导出行为。
- Actor 等具有世界上下文的蓝图自动使用 Self，普通 UObject 蓝图可显式传入世界上下文。
- 节点标为 DevelopmentOnly；运行时函数也在 Shipping/Test 构建中禁用打印。

## 实现与来源

编辑器节点位于 `BlueprintExtensions`，展开为每个值的反射文本转换、字符串数组和 `BlueprintExtensionsRuntime` 打印函数，最终调用 UE 原生 `UKismetSystemLibrary::PrintString`。不存在 Runtime 到 Editor 的依赖。

实现前阅读了 [MoxAlehin/DebugPrint](https://github.com/MoxAlehin/DebugPrint) 的节点代码。该源码头标注 All Rights Reserved，检查时未见明确许可证，因此没有复制其实现；本节点基于 UE 原生 K2、反射和打印 API 独立实现。它不包含上游的自动列对齐、节点区域拖入自动增 pin 等扩展交互。

自动化前缀：`XTools.BlueprintExtensions.DebugPrint`。通过项目 `Scripts/Test-UE53.ps1 -Tests XTools.BlueprintExtensions.DebugPrint` 执行编译和真实蓝图运行验证。已在 UE 5.3.2 完成 Editor 编译及 2 项自动化测试（无失败、无警告），覆盖数组与字符串字面量运行、拆分 Vector 重建、删除中间输入、格式输出和脱离蓝图的引脚创建。其他 UE 版本及打包运行须独立验证。
