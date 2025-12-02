#include "K2Node_PointSamplingPinManager.h"

#if WITH_EDITOR

#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SplineComponent.h"

#define LOCTEXT_NAMESPACE "K2Node_PointSampling"


// ============================================================================
// 引脚名称常量定义
// ============================================================================

const FName FPointSamplingPinNames::PN_Execute(TEXT("Execute"));
const FName FPointSamplingPinNames::PN_Then(TEXT("Then"));

const FName FPointSamplingPinNames::PN_SamplingMode(TEXT("SamplingMode"));
const FName FPointSamplingPinNames::PN_PointCount(TEXT("PointCount"));
const FName FPointSamplingPinNames::PN_CenterLocation(TEXT("CenterLocation"));
const FName FPointSamplingPinNames::PN_Rotation(TEXT("Rotation"));
const FName FPointSamplingPinNames::PN_CoordinateSpace(TEXT("CoordinateSpace"));
const FName FPointSamplingPinNames::PN_Spacing(TEXT("Spacing"));
const FName FPointSamplingPinNames::PN_JitterStrength(TEXT("JitterStrength"));
const FName FPointSamplingPinNames::PN_UseCache(TEXT("bUseCache"));
const FName FPointSamplingPinNames::PN_RandomSeed(TEXT("RandomSeed"));
const FName FPointSamplingPinNames::PN_OutputPositions(TEXT("OutputPositions"));

const FName FPointSamplingPinNames::PN_RowCount(TEXT("RowCount"));
const FName FPointSamplingPinNames::PN_ColumnCount(TEXT("ColumnCount"));
const FName FPointSamplingPinNames::PN_Height(TEXT("Height"));

const FName FPointSamplingPinNames::PN_InvertedTriangle(TEXT("bInvertedTriangle"));

const FName FPointSamplingPinNames::PN_Radius(TEXT("Radius"));
const FName FPointSamplingPinNames::PN_Is3D(TEXT("bIs3D"));
const FName FPointSamplingPinNames::PN_DistributionMode(TEXT("DistributionMode"));
const FName FPointSamplingPinNames::PN_MinDistance(TEXT("MinDistance"));
const FName FPointSamplingPinNames::PN_StartAngle(TEXT("StartAngle"));
const FName FPointSamplingPinNames::PN_Clockwise(TEXT("bClockwise"));

const FName FPointSamplingPinNames::PN_SpiralTurns(TEXT("SpiralTurns"));

const FName FPointSamplingPinNames::PN_SnowflakeBranches(TEXT("SnowflakeBranches"));
const FName FPointSamplingPinNames::PN_SnowflakeLayers(TEXT("SnowflakeLayers"));

const FName FPointSamplingPinNames::PN_SplineComponent(TEXT("SplineComponent"));
const FName FPointSamplingPinNames::PN_ClosedSpline(TEXT("bClosedSpline"));

const FName FPointSamplingPinNames::PN_StaticMesh(TEXT("StaticMesh"));
const FName FPointSamplingPinNames::PN_LODLevel(TEXT("LODLevel"));
const FName FPointSamplingPinNames::PN_BoundaryVerticesOnly(TEXT("bBoundaryVerticesOnly"));

const FName FPointSamplingPinNames::PN_SkeletalMesh(TEXT("SkeletalMesh"));
const FName FPointSamplingPinNames::PN_SocketNamePrefix(TEXT("SocketNamePrefix"));

const FName FPointSamplingPinNames::PN_Texture(TEXT("Texture"));
const FName FPointSamplingPinNames::PN_MaxSampleSize(TEXT("MaxSampleSize"));
const FName FPointSamplingPinNames::PN_TextureSpacing(TEXT("Spacing"));
const FName FPointSamplingPinNames::PN_PixelThreshold(TEXT("PixelThreshold"));
const FName FPointSamplingPinNames::PN_TextureScale(TEXT("TextureScale"));

// ============================================================================
// 公共方法实现
// ============================================================================

