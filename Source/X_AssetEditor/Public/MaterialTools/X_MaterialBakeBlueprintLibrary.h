/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "X_MaterialBakeBlueprintLibrary.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct FX_MaterialBakeVertexColorResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "结果", meta = (DisplayName = "成功"))
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "结果", meta = (DisplayName = "写入顶点实例数量"))
	int32 WrittenVertexInstanceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "结果", meta = (DisplayName = "烘焙材质槽数量"))
	int32 BakedMaterialSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "结果", meta = (DisplayName = "消息"))
	FString Message;
};

/**
 * 材质烘焙工具。
 */
UCLASS()
class X_ASSETEDITOR_API UX_MaterialBakeBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 将静态网格体材质的 BaseColor 烘焙到资产顶点色。
	 *
	 * 注意：该函数会修改 StaticMesh 资产的指定 LOD 源数据，并标记资产为脏；请在测试前复制资产或确认可覆盖。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|资产工具|材质",
		meta = (DisplayName = "烘焙材质颜色到资产顶点色",
			ToolTip = "编辑器工具：将静态网格体各材质槽的BaseColor按UV烘焙，并写入指定LOD的资产顶点色。会修改StaticMesh资产源数据，建议先复制资产再测试。"))
	static FX_MaterialBakeVertexColorResult BakeStaticMeshBaseColorToVertexColors(
		UPARAM(DisplayName = "静态网格体") UStaticMesh* StaticMesh,
		UPARAM(DisplayName = "LOD级别", meta = (ClampMin = "0", UIMin = "0")) int32 LODIndex = 0,
		UPARAM(DisplayName = "烘焙分辨率", meta = (ClampMin = "1", UIMin = "64", ClampMax = "4096", UIMax = "1024")) int32 TextureSize = 512,
		UPARAM(DisplayName = "UV通道", meta = (ClampMin = "0", UIMin = "0")) int32 UVChannel = 0
	);
};
