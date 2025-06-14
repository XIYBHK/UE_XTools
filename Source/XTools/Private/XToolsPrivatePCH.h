// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// 核心UObject
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

// 引擎
#include "Engine/World.h"

// 输入
#include "InputCoreTypes.h"

// Slate/UMG - 作为UI的巨型依赖项，暂时保留
#include "SlateBasics.h"
#include "UMG.h"

// 组件
#include "Components/SceneComponent.h"

// 调试
#include "Engine/Canvas.h" // DrawDebugHelpers 依赖
#include "DrawDebugHelpers.h"

// 曲线
#include "Curves/CurveFloat.h"

// 插件特定宏
#include "XToolsDefines.h"