void FPointSamplingPinManager::CreateBasePins(UK2Node* Node)
{
	if (!Node) return;

	// 创建执行引脚
	Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, FPointSamplingPinNames::PN_Execute);
	Node->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FPointSamplingPinNames::PN_Then);

	// 创建采样模式引脚（枚举）
	UEdGraphPin* ModePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte,
		StaticEnum<EPointSamplingMode>(), FPointSamplingPinNames::PN_SamplingMode);
	ModePin->PinToolTip = LOCTEXT("ModePin_Tooltip", "选择点采样模式").ToString();
	SetEnumPinDefaultValue(ModePin, StaticEnum<EPointSamplingMode>());

	// 创建坐标空间引脚（所有模式都需要）
	UEdGraphPin* SpacePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte,
		StaticEnum<EPoissonCoordinateSpace>(), FPointSamplingPinNames::PN_CoordinateSpace);
	SpacePin->PinToolTip = LOCTEXT("CoordinateSpace_Tooltip", "坐标空间类型").ToString();
	SetEnumPinDefaultValue(SpacePin, StaticEnum<EPoissonCoordinateSpace>());

	// 创建输出位置数组引脚
	UEdGraphPin* OutputPin = Node->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct,
		TBaseStructure<FVector>::Get(), FPointSamplingPinNames::PN_OutputPositions);
	OutputPin->PinType.ContainerType = EPinContainerType::Array;
	OutputPin->PinToolTip = LOCTEXT("OutputPositions_Tooltip", "生成的点位置数组").ToString();
}

void FPointSamplingPinManager::RebuildDynamicPins(UK2Node* Node, EPointSamplingMode SamplingMode)
{
	if (!Node) return;

	// 先清除现有动态引脚
	ClearDynamicPins(Node);

	// 创建通用参数引脚（根据模式按需创建）
	CreateCommonPins(Node, SamplingMode);

	// 根据采样模式创建对应的模式特定引脚
	switch (SamplingMode)
	{
	case EPointSamplingMode::SolidRectangle:
	case EPointSamplingMode::HollowRectangle:
		CreateRectanglePins(Node, SamplingMode);
		break;

	case EPointSamplingMode::SpiralRectangle:
		CreateRectanglePins(Node, SamplingMode);
		CreateSpiralPins(Node, SamplingMode);
		break;

	case EPointSamplingMode::SolidTriangle:
	case EPointSamplingMode::HollowTriangle:
		CreateTrianglePins(Node, SamplingMode);
		break;

	case EPointSamplingMode::Circle:
		CreateCirclePins(Node);
		break;

	case EPointSamplingMode::Snowflake:
	case EPointSamplingMode::SnowflakeArc:
		CreateSnowflakePins(Node, SamplingMode);
		break;

	case EPointSamplingMode::Spline:
		CreateSplinePins(Node);
		break;

	case EPointSamplingMode::SplineBoundary:
		CreateSplineBoundaryPins(Node);
		break;

	case EPointSamplingMode::StaticMeshVertices:
		CreateStaticMeshPins(Node);
		break;

	case EPointSamplingMode::SkeletalSockets:
		CreateSkeletalMeshPins(Node);
		break;

	case EPointSamplingMode::TexturePixels:
		CreateTexturePins(Node);
		break;
	}
}

void FPointSamplingPinManager::ClearDynamicPins(UK2Node* Node)
{
	if (!Node) return;

	TArray<FName> DynamicPinNames = GetDynamicPinNames();

	// 反向遍历以安全删除
	for (int32 i = Node->Pins.Num() - 1; i >= 0; --i)
	{
		if (UEdGraphPin* Pin = Node->Pins[i])
		{
			if (DynamicPinNames.Contains(Pin->PinName))
			{
				Node->RemovePin(Pin);
			}
		}
	}
}

bool FPointSamplingPinManager::IsDynamicPin(const FName& PinName)
{
	return GetDynamicPinNames().Contains(PinName);
}

