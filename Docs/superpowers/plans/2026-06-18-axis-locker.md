# AxisLocker 物理轴向锁定模块 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 XTools 新增 `AxisLocker` Runtime 模块，让物理模拟组件可在运行时通过蓝图锁定/解锁/查询位移与旋转自由度（DOF）。

**Architecture:** 核心逻辑放无状态的 `UAxisLockLibrary`（蓝图函数库），输入任意 `UPrimitiveComponent*`，底层走已验证的 `写 6 个 bLockXXX → SetDOFLock(SixDOF)`。`UAxisLockerComponent`（`USceneComponent`）是薄适配器，负责目标解析、声明式配置、BeginPlay 自动应用与临时锁定/恢复栈，所有锁定操作委托给函数库。

**Tech Stack:** UE 5.3–5.7 C++、`FBodyInstance` DOF 锁、`XToolsCore`（`FXToolsErrorReporter` / 版本兼容）。

## 测试策略说明（重要）

本模块是 UE 运行时物理功能，锁定效果依赖 `UWorld` + 物理模拟，**没有可行的纯单元测试**（见 spec 第 8 节）。因此本计划每个任务的验证循环是：

1. **编译验证** —— 通过 VS Code 任务 `Build Editor (Full & Formal)`（或编辑器运行中用 `Live Coding`），要求**零警告零错误**。
2. **PIE 手动验证** —— 在编辑器里按给定步骤操作，观察物理表现 / 日志，对照预期。
3. **提交**。

标准 TDD 的"先写失败测试"在此用"先明确手动验证预期 + 编译门"替代。

## Global Constraints

- 引擎版本：UE **5.3–5.7**；5.3 已实测 DOF 锁生效，5.4–5.7 靠编译 + 打包脚本保证。
- 所有 Runtime 模块依赖 `XToolsCore`：`.Build.cs` 加 `"XToolsCore"`。
- UE 5.4+ 强制 **IWYU**：显式包含所有用到的头文件，不依赖隐式包含。
- 蓝图暴露：所有节点 `Category = "XTools|轴向锁定"`，必须有中文 `DisplayName` / `ToolTip` / `Keywords`。
- 错误处理：统一用 `XTOOLS_LOG_WARNING_EX` / `XTOOLS_LOG_WARNING` 宏（`XToolsErrorReporter.h`），不裸 `UE_LOG`。
- 编码：`.Build.cs` 标准配置 `PCHUsage=UseExplicitOrSharedPCHs`、`bUseUnity=false`、`bEnableExceptions=false`、`bUseRTTI=false`、`IWYUSupport=Full`。
- 版权头：每个源文件以项目统一头开始（见各任务代码）。
- 提交格式：`<type>(AxisLocker): <中文描述>`。
- 解锁语义：用"6 轴全 false + `SetDOFLock(SixDOF)`"，不用 `EDOFMode::None`。

## 文件结构

```
Source/AxisLocker/
├─ AxisLocker.Build.cs              # 模块构建配置
├─ Public/
│  ├─ AxisLockerAPI.h               # LogAxisLocker 日志分类
│  ├─ AxisLockTypes.h               # FAxisLockState 结构体 + EAxisLockPreset 枚举
│  ├─ AxisLockLibrary.h             # UBlueprintFunctionLibrary 声明
│  └─ AxisLockerComponent.h         # USceneComponent 声明
└─ Private/
   ├─ AxisLockerModule.cpp          # IMPLEMENT_MODULE + DEFINE_LOG_CATEGORY（模块类内联）
   ├─ AxisLockLibrary.cpp           # 函数库实现
   └─ AxisLockerComponent.cpp       # 组件实现
```
另需修改 `XTools.uplugin` 注册模块。

| 文件 | 责任 |
|------|------|
| `AxisLocker.Build.cs` | 模块依赖与编译选项 |
| `AxisLockerAPI.h` | 日志分类 `LogAxisLocker` |
| `AxisLockTypes.h` | 蓝图可用的状态结构体与预设枚举 |
| `AxisLockLibrary.{h,cpp}` | 全部锁定/查询核心逻辑（无状态静态函数） |
| `AxisLockerComponent.{h,cpp}` | 目标解析、声明式配置、自动应用、恢复栈 |
| `AxisLockerModule.cpp` | 模块启动/关闭 + 日志分类定义 |

---

## Task 1: 模块骨架与注册

