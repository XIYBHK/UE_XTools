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
	None,
	/** 自动检测和连接 */
	Auto,
	/** 添加节点 */
	Add,
	/** 相乘节点 */
	Multiply
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
	UPROPERTY(EditAnywhere, Category = "基本设置", meta = (DisplayName = "节点名称"))
	FString NodeName;

	/** X坐标位置 */
	UPROPERTY(EditAnywhere, Category = "位置", meta = (DisplayName = "X坐标", ClampMin = "-5000", ClampMax = "5000"))
	int32 PosX = -300;

	/** Y坐标位置 */
	UPROPERTY(EditAnywhere, Category = "位置", meta = (DisplayName = "Y坐标", ClampMin = "-5000", ClampMax = "5000"))
	int32 PosY = 0;

	/** 是否自动设置连接 */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "自动连接到材质属性"))
	bool bSetupConnections = true;

	/** 是否启用自动连接逻辑 */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "启用智能连接", EditCondition = "bSetupConnections"))
	bool bEnableSmartConnect = true;

	/** 连接模式 */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "连接模式", EditCondition = "bSetupConnections && !bEnableSmartConnect"))
	EConnectionMode ConnectionMode = EConnectionMode::Add;

	/** 是否连接到BaseColor */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "连接到基础颜色", EditCondition = "bSetupConnections && !bEnableSmartConnect"))
	bool bConnectToBaseColor = false;

	/** 是否连接到Metallic */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "连接到金属度", EditCondition = "bSetupConnections && !bEnableSmartConnect"))
	bool bConnectToMetallic = false;

	/** 是否连接到Roughness */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "连接到粗糙度", EditCondition = "bSetupConnections && !bEnableSmartConnect"))
	bool bConnectToRoughness = false;

	/** 是否连接到Normal */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "连接到法线", EditCondition = "bSetupConnections && !bEnableSmartConnect"))
	bool bConnectToNormal = false;

	/** 是否连接到Emissive */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "连接到自发光", EditCondition = "bSetupConnections && !bEnableSmartConnect"))
	bool bConnectToEmissive = false;

	/** 是否连接到AO */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "连接到环境光遮蔽", EditCondition = "bSetupConnections && !bEnableSmartConnect"))
	bool bConnectToAO = false;

	/** 是否强制使用MaterialAttributes连接 */
	UPROPERTY(EditAnywhere, Category = "连接设置", meta = (DisplayName = "使用材质属性连接"))
	bool bUseMaterialAttributes = false;

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