bool FPointSamplingPinManager::SamplingModeNeedsPin(EPointSamplingMode SamplingMode, const FName& PinName)
{
	// 通用参数 - PointCount
	if (PinName == FPointSamplingPinNames::PN_PointCount)
	{
		return SamplingMode != EPointSamplingMode::StaticMeshVertices &&
			   SamplingMode != EPointSamplingMode::SkeletalSockets &&
			   SamplingMode != EPointSamplingMode::TexturePixels;
	}

	// 通用参数 - CenterLocation
	if (PinName == FPointSamplingPinNames::PN_CenterLocation)
	{
		return SamplingMode != EPointSamplingMode::Spline &&
			   SamplingMode != EPointSamplingMode::SplineBoundary &&
			   SamplingMode != EPointSamplingMode::StaticMeshVertices &&
			   SamplingMode != EPointSamplingMode::SkeletalSockets;
	}

	// 通用参数 - Rotation
	if (PinName == FPointSamplingPinNames::PN_Rotation)
	{
		return SamplingMode != EPointSamplingMode::Spline &&
			   SamplingMode != EPointSamplingMode::SplineBoundary &&
			   SamplingMode != EPointSamplingMode::StaticMeshVertices &&
			   SamplingMode != EPointSamplingMode::SkeletalSockets;
	}

	// 通用参数 - Spacing
	if (PinName == FPointSamplingPinNames::PN_Spacing)
	{
		return SamplingMode == EPointSamplingMode::SolidRectangle ||
			   SamplingMode == EPointSamplingMode::HollowRectangle ||
			   SamplingMode == EPointSamplingMode::SpiralRectangle ||
			   SamplingMode == EPointSamplingMode::SolidTriangle ||
			   SamplingMode == EPointSamplingMode::HollowTriangle ||
			   SamplingMode == EPointSamplingMode::Snowflake ||
			   SamplingMode == EPointSamplingMode::SnowflakeArc;
	}

	// 通用参数 - JitterStrength
	if (PinName == FPointSamplingPinNames::PN_JitterStrength)
	{
		return SamplingMode != EPointSamplingMode::Spline &&
			   SamplingMode != EPointSamplingMode::SplineBoundary &&
			   SamplingMode != EPointSamplingMode::StaticMeshVertices &&
			   SamplingMode != EPointSamplingMode::SkeletalSockets &&
			   SamplingMode != EPointSamplingMode::TexturePixels;
	}

	// 通用参数 - RandomSeed
	if (PinName == FPointSamplingPinNames::PN_RandomSeed)
	{
		return SamplingMode != EPointSamplingMode::Spline &&
			   SamplingMode != EPointSamplingMode::SplineBoundary &&
			   SamplingMode != EPointSamplingMode::StaticMeshVertices &&
			   SamplingMode != EPointSamplingMode::SkeletalSockets;
	}

	// 矩形参数
	if (PinName == FPointSamplingPinNames::PN_RowCount ||
		PinName == FPointSamplingPinNames::PN_ColumnCount ||
		PinName == FPointSamplingPinNames::PN_Height)
	{
		return SamplingMode == EPointSamplingMode::SolidRectangle ||
			   SamplingMode == EPointSamplingMode::HollowRectangle ||
			   SamplingMode == EPointSamplingMode::SpiralRectangle;
	}

	// 三角形参数
	if (PinName == FPointSamplingPinNames::PN_InvertedTriangle)
	{
		return SamplingMode == EPointSamplingMode::SolidTriangle ||
			   SamplingMode == EPointSamplingMode::HollowTriangle;
	}

	// 圆形参数
	if (PinName == FPointSamplingPinNames::PN_Radius ||
		PinName == FPointSamplingPinNames::PN_Is3D ||
		PinName == FPointSamplingPinNames::PN_DistributionMode ||
		PinName == FPointSamplingPinNames::PN_StartAngle ||
		PinName == FPointSamplingPinNames::PN_Clockwise)
	{
		return SamplingMode == EPointSamplingMode::Circle;
	}

	// MinDistance参数（圆形和样条线边界共用）
	if (PinName == FPointSamplingPinNames::PN_MinDistance)
	{
		return SamplingMode == EPointSamplingMode::Circle ||
			   SamplingMode == EPointSamplingMode::SplineBoundary;
	}

	// 螺旋参数
	if (PinName == FPointSamplingPinNames::PN_SpiralTurns)
	{
		return SamplingMode == EPointSamplingMode::SpiralRectangle;
	}

	// 雪花参数
	if (PinName == FPointSamplingPinNames::PN_SnowflakeBranches || PinName == FPointSamplingPinNames::PN_SnowflakeLayers)
	{
		return SamplingMode == EPointSamplingMode::Snowflake ||
			   SamplingMode == EPointSamplingMode::SnowflakeArc;
	}

	// 样条组件参数（样条线和样条线边界共用）
	if (PinName == FPointSamplingPinNames::PN_SplineComponent)
	{
		return SamplingMode == EPointSamplingMode::Spline ||
			   SamplingMode == EPointSamplingMode::SplineBoundary;
	}

	// 闭合样条线参数（仅Spline模式）
	if (PinName == FPointSamplingPinNames::PN_ClosedSpline)
	{
		return SamplingMode == EPointSamplingMode::Spline;
	}

	// 静态网格体参数
	if (PinName == FPointSamplingPinNames::PN_StaticMesh || PinName == FPointSamplingPinNames::PN_LODLevel || PinName == FPointSamplingPinNames::PN_BoundaryVerticesOnly)
	{
		return SamplingMode == EPointSamplingMode::StaticMeshVertices;
	}

	// 骨骼网格体参数
	if (PinName == FPointSamplingPinNames::PN_SkeletalMesh || PinName == FPointSamplingPinNames::PN_SocketNamePrefix)
	{
		return SamplingMode == EPointSamplingMode::SkeletalSockets;
	}

	// 纹理参数
	if (PinName == FPointSamplingPinNames::PN_Texture ||
		PinName == FPointSamplingPinNames::PN_MaxSampleSize ||
		PinName == FPointSamplingPinNames::PN_TextureSpacing ||
		PinName == FPointSamplingPinNames::PN_PixelThreshold ||
		PinName == FPointSamplingPinNames::PN_TextureScale)
	{
		return SamplingMode == EPointSamplingMode::TexturePixels;
	}

	return false;
}