**Files:**
- Create: `Source/AxisLocker/AxisLocker.Build.cs`
- Create: `Source/AxisLocker/Public/AxisLockerAPI.h`
- Create: `Source/AxisLocker/Private/AxisLockerModule.cpp`
- Modify: `XTools.uplugin`（`Modules` 数组追加一项）

**Interfaces:**
- Produces: 模块 `AxisLocker`；日志分类 `LogAxisLocker`（`AxisLockerAPI.h`）；API 宏 `AXISLOCKER_API`（UBT 自动生成）。

- [ ] **Step 1: 写 `AxisLocker.Build.cs`**

```csharp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

using UnrealBuildTool;

/**
 * AxisLocker 插件模块
 *
 * 提供运行时物理 Actor 轴向锁定（DOF）功能：硬锁/解锁、查询、预设、临时恢复
 */
public class AxisLocker : ModuleRules
{
	public AxisLocker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Default;
		IWYUSupport = IWYUSupport.Full;
		bUseUnity = false;
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDefinitions.AddRange(new string[] {
			"WITH_AXISLOCKER=1"
		});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"XToolsCore"  // XTools版本兼容层 + 统一错误处理
			}
		);
	}
}
```

- [ ] **Step 2: 写 `Public/AxisLockerAPI.h`**

```cpp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"

// 使用由 UBT 自动生成的模块 API 宏 AXISLOCKER_API

// AxisLocker 模块日志分类
DECLARE_LOG_CATEGORY_EXTERN(LogAxisLocker, Log, All);
```

- [ ] **Step 3: 写 `Private/AxisLockerModule.cpp`**

```cpp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "Modules/ModuleManager.h"
#include "AxisLockerAPI.h"

#define LOCTEXT_NAMESPACE "FAxisLockerModule"

DEFINE_LOG_CATEGORY(LogAxisLocker);

class FAxisLockerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogAxisLocker, Log, TEXT("AxisLocker module started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogAxisLocker, Log, TEXT("AxisLocker module shutdown"));
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAxisLockerModule, AxisLocker)
```

- [ ] **Step 4: 在 `XTools.uplugin` 的 `Modules` 数组追加模块项**

在数组中（紧跟 `XToolsCore` 项之后即可）插入：

```json
		{
			"Name": "AxisLocker",
			"Type": "Runtime",
			"LoadingPhase": "Default",
			"PlatformAllowList": [
				"Win64",
				"Mac",
				"Linux",
				"Android",
				"iOS"
			]
		},
```

注意：JSON 数组项之间需正确加逗号，确保插入后整体仍是合法 JSON。

- [ ] **Step 5: 编译验证**

运行 VS Code 任务 `Build Editor (Full & Formal)`（或重启编辑器触发编译）。
Expected: 编译成功，零警告零错误。编辑器输出日志出现 `AxisLocker module started`。

- [ ] **Step 6: 提交**

```bash
git add Source/AxisLocker/AxisLocker.Build.cs Source/AxisLocker/Public/AxisLockerAPI.h Source/AxisLocker/Private/AxisLockerModule.cpp XTools.uplugin
git commit -m "feat(AxisLocker): 新增模块骨架与插件注册"
```

---

## Task 2: 类型定义（状态结构体与预设枚举）

**Files:**
- Create: `Source/AxisLocker/Public/AxisLockTypes.h`

**Interfaces:**
- Produces:
  - `struct FAxisLockState`，字段 `bool bLockPositionX/Y/Z, bLockRotationX/Y/Z`（全部默认 `false`）。
  - `enum class EAxisLockPreset : uint8 { None, StayUpright, YawOnly, HorizontalOnly, FreezeAll }`。

- [ ] **Step 1: 写 `Public/AxisLockTypes.h`**

```cpp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "AxisLockTypes.generated.h"

/**
 * 轴向锁定状态。
 * 既是查询返回值，也是组件临时恢复栈的元素。
 * 6 个字段对应 FBodyInstance 的 6 个自由度锁定开关。
 */
USTRUCT(BlueprintType)
struct FAxisLockState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移X"))
	bool bLockPositionX = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Y"))
	bool bLockPositionY = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Z"))
	bool bLockPositionZ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转X"))
	bool bLockRotationX = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Y"))
	bool bLockRotationY = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Z"))
	bool bLockRotationZ = false;
};

/**
 * 高频物理玩法预设。映射关系见 AxisLockLibrary::PresetToState。
 */
UENUM(BlueprintType)
enum class EAxisLockPreset : uint8
{
	None           UMETA(DisplayName = "无（用手动配置）"),
	StayUpright    UMETA(DisplayName = "站立不倒（锁X/Y旋转）"),
	YawOnly        UMETA(DisplayName = "只绕Z轴（锁全位移+X/Y旋转）"),
	HorizontalOnly UMETA(DisplayName = "只能水平移动（锁Z位移）"),
	FreezeAll      UMETA(DisplayName = "完全冻结（6轴全锁）")
};
```

