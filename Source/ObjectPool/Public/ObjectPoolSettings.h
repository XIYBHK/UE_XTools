/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ObjectPoolSettings.generated.h"

/**
 * XTools 对象池运行时开发者设置
 *
 * 运行时可用（编辑器与打包后均生效），替代原 Editor-only 的
 * /Script/X_AssetEditor.X_AssetEditorSettings 字符串反射读取方案，
 * 确保同一项目在编辑器与打包成品中的对象池开关行为一致。
 *
 * 配置入口：项目设置 -> 游戏 -> XTools 对象池设置
 */
UCLASS(config = Game, DefaultConfig, meta = (DisplayName = "XTools 对象池设置"))
class OBJECTPOOL_API UObjectPoolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * 启用对象池子系统
	 *
	 * 功能：提供高性能的Actor对象池，减少频繁创建/销毁开销
	 * 性能影响：启用后会在BeginPlay时分帧预热对象池
	 * 关闭后子系统不会创建，所有生成请求自动回退为普通SpawnActor（永不失败）
	 *
	 * 默认值：false（保守兼容）。旧开关位于 Editor-only 的
	 * /Script/X_AssetEditor.X_AssetEditorSettings 且默认关闭；若本设置默认开启，
	 * 未显式配置过的升级项目会被静默启用对象池，违背其既有行为预期。
	 * 需要对象池的项目请在此显式开启（配置写入项目 DefaultGame.ini，
	 * 编辑器与打包成品行为一致）。
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "对象池", meta = (
		DisplayName = "启用对象池子系统",
		ToolTip = "默认关闭，按需启用。\n启用后提供Actor对象池功能，适合需要频繁生成/销毁Actor的项目\n关闭后子系统不创建，生成请求自动回退为普通SpawnActor（永不失败）\n编辑器与打包成品行为一致\n升级提示：旧版对象池开关（X_AssetEditorSettings）默认关闭，从旧版升级的项目如需对象池请显式开启本选项"))
	bool bEnableObjectPoolSubsystem = false;
};