void FPointSamplingPinManager::SetEnumPinDefaultValue(UEdGraphPin* EnumPin, UEnum* EnumClass)
{
	if (!EnumPin || !EnumClass) return;

	// 如果引脚已经有默认值，不要覆盖
	if (!EnumPin->DefaultValue.IsEmpty()) return;

	// 设置为枚举的第一项（排除MAX值）
	if (EnumClass->NumEnums() > 1)
	{
		FString FirstEnumName = EnumClass->GetNameStringByIndex(0);
		EnumPin->DefaultValue = FirstEnumName;
	}
}

// ============================================================================
// 私有方法实现 - 创建特定类型的引脚
// ============================================================================

void FPointSamplingPinManager::CreateCommonPins(UK2Node* Node, EPointSamplingMode SamplingMode)
{
	if (!Node) return;

	// PointCount - 重用 SamplingModeNeedsPin 判断逻辑
	if (SamplingModeNeedsPin(SamplingMode, FPointSamplingPinNames::PN_PointCount))
	{
		UEdGraphPin* PointCountPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_PointCount);
		PointCountPin->DefaultValue = TEXT("-1");
		PointCountPin->PinToolTip = LOCTEXT("PointCount_Tooltip", "生成的点数量（-1表示由行列/半径等参数控制）").ToString();
	}

	// CenterLocation
	if (SamplingModeNeedsPin(SamplingMode, FPointSamplingPinNames::PN_CenterLocation))
	{
		UEdGraphPin* CenterPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct,
			TBaseStructure<FVector>::Get(), FPointSamplingPinNames::PN_CenterLocation);
		CenterPin->PinToolTip = LOCTEXT("CenterLocation_Tooltip", "点阵中心位置").ToString();
	}

	// Rotation
	if (SamplingModeNeedsPin(SamplingMode, FPointSamplingPinNames::PN_Rotation))
	{
		UEdGraphPin* RotationPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct,
			TBaseStructure<FRotator>::Get(), FPointSamplingPinNames::PN_Rotation);
		RotationPin->PinToolTip = LOCTEXT("Rotation_Tooltip", "点阵旋转").ToString();
	}

	// Spacing
	if (SamplingModeNeedsPin(SamplingMode, FPointSamplingPinNames::PN_Spacing))
	{
		UEdGraphPin* SpacingPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_Spacing);
		SpacingPin->DefaultValue = TEXT("100.0");
		SpacingPin->PinToolTip = LOCTEXT("Spacing_Tooltip", "点之间的间距").ToString();
	}

	// JitterStrength
	if (SamplingModeNeedsPin(SamplingMode, FPointSamplingPinNames::PN_JitterStrength))
	{
		UEdGraphPin* JitterPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_JitterStrength);
		JitterPin->DefaultValue = TEXT("0.0");
		JitterPin->PinToolTip = LOCTEXT("JitterStrength_Tooltip", "噪波扰动强度 (0-1)").ToString();
		JitterPin->bAdvancedView = true;
	}

	// RandomSeed
	if (SamplingModeNeedsPin(SamplingMode, FPointSamplingPinNames::PN_RandomSeed))
	{
		UEdGraphPin* SeedPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_RandomSeed);
		SeedPin->DefaultValue = TEXT("0");
		SeedPin->PinToolTip = LOCTEXT("RandomSeed_Tooltip", "随机种子").ToString();
		SeedPin->bAdvancedView = true;
	}
}