- [ ] **Step 2: 编译验证**

运行 `Build Editor (Full & Formal)`。
Expected: 编译成功，零警告。

- [ ] **Step 3: PIE 手动验证**

打开任意蓝图，在变量类型下拉中搜索 `AxisLockState` 与 `AxisLockPreset`。
Expected: 两者均可见，`FAxisLockState` 展开有 6 个中文命名 bool；`EAxisLockPreset` 下拉有 5 个中文选项。

- [ ] **Step 4: 提交**

```bash
git add Source/AxisLocker/Public/AxisLockTypes.h
git commit -m "feat(AxisLocker): 新增锁定状态结构体与预设枚举"
```

---

## Task 3: 函数库锁定核心（LockAxes / UnlockAll + 校验）

**Files:**
- Create: `Source/AxisLocker/Public/AxisLockLibrary.h`
- Create: `Source/AxisLocker/Private/AxisLockLibrary.cpp`

**Interfaces:**
- Consumes: `FAxisLockState`、`EAxisLockPreset`（Task 2）；`LogAxisLocker`（Task 1）。
- Produces:
  - `static void UAxisLockLibrary::LockAxes(UPrimitiveComponent* Target, bool bLockPosX, bool bLockPosY, bool bLockPosZ, bool bLockRotX, bool bLockRotY, bool bLockRotZ)`
  - `static void UAxisLockLibrary::UnlockAll(UPrimitiveComponent* Target)`
  - 私有 `static FBodyInstance* GetValidBodyInstance(UPrimitiveComponent* Target)`（取 BodyInstance + 校验，未模拟物理仅警告不拦截）。

- [ ] **Step 1: 写 `Public/AxisLockLibrary.h`（含本任务两个公开函数）**

```cpp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AxisLockTypes.h"
#include "AxisLockLibrary.generated.h"

class UPrimitiveComponent;
struct FBodyInstance;

/**
 * 物理 Actor 轴向锁定函数库（无状态）。
 * 输入任意已开启物理模拟的 UPrimitiveComponent，锁定/解锁/查询其自由度。
 */
UCLASS()
class AXISLOCKER_API UAxisLockLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 按 6 轴任意组合锁定目标组件的自由度（底层 SixDOF）。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "锁定轴向",
			Keywords = "物理,锁定,轴,DOF,自由度,旋转,位移",
			ToolTip = "锁定目标组件的位移/旋转自由度（需已开启物理模拟）。\n参数:\nTarget - 目标物理组件\nbLockPosX/Y/Z - 锁定对应轴的位移\nbLockRotX/Y/Z - 锁定对应轴的旋转"
		))
	static void LockAxes(
		UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target,
		UPARAM(DisplayName = "锁定位移X") bool bLockPosX,
		UPARAM(DisplayName = "锁定位移Y") bool bLockPosY,
		UPARAM(DisplayName = "锁定位移Z") bool bLockPosZ,
		UPARAM(DisplayName = "锁定旋转X") bool bLockRotX,
		UPARAM(DisplayName = "锁定旋转Y") bool bLockRotY,
		UPARAM(DisplayName = "锁定旋转Z") bool bLockRotZ);

	/** 解除目标组件的全部自由度锁定。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "解除全部锁定",
			Keywords = "物理,解锁,DOF,自由度",
			ToolTip = "解除目标组件的所有位移/旋转锁定。"
		))
	static void UnlockAll(UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target);

private:
	/**
	 * 取目标的 BodyInstance 并做校验。
	 * 目标无效/无 BodyInstance 时返回 nullptr 并警告；
	 * 未开启物理模拟时仅警告（仍返回 BodyInstance，因为开启模拟后锁定即生效）。
	 */
	static FBodyInstance* GetValidBodyInstance(UPrimitiveComponent* Target);
};
```

- [ ] **Step 2: 写 `Private/AxisLockLibrary.cpp`（本任务部分）**

