#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.h"

class UEdGraphPin;
class UK2Node;
class UEnum;

/**
 * @brief 点采样节点的引脚名称常量
 * 集中管理所有引脚名称，避免魔法字符串
 */
struct FPointSamplingPinNames
{
	// 执行引脚
	static const FName PN_Execute;
	static const FName PN_Then;

	// 基础参数
	static const FName PN_SamplingMode;
	static const FName PN_PointCount;
	static const FName PN_CenterLocation;
	static const FName PN_Rotation;
	static const FName PN_CoordinateSpace;
	static const FName PN_Spacing;
	static const FName PN_JitterStrength;
	static const FName PN_UseCache;
	static const FName PN_RandomSeed;
	static const FName PN_OutputPositions;

	// 矩形参数
	static const FName PN_RowCount;
	static const FName PN_ColumnCount;
	static const FName PN_Height;

	// 三角形参数
	static const FName PN_InvertedTriangle;

	// 圆形参数
	static const FName PN_Radius;
	static const FName PN_Is3D;
	static const FName PN_DistributionMode;
	static const FName PN_MinDistance;
	static const FName PN_StartAngle;
	static const FName PN_Clockwise;

	// 螺旋参数
	static const FName PN_SpiralTurns;

	// 雪花参数
	static const FName PN_SnowflakeBranches;
	static const FName PN_SnowflakeLayers;

	// 样条线参数
	static const FName PN_SplineComponent;
	static const FName PN_ClosedSpline;

	// 静态网格体参数
	static const FName PN_StaticMesh;
	static const FName PN_LODLevel;
	static const FName PN_BoundaryVerticesOnly;

	// 骨骼网格体参数
	static const FName PN_SkeletalMesh;
	static const FName PN_SocketNamePrefix;

	// 纹理参数
	static const FName PN_Texture;
	static const FName PN_MaxSampleSize;
	static const FName PN_TextureSpacing;
	static const FName PN_PixelThreshold;
	static const FName PN_TextureScale;
};

/**
 * @brief 点采样节点引脚管理器
 * 职责：动态创建和管理引脚，根据采样模式显示/隐藏相应的参数引脚
 *
 * 设计原则：
 * - 单一职责：仅负责引脚的创建、查找和管理
 * - 无状态：不持有节点状态，所有操作基于传入的节点指针
 * - 可测试：所有方法都是静态的，易于单元测试
 */
class FPointSamplingPinManager
{
public:
	/**
	 * @brief 创建所有基础引脚（执行引脚、通用参数引脚）
	 * @param Node 目标节点
	 */
	static void CreateBasePins(UK2Node* Node);

	/**
	 * @brief 根据采样模式重建动态引脚
	 * @param Node 目标节点
	 * @param SamplingMode 当前采样模式
	 */
	static void RebuildDynamicPins(UK2Node* Node, EPointSamplingMode SamplingMode);

	/**
	 * @brief 清除所有动态引脚（保留基础引脚）
	 * @param Node 目标节点
	 */
	static void ClearDynamicPins(UK2Node* Node);

	/**
	 * @brief 判断引脚是否为动态引脚
	 * @param PinName 引脚名称
	 * @return 是否为动态引脚
	 */
	static bool IsDynamicPin(const FName& PinName);

	/**
	 * @brief 判断指定采样模式是否需要某个参数引脚
	 * @param SamplingMode 采样模式
	 * @param PinName 引脚名称
	 * @return 是否需要该引脚
	 */
	static bool SamplingModeNeedsPin(EPointSamplingMode SamplingMode, const FName& PinName);

	/**
	 * @brief 设置枚举引脚的默认值为第一项
	 * @param EnumPin 枚举引脚
	 * @param EnumClass 枚举类
	 */
	static void SetEnumPinDefaultValue(UEdGraphPin* EnumPin, UEnum* EnumClass);

private:
	// 创建通用参数引脚（根据模式按需创建）
	static void CreateCommonPins(UK2Node* Node, EPointSamplingMode SamplingMode);

	// 创建特定采样模式的引脚
	static void CreateRectanglePins(UK2Node* Node, EPointSamplingMode SamplingMode);
	static void CreateTrianglePins(UK2Node* Node, EPointSamplingMode SamplingMode);
	static void CreateCirclePins(UK2Node* Node);
	static void CreateSpiralPins(UK2Node* Node, EPointSamplingMode SamplingMode);
	static void CreateSnowflakePins(UK2Node* Node, EPointSamplingMode SamplingMode);
	static void CreateSplinePins(UK2Node* Node);
	static void CreateSplineBoundaryPins(UK2Node* Node);
	static void CreateStaticMeshPins(UK2Node* Node);
	static void CreateSkeletalMeshPins(UK2Node* Node);
	static void CreateTexturePins(UK2Node* Node);

	// 获取动态引脚名称列表
	static TArray<FName> GetDynamicPinNames();
};