void FPointSamplingPinManager::CreateRectanglePins(UK2Node* Node, EPointSamplingMode SamplingMode)
{
	if (!Node) return;

	UEdGraphPin* RowPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_RowCount);
	RowPin->DefaultValue = TEXT("5");
	RowPin->PinToolTip = LOCTEXT("RowCount_Tooltip", "行数（0表示由PointCount自动计算）").ToString();

	UEdGraphPin* ColPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_ColumnCount);
	ColPin->DefaultValue = TEXT("5");
	ColPin->PinToolTip = LOCTEXT("ColumnCount_Tooltip", "列数（0表示由PointCount自动计算）").ToString();

	UEdGraphPin* HeightPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_Height);
	HeightPin->DefaultValue = TEXT("1.0");
	HeightPin->PinToolTip = LOCTEXT("Height_Tooltip", "高度，支持3D矩形点阵（1=2D平面）").ToString();
	HeightPin->bAdvancedView = true;
}

void FPointSamplingPinManager::CreateTrianglePins(UK2Node* Node, EPointSamplingMode SamplingMode)
{
	if (!Node) return;

	UEdGraphPin* InvertedPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, FPointSamplingPinNames::PN_InvertedTriangle);
	InvertedPin->DefaultValue = TEXT("false");
	InvertedPin->PinToolTip = LOCTEXT("InvertedTriangle_Tooltip", "是否为倒三角").ToString();
}

void FPointSamplingPinManager::CreateCirclePins(UK2Node* Node)
{
	if (!Node) return;

	UEdGraphPin* RadiusPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_Radius);
	RadiusPin->DefaultValue = TEXT("200.0");
	RadiusPin->PinToolTip = LOCTEXT("Radius_Tooltip", "圆形/球体半径").ToString();

	UEdGraphPin* Is3DPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, FPointSamplingPinNames::PN_Is3D);
	Is3DPin->DefaultValue = TEXT("false");
	Is3DPin->PinToolTip = LOCTEXT("Is3D_Tooltip", "是否为3D球体（false=2D圆形，true=3D球体）").ToString();

	UEdGraphPin* DistributionPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte,
		StaticEnum<ECircleDistributionMode>(), FPointSamplingPinNames::PN_DistributionMode);
	DistributionPin->PinToolTip = LOCTEXT("DistributionMode_Tooltip", "分布模式（均匀/斐波那契/泊松）").ToString();
	SetEnumPinDefaultValue(DistributionPin, StaticEnum<ECircleDistributionMode>());

	UEdGraphPin* MinDistancePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_MinDistance);
	MinDistancePin->DefaultValue = TEXT("50.0");
	MinDistancePin->PinToolTip = LOCTEXT("MinDistance_Tooltip", "泊松分布的最小距离（仅Poisson模式有效）").ToString();
	MinDistancePin->bAdvancedView = true;

	UEdGraphPin* StartAnglePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_StartAngle);
	StartAnglePin->DefaultValue = TEXT("0.0");
	StartAnglePin->PinToolTip = LOCTEXT("StartAngle_Tooltip", "起始角度（度，仅Uniform模式2D有效）").ToString();
	StartAnglePin->bAdvancedView = true;

	UEdGraphPin* ClockwisePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, FPointSamplingPinNames::PN_Clockwise);
	ClockwisePin->DefaultValue = TEXT("true");
	ClockwisePin->PinToolTip = LOCTEXT("Clockwise_Tooltip", "是否顺时针排列（仅Uniform模式2D有效）").ToString();
	ClockwisePin->bAdvancedView = true;
}

