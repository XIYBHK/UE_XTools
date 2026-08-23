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
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "对象池", meta = (
		DisplayName = "启用对象池子系统",
		ToolTip = "启用后提供Actor对象池功能，适合需要频繁生成/销毁Actor的项目\n关闭后子系统不创建，生成请求自动回退为普通SpawnActor\n编辑器与打包成品行为一致"))
	bool bEnableObjectPoolSubsystem = true;
};