```cpp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AxisLockLibrary.h"
#include "AxisLockerAPI.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "XToolsErrorReporter.h"

FBodyInstance* UAxisLockLibrary::GetValidBodyInstance(UPrimitiveComponent* Target)
{
	if (!IsValid(Target))
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("轴向锁定失败：目标组件为空或无效"), NAME_None, true, 5.0f);
		return nullptr;
	}

	FBodyInstance* BodyInstance = Target->GetBodyInstance();
	if (!BodyInstance)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("轴向锁定失败：目标组件没有有效的 BodyInstance"), NAME_None, true, 5.0f);
		return nullptr;
	}

	if (!Target->IsSimulatingPhysics())
	{
		// 仅警告，不拦截：开启物理模拟后锁定即生效
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("目标组件未开启物理模拟，轴向锁定对其当前无效（开启模拟后生效）"), NAME_None, true, 5.0f);
	}

	return BodyInstance;
}

void UAxisLockLibrary::LockAxes(
	UPrimitiveComponent* Target,
	bool bLockPosX, bool bLockPosY, bool bLockPosZ,
	bool bLockRotX, bool bLockRotY, bool bLockRotZ)
{
	FBodyInstance* BodyInstance = GetValidBodyInstance(Target);
	if (!BodyInstance)
	{
		return;
	}

	BodyInstance->bLockXTranslation = bLockPosX;
	BodyInstance->bLockYTranslation = bLockPosY;
	BodyInstance->bLockZTranslation = bLockPosZ;
	BodyInstance->bLockXRotation = bLockRotX;
	BodyInstance->bLockYRotation = bLockRotY;
	BodyInstance->bLockZRotation = bLockRotZ;

	// SixDOF 是表达任意轴组合的唯一模式；内部会 Term 旧约束并按上面 6 个开关重建
	BodyInstance->SetDOFLock(EDOFMode::SixDOF);
}

void UAxisLockLibrary::UnlockAll(UPrimitiveComponent* Target)
{
	// 6 轴全 false + SetDOFLock：CreateDOFLock 检测到全 false 时不创建约束，等于解除
	LockAxes(Target, false, false, false, false, false, false);
}
```

- [ ] **Step 3: 编译验证**

运行 `Build Editor (Full & Formal)`。
Expected: 编译成功，零警告。

- [ ] **Step 4: PIE 手动验证**

1. 新建一个 Actor，加一个 Cube `StaticMeshComponent`，勾选 `Simulate Physics`，置于空中可下落翻滚的位置。
2. 关卡蓝图 BeginPlay：取该 Cube 组件 → 调用 `锁定轴向`，勾选 `锁定旋转X/Y/Z` 全部为 true，位移全 false。
3. Play。
Expected: Cube 受重力下落，但**不再翻滚旋转**（旋转被锁），落地后可平移。再做一个调用 `解除全部锁定` 的测试，确认解锁后恢复正常翻滚。
4. 用一个空目标调用 `锁定轴向`。Expected: 不崩溃，输出/屏幕出现"目标组件为空或无效"警告。

- [ ] **Step 5: 提交**

```bash
git add Source/AxisLocker/Public/AxisLockLibrary.h Source/AxisLocker/Private/AxisLockLibrary.cpp
git commit -m "feat(AxisLocker): 函数库锁定核心 LockAxes/UnlockAll 与校验"
```

---

## Task 4: 函数库查询（GetLockState / IsAnyAxisLocked）

**Files:**
- Modify: `Source/AxisLocker/Public/AxisLockLibrary.h`（追加 2 个公开纯函数声明）
- Modify: `Source/AxisLocker/Private/AxisLockLibrary.cpp`（追加 2 个实现）

**Interfaces:**
- Consumes: `GetValidBodyInstance`（Task 3）、`FAxisLockState`（Task 2）。
- Produces:
  - `static FAxisLockState UAxisLockLibrary::GetLockState(UPrimitiveComponent* Target)`
  - `static bool UAxisLockLibrary::IsAnyAxisLocked(UPrimitiveComponent* Target)`

- [ ] **Step 1: 在 `AxisLockLibrary.h` 的 `public:` 段、`private:` 之前追加声明**

```cpp
	/** 查询目标组件当前的轴向锁定状态。*/
	UFUNCTION(BlueprintPure, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "获取锁定状态",
			Keywords = "物理,查询,锁定,状态,DOF",
			ToolTip = "读取目标组件当前 6 个自由度的锁定情况。目标无效时返回全部未锁定。"
		))
	static FAxisLockState GetLockState(UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target);

	/** 目标组件是否有任意一个轴被锁定。*/
	UFUNCTION(BlueprintPure, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "是否有轴被锁定",
			Keywords = "物理,查询,锁定,DOF",
			ToolTip = "目标组件只要有任意位移/旋转轴被锁定就返回 true。"
		))
	static bool IsAnyAxisLocked(UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target);
```