void FPointSamplingPinManager::CreateSpiralPins(UK2Node* Node, EPointSamplingMode SamplingMode)
{
	if (!Node) return;

	UEdGraphPin* TurnsPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_SpiralTurns);
	TurnsPin->DefaultValue = TEXT("2.0");
	TurnsPin->PinToolTip = LOCTEXT("SpiralTurns_Tooltip", "螺旋圈数").ToString();
}

void FPointSamplingPinManager::CreateSnowflakePins(UK2Node* Node, EPointSamplingMode SamplingMode)
{
	if (!Node) return;

	UEdGraphPin* BranchesPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_SnowflakeBranches);
	BranchesPin->DefaultValue = TEXT("6");
	BranchesPin->PinToolTip = LOCTEXT("SnowflakeBranches_Tooltip", "雪花分支数量").ToString();

	UEdGraphPin* LayersPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_SnowflakeLayers);
	LayersPin->DefaultValue = TEXT("3");
	LayersPin->PinToolTip = LOCTEXT("SnowflakeLayers_Tooltip", "雪花层数").ToString();
}

void FPointSamplingPinManager::CreateSplinePins(UK2Node* Node)
{
	if (!Node) return;

	// 修改为样条组件类型
	UEdGraphPin* SplineComponentPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object,
		USplineComponent::StaticClass(), FPointSamplingPinNames::PN_SplineComponent);
	SplineComponentPin->PinToolTip = LOCTEXT("SplineComponent_Tooltip", "样条组件引用").ToString();

	UEdGraphPin* ClosedPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, FPointSamplingPinNames::PN_ClosedSpline);
	ClosedPin->DefaultValue = TEXT("false");
	ClosedPin->PinToolTip = LOCTEXT("ClosedSpline_Tooltip", "是否闭合样条线").ToString();
}

void FPointSamplingPinManager::CreateSplineBoundaryPins(UK2Node* Node)
{
	if (!Node) return;

	// 修改为样条组件类型
	UEdGraphPin* SplineComponentPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object,
		USplineComponent::StaticClass(), FPointSamplingPinNames::PN_SplineComponent);
	SplineComponentPin->PinToolTip = LOCTEXT("SplineComponent_Tooltip", "样条组件引用").ToString();

	UEdGraphPin* MinDistancePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_MinDistance);
	MinDistancePin->DefaultValue = TEXT("50.0");
	MinDistancePin->PinToolTip = LOCTEXT("MinDistance_Boundary_Tooltip", "泊松采样最小点间距（<=0时自动计算）").ToString();
	MinDistancePin->bAdvancedView = true;
}

void FPointSamplingPinManager::CreateStaticMeshPins(UK2Node* Node)
{
	if (!Node) return;

	UEdGraphPin* MeshPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object,
		UStaticMesh::StaticClass(), FPointSamplingPinNames::PN_StaticMesh);
	MeshPin->PinToolTip = LOCTEXT("StaticMesh_Tooltip", "静态网格体引用").ToString();

	UEdGraphPin* LODPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_LODLevel);
	LODPin->DefaultValue = TEXT("0");
	LODPin->PinToolTip = LOCTEXT("LODLevel_Tooltip", "LOD 级别").ToString();

	UEdGraphPin* BoundaryPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, FPointSamplingPinNames::PN_BoundaryVerticesOnly);
	BoundaryPin->DefaultValue = TEXT("false");
	BoundaryPin->PinToolTip = LOCTEXT("BoundaryVerticesOnly_Tooltip", "仅使用边界顶点").ToString();
}

