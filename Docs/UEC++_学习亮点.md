# UE C++ 进阶学习亮点

> 参考仓库：<https://github.com/xusjtuer/NoteUE4/tree/master/AwesomeBlueprints>
> UE 版本：5.x

---

## 1. 插件模块化与 Build.cs 管理

* **示例路径**：`AwesomeBlueprints/Plugins/*/*.Build.cs`
* 亮点：清晰划分 `Public` / `Private` 依赖，`Target.bBuildEditor` 条件编译，最小化运行时模块体积。

```cs
if (Target.bBuildEditor)
{
    PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "BlueprintGraph" });
}
```

---

## 2. AssetTypeActions & 自定义资源分类

* **示例路径**：`Plugins/AssetTools/` 相关类
* 亮点：注册 `FAssetTypeActions_Base` 子类，自定义图标、菜单、颜色，实现一键创建/双击打开。

---

## 3. 编辑器扩展

### 3.1 详情面板自定义（DetailCustomization）

* `IDetailCustomization::CustomizeDetails` 动态 UI
* 可结合 `IPropertyTypeCustomization` 定制结构体显示。

### 3.2 Component Visualizer

* 通过 `FComponentVisualizer` 绘制运行时 Gizmo。
* 支持拖拽编辑、点击拾取。

---

## 4. Slate & UMG 插件化控件

* **示例路径**：`Widgets/**`
* 使用 `SCompoundWidget` + `SListView` + `SSearchBox` 组合高性能编辑器工具面板。

---

## 5. BlueprintFunctionLibrary 高效实现

* 用 `BlueprintCallable` + `DisplayName` + `AutoCreateRefTerm` 精准控制节点外观。
* `meta = (WorldContext="WorldContextObject")` 支持纯蓝图调用。

---

## 6. 数据驱动：DataAsset & GameplayTags

* 自定义 `UPrimaryDataAsset` 持久化配置。
* 使用 `UGameplayTagsManager::RequestGameplayTag` 动态注册标签。

---

## 7. Subsystem 体系

* **示例**：`UGameInstanceSubsystem`, `UEditorSubsystem`
* 按生命周期集中管理全局功能，避免使用单例。

---

## 8. Async 任务封装

* `AsyncTask(ENamedThreads::GameThread, [=]{});`
* `FAsyncTask` / `FNonAbandonableTask` 处理 CPU 重任务。
* Latent Action 包装蓝图易用接口。

---

## 9. 自动化测试（AutomationSpec）

* **示例路径**：`Tests/**`
* BDD 风格：`Describe` / `It`，结合 `ADD_LATENT_AUTOMATION_COMMAND` 驱动关卡测试。

---

## 10. 性能分析与 STAT_GROUP

```cpp
DECLARE_STAT_GROUP(TEXT("MyPlugIn"), STATGROUP_MyPlugin, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("TickWork"), STAT_TickWork, STATGROUP_MyPlugin);
```

`SCOPE_CYCLE_COUNTER(STAT_TickWork);` 捕获 Tick 耗时。

---

## 11. 反射宏进阶

* 使用 `meta = (AllowPrivateAccess = "true")` 暴露私有属性。
* `TWeakObjectPtr` + `UPROPERTY()` 避免循环引用。

---

## 12. 批处理脚本：Python & Commandlet

* **示例**：`UE-Python` 脚本批量导入资源。
* 自定义 `UCommandlet` 进行离线烘培、数据检查。

---

> 若需任何板块的代码模板或深入解读，敬请提出！ 