- [ ] **Step 2: 在 `AxisLockLibrary.cpp` 末尾追加实现**

```cpp
FAxisLockState UAxisLockLibrary::GetLockState(UPrimitiveComponent* Target)
{
	FAxisLockState State; // 默认全 false

	// 查询不修改状态，无需 IsSimulatingPhysics 校验；目标无效直接返回全 false
	if (!IsValid(Target))
	{
		return State;
	}

	FBodyInstance* BodyInstance = Target->GetBodyInstance();
	if (!BodyInstance)
	{
		return State;
	}

	State.bLockPositionX = BodyInstance->bLockXTranslation;
	State.bLockPositionY = BodyInstance->bLockYTranslation;
	State.bLockPositionZ = BodyInstance->bLockZTranslation;
	State.bLockRotationX = BodyInstance->bLockXRotation;
	State.bLockRotationY = BodyInstance->bLockYRotation;
	State.bLockRotationZ = BodyInstance->bLockZRotation;
	return State;
}

bool UAxisLockLibrary::IsAnyAxisLocked(UPrimitiveComponent* Target)
{
	const FAxisLockState State = GetLockState(Target);
	return State.bLockPositionX || State.bLockPositionY || State.bLockPositionZ
		|| State.bLockRotationX || State.bLockRotationY || State.bLockRotationZ;
}
```

- [ ] **Step 3: 编译验证**

运行 `Build Editor (Full & Formal)`。
Expected: 编译成功，零警告。

- [ ] **Step 4: PIE 手动验证**

沿用 Task 3 的 Cube。BeginPlay：先 `锁定轴向`（锁旋转X/Y/Z）→ 再 `获取锁定状态` 并 Print → 再 `是否有轴被锁定` 并 Print。
Expected: 打印的状态中旋转 X/Y/Z 为 true、位移为 false；"是否有轴被锁定"为 true。对未锁定的新组件查询，返回全 false / false。

- [ ] **Step 5: 提交**

```bash
git add Source/AxisLocker/Public/AxisLockLibrary.h Source/AxisLocker/Private/AxisLockLibrary.cpp
git commit -m "feat(AxisLocker): 函数库查询 GetLockState/IsAnyAxisLocked"
```

---

## Task 5: 函数库预设（PresetToState / ApplyPreset）

**Files:**
- Modify: `Source/AxisLocker/Public/AxisLockLibrary.h`（追加 1 公开 + 1 公开纯函数）
- Modify: `Source/AxisLocker/Private/AxisLockLibrary.cpp`（追加 2 实现）

**Interfaces:**
- Consumes: `LockAxes`（Task 3）、`EAxisLockPreset` / `FAxisLockState`（Task 2）。
- Produces:
  - `static FAxisLockState UAxisLockLibrary::PresetToState(EAxisLockPreset Preset)`（纯函数，预设→状态映射，供组件复用）
  - `static void UAxisLockLibrary::ApplyPreset(UPrimitiveComponent* Target, EAxisLockPreset Preset)`

**预设映射表（实现依据）:**

| 预设 | PosX | PosY | PosZ | RotX | RotY | RotZ |
|------|------|------|------|------|------|------|
| None | F | F | F | F | F | F |
| StayUpright | F | F | F | T | T | F |
| YawOnly | T | T | T | T | T | F |
| HorizontalOnly | F | F | T | F | F | F |
| FreezeAll | T | T | T | T | T | T |

- [ ] **Step 1: 在 `AxisLockLibrary.h` 的 `public:` 段追加声明**

```cpp
	/** 将预设转换为锁定状态（纯映射，不接触物理）。*/
	UFUNCTION(BlueprintPure, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "预设转锁定状态",
			Keywords = "物理,预设,锁定,状态,DOF",
			ToolTip = "把 EAxisLockPreset 预设转换为对应的 6 轴锁定状态。"
		))
	static FAxisLockState PresetToState(UPARAM(DisplayName = "预设") EAxisLockPreset Preset);

	/** 将预设组合一键应用到目标组件。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定",
		meta = (
			DisplayName = "应用锁定预设",
			Keywords = "物理,预设,锁定,DOF,站立不倒,只绕Z轴",
			ToolTip = "把高频物理玩法预设一键应用到目标组件（需已开启物理模拟）。"
		))
	static void ApplyPreset(
		UPARAM(DisplayName = "目标组件") UPrimitiveComponent* Target,
		UPARAM(DisplayName = "预设") EAxisLockPreset Preset);
```

