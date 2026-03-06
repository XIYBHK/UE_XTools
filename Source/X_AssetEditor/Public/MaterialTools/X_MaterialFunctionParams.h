/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "X_MaterialFunctionParams.generated.h"

/**
 * 材质函数连接模式
 */
UENUM()
enum class EConnectionMode : uint8
{
	/** 直接连接，不使用任何操作节点 */
	None UMETA(DisplayName = "直接连接"),
	/** 自动检测和连接 */
	Auto UMETA(DisplayName = "自动选择"),
	/** 添加节点 */
	Add UMETA(DisplayName = "Add 叠加"),
	/** 相乘节点 */
	Multiply UMETA(DisplayName = "Multiply 相乘")
};

/**
 * 材质函数参数结构体
 * 用于在添加材质函数到材质时配置参数
 */
USTRUCT()
struct FX_MaterialFunctionParams
{
	GENERATED_USTRUCT_BODY()

	/** 节点名称 */
	UPROPERTY(EditAnywhere, Category = "基本", meta = (
		DisplayName = "节点名称",
		ToolTip = "添加到材质图中的材质函数节点名称。默认使用材质函数资产名。"))
	FString NodeName;

	/** X坐标位置 */
	UPROPERTY(EditAnywhere, Category = "布局", meta = (
		DisplayName = "X 坐标",
		ToolTip = "材质函数节点在材质编辑器中的水平位置。",
		ClampMin = "-5000", ClampMax = "5000"))
	int32 PosX = -300;

	/** Y坐标位置 */
	UPROPERTY(EditAnywhere, Category = "布局", meta = (
		DisplayName = "Y 坐标",
		ToolTip = "材质函数节点在材质编辑器中的垂直位置。",
		ClampMin = "-5000", ClampMax = "5000"))
	int32 PosY = 0;

	/** 是否自动设置连接 */
	UPROPERTY(EditAnywhere, Category = "连接", meta = (
		DisplayName = "自动连接到材质属性",
		ToolTip = "开启后，添加节点后会尝试自动连接到材质最终输出或现有链路。"))
	bool bSetupConnections = true;

	/** 是否启用自动连接逻辑 */
	UPROPERTY(EditAnywhere, Category = "连接", meta = (
		DisplayName = "启用智能连接",
		ToolTip = "根据函数输入输出语义自动推断连接位置。关闭后使用手动连接目标。",
		EditCondition = "bSetupConnections",
		EditConditionHides))
	bool bEnableSmartConnect = true;

	/** 是否强制使用MaterialAttributes连接 */
	UPROPERTY(EditAnywhere, Category = "连接", meta = (
		DisplayName = "优先使用 Material Attributes",
		ToolTip = "当函数输出 Material Attributes 时，优先连接到材质的 Material Attributes 主链路。",
		EditCondition = "bSetupConnections",
		EditConditionHides))
	bool bUseMaterialAttributes = false;

	/** 连接模式 */
	UPROPERTY(EditAnywhere, Category = "连接", meta = (
		DisplayName = "连接模式",
		ToolTip = "决定函数输出接入材质时的方式：直接连接、Add 叠加或 Multiply 相乘。",
		EditCondition = "bSetupConnections && !bEnableSmartConnect && !bUseMaterialAttributes",
		EditConditionHides))
	EConnectionMode ConnectionMode = EConnectionMode::Add;

	/** 是否连接到BaseColor */
	UPROPERTY(EditAnywhere, Category = "连接目标", meta = (
		DisplayName = "Base Color",
		ToolTip = "将函数输出连接到材质的 Base Color。",
		EditCondition = "bSetupConnections && !bEnableSmartConnect && !bUseMaterialAttributes",
		EditConditionHides))
	bool bConnectToBaseColor = false;

	/** 是否连接到Metallic */
	UPROPERTY(EditAnywhere, Category = "连接目标", meta = (
		DisplayName = "Metallic",
		ToolTip = "将函数输出连接到材质的 Metallic。",
		EditCondition = "bSetupConnections && !bEnableSmartConnect && !bUseMaterialAttributes",
		EditConditionHides))
	bool bConnectToMetallic = false;