void FPointSamplingPinManager::CreateSkeletalMeshPins(UK2Node* Node)
{
	if (!Node) return;

	UEdGraphPin* MeshPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object,
		USkeletalMesh::StaticClass(), FPointSamplingPinNames::PN_SkeletalMesh);
	MeshPin->PinToolTip = LOCTEXT("SkeletalMesh_Tooltip", "骨骼网格体引用").ToString();

	UEdGraphPin* PrefixPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, FPointSamplingPinNames::PN_SocketNamePrefix);
	PrefixPin->DefaultValue = TEXT("");
	PrefixPin->PinToolTip = LOCTEXT("SocketNamePrefix_Tooltip", "插槽名称前缀过滤").ToString();
}

void FPointSamplingPinManager::CreateTexturePins(UK2Node* Node)
{
	if (!Node) return;

	UEdGraphPin* TexturePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object,
		UTexture2D::StaticClass(), FPointSamplingPinNames::PN_Texture);
	TexturePin->PinToolTip = LOCTEXT("Texture_Tooltip", "纹理引用").ToString();

	UEdGraphPin* MaxSampleSizePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, FPointSamplingPinNames::PN_MaxSampleSize);
	MaxSampleSizePin->DefaultValue = TEXT("512");
	MaxSampleSizePin->PinToolTip = LOCTEXT("MaxSampleSize_Tooltip", "最大采样尺寸（纹理会被智能降采样到此尺寸，控制最大点数量）").ToString();

	UEdGraphPin* SpacingPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_TextureSpacing);
	SpacingPin->DefaultValue = TEXT("20.0");
	SpacingPin->PinToolTip = LOCTEXT("TextureSpacing_Tooltip", "像素步长（采样间隔，建议范围10-50，值越大点越稀疏）").ToString();

	UEdGraphPin* ThresholdPin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_PixelThreshold);
	ThresholdPin->DefaultValue = TEXT("0.5");
	ThresholdPin->PinToolTip = LOCTEXT("PixelThreshold_Tooltip", "像素采样阈值 (0-1)").ToString();
	ThresholdPin->bAdvancedView = true;

	UEdGraphPin* ScalePin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, FPointSamplingPinNames::PN_TextureScale);
	ScalePin->DefaultValue = TEXT("1.0");
	ScalePin->PinToolTip = LOCTEXT("TextureScale_Tooltip", "图片缩放（影响生成点位的物理尺寸）").ToString();
	ScalePin->bAdvancedView = true;
}

TArray<FName> FPointSamplingPinManager::GetDynamicPinNames()
{
	return {
		// 通用参数（现在也是动态的）
		FPointSamplingPinNames::PN_PointCount,
		FPointSamplingPinNames::PN_CenterLocation,
		FPointSamplingPinNames::PN_Rotation,
		FPointSamplingPinNames::PN_Spacing,
		FPointSamplingPinNames::PN_JitterStrength,
		FPointSamplingPinNames::PN_RandomSeed,
		// 模式特定参数
		FPointSamplingPinNames::PN_RowCount, FPointSamplingPinNames::PN_ColumnCount, FPointSamplingPinNames::PN_Height,
		FPointSamplingPinNames::PN_InvertedTriangle,
		FPointSamplingPinNames::PN_Radius, FPointSamplingPinNames::PN_Is3D,
		FPointSamplingPinNames::PN_DistributionMode, FPointSamplingPinNames::PN_MinDistance,
		FPointSamplingPinNames::PN_StartAngle, FPointSamplingPinNames::PN_Clockwise,
		FPointSamplingPinNames::PN_SpiralTurns,
		FPointSamplingPinNames::PN_SnowflakeBranches, FPointSamplingPinNames::PN_SnowflakeLayers,
		FPointSamplingPinNames::PN_SplineComponent, FPointSamplingPinNames::PN_ClosedSpline,
		FPointSamplingPinNames::PN_StaticMesh, FPointSamplingPinNames::PN_LODLevel, FPointSamplingPinNames::PN_BoundaryVerticesOnly,
		FPointSamplingPinNames::PN_SkeletalMesh, FPointSamplingPinNames::PN_SocketNamePrefix,
		FPointSamplingPinNames::PN_Texture, FPointSamplingPinNames::PN_MaxSampleSize, FPointSamplingPinNames::PN_TextureSpacing,
		FPointSamplingPinNames::PN_PixelThreshold, FPointSamplingPinNames::PN_TextureScale
	};
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