- [ ] **Step 2: 在 `AxisLockLibrary.cpp` 末尾追加实现**

```cpp
FAxisLockState UAxisLockLibrary::PresetToState(EAxisLockPreset Preset)
{
	FAxisLockState State; // 默认全 false（含 None）

	switch (Preset)
	{
	case EAxisLockPreset::StayUpright:
		State.bLockRotationX = true;
		State.bLockRotationY = true;
		break;

	case EAxisLockPreset::YawOnly:
		State.bLockPositionX = true;
		State.bLockPositionY = true;
		State.bLockPositionZ = true;
		State.bLockRotationX = true;
		State.bLockRotationY = true;
		break;

	case EAxisLockPreset::HorizontalOnly:
		State.bLockPositionZ = true;
		break;

	case EAxisLockPreset::FreezeAll:
		State.bLockPositionX = true;
		State.bLockPositionY = true;
		State.bLockPositionZ = true;
		State.bLockRotationX = true;
		State.bLockRotationY = true;
		State.bLockRotationZ = true;
		break;

	case EAxisLockPreset::None:
	default:
		break;
	}

	return State;
}

void UAxisLockLibrary::ApplyPreset(UPrimitiveComponent* Target, EAxisLockPreset Preset)
{
	const FAxisLockState State = PresetToState(Preset);
	LockAxes(Target,
		State.bLockPositionX, State.bLockPositionY, State.bLockPositionZ,
		State.bLockRotationX, State.bLockRotationY, State.bLockRotationZ);
}
```

- [ ] **Step 3: 编译验证**

运行 `Build Editor (Full & Formal)`。
Expected: 编译成功，零警告。

- [ ] **Step 4: PIE 手动验证**

用 Task 3 的 Cube，BeginPlay 调用 `应用锁定预设`，分别测试：
- `站立不倒`：Cube 下落不翻倒，落地保持直立，可被推着平移。
- `只绕Z轴`：Cube 位置固定，只能绕竖直轴自转（施加扭矩验证）。
- `完全冻结`：Cube 悬空不动（位置与朝向都锁死）。
Expected: 各预设物理表现与上面描述一致。

- [ ] **Step 5: 提交**

```bash
git add Source/AxisLocker/Public/AxisLockLibrary.h Source/AxisLocker/Private/AxisLockLibrary.cpp
git commit -m "feat(AxisLocker): 函数库预设 PresetToState/ApplyPreset"
```

---

## Task 6: 组件基础（目标解析 + 声明式配置 + 自动应用）

**Files:**
- Create: `Source/AxisLocker/Public/AxisLockerComponent.h`
- Create: `Source/AxisLocker/Private/AxisLockerComponent.cpp`

**Interfaces:**
- Consumes: `UAxisLockLibrary::LockAxes` / `ApplyPreset`（Task 3、5）；`EAxisLockPreset`（Task 2）。
- Produces:
  - `UAxisLockerComponent : public USceneComponent`
  - `UPrimitiveComponent* ResolveTarget() const`（优先级：运行时设置的 `TargetComponentOverride` → `TargetComponentRef` 解析 → `GetAttachParent`）
  - `void SetTargetComponent(UPrimitiveComponent* InTarget)`
  - `void ApplyConfiguredLock()`

- [ ] **Step 1: 写 `Public/AxisLockerComponent.h`**

```cpp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "AxisLockTypes.h"
#include "AxisLockerComponent.generated.h"

class UPrimitiveComponent;

/**
 * 物理轴向锁定组件（薄适配器）。
 * 默认锁定自己挂载的父级 PrimitiveComponent（与参考插件等价：挂到 Cube 下控制 Cube）；
 * 也可通过显式目标引用或运行时 SetTargetComponent 指定其他组件。
 * 所有锁定操作委托给 UAxisLockLibrary。
 */
UCLASS(ClassGroup = (XTools), meta = (BlueprintSpawnableComponent, DisplayName = "轴向锁定组件"))
class AXISLOCKER_API UAxisLockerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAxisLockerComponent();

	/** 锁定预设；为"无"时改用下面 6 个手动开关。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "预设"))
	EAxisLockPreset Preset = EAxisLockPreset::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移X", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockPositionX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Y", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockPositionY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定位移Z", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockPositionZ = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转X", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockRotationX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Y", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockRotationY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "锁定旋转Z", EditCondition = "Preset == EAxisLockPreset::None"))
	bool bLockRotationZ = false;

	/** 是否在 BeginPlay 时按配置自动应用锁定。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "开始游戏时自动应用"))
	bool bAutoApplyOnBeginPlay = true;

	/** 可选：显式指定要锁定的同 Actor 组件；留空则锁定挂载父级。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|轴向锁定", meta = (DisplayName = "目标组件（可选）", UseComponentPicker, AllowAnyActor))
	FComponentReference TargetComponentRef;

	/** 运行时指定锁定目标（优先级最高）。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "设置目标组件"))
	void SetTargetComponent(UPrimitiveComponent* InTarget);

	/** 按当前配置（预设或手动开关）应用锁定。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "应用配置的锁定"))
	void ApplyConfiguredLock();

protected:
	virtual void BeginPlay() override;

	/** 解析最终要操作的物理组件。*/
	UPrimitiveComponent* ResolveTarget() const;

private:
	/** 运行时覆盖目标（SetTargetComponent 设置），优先于 TargetComponentRef 与挂载父级。*/
	TWeakObjectPtr<UPrimitiveComponent> TargetComponentOverride;
};
```