	/** 是否连接到Roughness */
	UPROPERTY(EditAnywhere, Category = "连接目标", meta = (
		DisplayName = "Roughness",
		ToolTip = "将函数输出连接到材质的 Roughness。",
		EditCondition = "bSetupConnections && !bEnableSmartConnect && !bUseMaterialAttributes",
		EditConditionHides))
	bool bConnectToRoughness = false;

	/** 是否连接到Normal */
	UPROPERTY(EditAnywhere, Category = "连接目标", meta = (
		DisplayName = "Normal",
		ToolTip = "将函数输出连接到材质的 Normal。",
		EditCondition = "bSetupConnections && !bEnableSmartConnect && !bUseMaterialAttributes",
		EditConditionHides))
	bool bConnectToNormal = false;

	/** 是否连接到Emissive */
	UPROPERTY(EditAnywhere, Category = "连接目标", meta = (
		DisplayName = "Emissive Color",
		ToolTip = "将函数输出连接到材质的 Emissive Color。",
		EditCondition = "bSetupConnections && !bEnableSmartConnect && !bUseMaterialAttributes",
		EditConditionHides))
	bool bConnectToEmissive = false;

	/** 是否连接到AO */
	UPROPERTY(EditAnywhere, Category = "连接目标", meta = (
		DisplayName = "Ambient Occlusion",
		ToolTip = "将函数输出连接到材质的 Ambient Occlusion。",
		EditCondition = "bSetupConnections && !bEnableSmartConnect && !bUseMaterialAttributes",
		EditConditionHides))
	bool bConnectToAO = false;

	/** 构造函数 */
	FX_MaterialFunctionParams()
	{
		// 默认值
		NodeName = TEXT("MaterialFunction");
		PosX = 0;
		PosY = 0;
		bSetupConnections = true;
		bEnableSmartConnect = true;
		ConnectionMode = EConnectionMode::Add;
		bConnectToBaseColor = false;
		bConnectToMetallic = false;
		bConnectToRoughness = false;
		bConnectToNormal = false;
		bConnectToEmissive = false;
		bConnectToAO = false;
		bUseMaterialAttributes = false;
	}

	/** 
	 * 根据材质函数名称自动设置连接选项
	 * @param FunctionName - 材质函数名称
	 */
	void SetupConnectionsByFunctionName(const FString& FunctionName)
	{
		// 重置所有连接
		bConnectToBaseColor = false;
		bConnectToMetallic = false;
		bConnectToRoughness = false;
		bConnectToNormal = false;
		bConnectToEmissive = false;
		bConnectToAO = false;
		bUseMaterialAttributes = false;

		//  优先检测MaterialAttributes函数
		if (FunctionName.Contains(TEXT("MaterialAttributes")) ||
			FunctionName.Contains(TEXT("MA_")) ||
			FunctionName.Contains(TEXT("MakeMA")) ||
			FunctionName.Contains(TEXT("SetMA")) ||
			FunctionName.Contains(TEXT("BlendMA")))
		{
			bUseMaterialAttributes = true;
			// MaterialAttributes模式下禁用其他连接选项
			bSetupConnections = true;
			bEnableSmartConnect = false;  // 使用专用连接逻辑
		}
		// 根据函数名称设置默认连接
		else if (FunctionName.Contains(TEXT("BaseColor")))
		{
			bConnectToBaseColor = true;
		}
		else if (FunctionName.Contains(TEXT("Metallic")))
		{
			bConnectToMetallic = true;
		}
		else if (FunctionName.Contains(TEXT("Roughness")))
		{
			bConnectToRoughness = true;
		}
		else if (FunctionName.Contains(TEXT("Normal")))
		{
			bConnectToNormal = true;
		}
		else if (FunctionName.Contains(TEXT("Emissive")) || FunctionName.Contains(TEXT("Fresnel")))
		{
			bConnectToEmissive = true;
		}
		else if (FunctionName.Contains(TEXT("AO")) || FunctionName.Contains(TEXT("Ambient")))
		{
			bConnectToAO = true;
		}

		// 设置节点名称
		NodeName = FunctionName;
	}
};