- [ ] **Step 2: 写 `Private/AxisLockerComponent.cpp`（本任务部分）**

```cpp
/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "AxisLockerComponent.h"
#include "AxisLockLibrary.h"
#include "AxisLockerAPI.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "XToolsErrorReporter.h"

UAxisLockerComponent::UAxisLockerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAxisLockerComponent::SetTargetComponent(UPrimitiveComponent* InTarget)
{
	TargetComponentOverride = InTarget;
}

UPrimitiveComponent* UAxisLockerComponent::ResolveTarget() const
{
	// 1) 运行时覆盖优先
	if (TargetComponentOverride.IsValid())
	{
		return TargetComponentOverride.Get();
	}

	// 2) 编辑器显式引用
	if (const AActor* Owner = GetOwner())
	{
		if (UPrimitiveComponent* Resolved = Cast<UPrimitiveComponent>(TargetComponentRef.GetComponent(const_cast<AActor*>(Owner))))
		{
			return Resolved;
		}
	}

	// 3) 回退到挂载父级（与参考插件等价）
	return Cast<UPrimitiveComponent>(GetAttachParent());
}

void UAxisLockerComponent::ApplyConfiguredLock()
{
	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("轴向锁定组件：未能解析到目标物理组件（请挂到 PrimitiveComponent 下或设置目标组件）"), NAME_None, true, 5.0f);
		return;
	}

	if (Preset != EAxisLockPreset::None)
	{
		UAxisLockLibrary::ApplyPreset(Target, Preset);
	}
	else
	{
		UAxisLockLibrary::LockAxes(Target,
			bLockPositionX, bLockPositionY, bLockPositionZ,
			bLockRotationX, bLockRotationY, bLockRotationZ);
	}
}

void UAxisLockerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoApplyOnBeginPlay)
	{
		ApplyConfiguredLock();
	}
}
```

- [ ] **Step 3: 编译验证**

运行 `Build Editor (Full & Formal)`。
Expected: 编译成功，零警告。

- [ ] **Step 4: PIE 手动验证**

1. 在 Task 3 的物理 Cube Actor 中，把 `轴向锁定组件` 拖为 **Cube 的子组件**（挂在 Cube 下）。
2. 组件细节面板设 `预设 = 站立不倒`，`开始游戏时自动应用 = true`，`目标组件（可选）` 留空。
3. Play。
Expected: 无需任何蓝图代码，Cube 下落保持直立不翻倒（组件自动锁定了挂载父级 Cube）。
4. 另测：把组件挂到非 Cube 处、并把 `目标组件（可选）` 选成 Cube，Play 应同样生效（显式引用覆盖挂载父级）。

- [ ] **Step 5: 提交**

```bash
git add Source/AxisLocker/Public/AxisLockerComponent.h Source/AxisLocker/Private/AxisLockerComponent.cpp
git commit -m "feat(AxisLocker): 轴向锁定组件目标解析与自动应用"
```

---

## Task 7: 组件临时锁定/恢复栈（Push/Pop）

**Files:**
- Modify: `Source/AxisLocker/Public/AxisLockerComponent.h`（追加 2 函数 + 1 私有成员）
- Modify: `Source/AxisLocker/Private/AxisLockerComponent.cpp`（追加 2 实现）

**Interfaces:**
- Consumes: `ResolveTarget`（Task 6）、`UAxisLockLibrary::GetLockState` / `LockAxes`（Task 3、4）、`FAxisLockState`（Task 2）。
- Produces:
  - `void PushLockState()`（把目标当前状态压栈）
  - `void PopLockState()`（弹出并恢复）

- [ ] **Step 1: 在 `AxisLockerComponent.h` 的 `public:` 段追加声明**

```cpp
	/** 保存目标组件当前锁定状态到栈（之后可 PopLockState 还原）。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "压入锁定状态"))
	void PushLockState();

	/** 从栈弹出并恢复最近一次保存的锁定状态。*/
	UFUNCTION(BlueprintCallable, Category = "XTools|轴向锁定", meta = (DisplayName = "弹出并恢复锁定状态"))
	void PopLockState();
```

- [ ] **Step 2: 在 `AxisLockerComponent.h` 的 `private:` 段追加成员**

```cpp
	/** 临时锁定状态栈。*/
	TArray<FAxisLockState> LockStateStack;
```

- [ ] **Step 3: 在 `AxisLockerComponent.cpp` 末尾追加实现**

```cpp
void UAxisLockerComponent::PushLockState()
{
	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("压入锁定状态失败：未能解析到目标物理组件"), NAME_None, true, 5.0f);
		return;
	}

	LockStateStack.Push(UAxisLockLibrary::GetLockState(Target));
}

void UAxisLockerComponent::PopLockState()
{
	if (LockStateStack.Num() == 0)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("弹出锁定状态失败：状态栈为空"), NAME_None, true, 5.0f);
		return;
	}

	const FAxisLockState State = LockStateStack.Pop();

	UPrimitiveComponent* Target = ResolveTarget();
	if (!Target)
	{
		XTOOLS_LOG_WARNING_EX(LogAxisLocker, TEXT("恢复锁定状态失败：未能解析到目标物理组件"), NAME_None, true, 5.0f);
		return;
	}

	UAxisLockLibrary::LockAxes(Target,
		State.bLockPositionX, State.bLockPositionY, State.bLockPositionZ,
		State.bLockRotationX, State.bLockRotationY, State.bLockRotationZ);
}
```

- [ ] **Step 4: 编译验证**

运行 `Build Editor (Full & Formal)`。
Expected: 编译成功，零警告。

- [ ] **Step 5: PIE 手动验证**

用 Task 6 的设置（组件挂 Cube 下，预设站立不倒）。蓝图序列：
1. `压入锁定状态`（保存当前=站立不倒）。
2. 调用函数库 `应用锁定预设` → `完全冻结`（Cube 定身）。
3. 延迟 2 秒后 `弹出并恢复锁定状态`。
Expected: 前 2 秒 Cube 完全不动；恢复后回到"站立不倒"（重新下落但不翻倒）。再对空栈调用 `弹出并恢复锁定状态`，确认不崩溃且有"状态栈为空"警告。

- [ ] **Step 6: 提交**

```bash
git add Source/AxisLocker/Public/AxisLockerComponent.h Source/AxisLocker/Private/AxisLockerComponent.cpp
git commit -m "feat(AxisLocker): 组件临时锁定/恢复栈 Push/Pop"
```

---

## Self-Review 结果

**Spec 覆盖：**
- 模块结构（spec §3）→ Task 1、2、6 创建对应文件，Task 1 注册 uplugin ✓
- 函数库 LockAxes/ApplyPreset/UnlockAll/GetLockState/IsAnyAxisLocked（spec §4.2）→ Task 3/4/5 全覆盖 ✓
- 组件目标解析 + 声明式配置 + 自动应用 + Push/Pop（spec §4.3）→ Task 6/7 ✓
- 核心机制三步（spec §5）→ Task 3 LockAxes 实现 ✓；解锁用全 false（spec §5）→ Task 3 UnlockAll ✓
- 错误处理前置校验（spec §6）→ Task 3 GetValidBodyInstance + 各函数 ✓
- 跨版本无版本宏（spec §7）→ 所用 API 一致，计划未引入版本分支 ✓
- 测试 PIE 手动验证（spec §8）→ 每个 Task Step 4/5 ✓
- 开放项预设映射（spec §9）→ Task 5 映射表已定具体值 ✓

**占位符扫描：** 无 TBD/TODO；每个代码步骤含完整可编译代码。

**类型一致性：** `FAxisLockState` 6 字段名（bLockPositionX/Y/Z、bLockRotationX/Y/Z）在 Task 2 定义后，Task 3/4/5/7 引用一致；`EAxisLockPreset` 5 枚举值一致；函数签名 `LockAxes`/`ApplyPreset`/`GetLockState`/`PresetToState` 在声明与调用处一致 ✓。
