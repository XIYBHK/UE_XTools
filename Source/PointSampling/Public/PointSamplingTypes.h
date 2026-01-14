/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/


#pragma once

#include "CoreMinimal.h"
#include "PointSamplingTypes.generated.h"

/**
 * 泊松采样坐标空间类型
 *
 * 定义泊松采样生成点位的坐标系统。不同坐标空间影响 AddInstance 时的行为：
 * - World: 返回世界绝对坐标（已应用 Transform 的位置 + 旋转）
 * - Local/Raw: 返回相对坐标（仅补偿缩放，位置 + 旋转由父组件处理）
 *
 * 注意：Local 与 Raw 当前行为相同，保留 Raw 枚举值以便未来扩展。
 */
UENUM(BlueprintType)
enum class EPoissonCoordinateSpace : uint8
{
	/** 世界空间 - 返回世界绝对坐标（应用 Transform 的位置 + 旋转）
	 *
	 * 适用场景：
	 * - HISMC.AddInstance(Point, WorldSpace = true)
	 * - SpawnActor(Point) 直接作为世界坐标
	 *
	 * 缓存行为：缓存键包含 Position + Rotation（移动或旋转组件会使缓存失效）
	 */
	World		UMETA(DisplayName = "世界空间"),

	/** 局部空间 - 返回相对于 Transform 中心的坐标（补偿缩放，位置 + 旋转由父组件处理）
	 *
	 * 适用场景（推荐）：
	 * - HISMC.AddInstance(Point, WorldSpace = false)
	 * - 子组件相对父组件放置
	 *
	 * 缓存行为：缓存键包含 Scale（改变缩放会使缓存失效，位置/旋转不影响缓存）
	 *
	 * 注意：当前与 Raw 行为相同，用于语义清晰性
	 */
	Local		UMETA(DisplayName = "局部空间"),

	/** 原始空间 - 返回算法原始输出（补偿缩放，未应用位置 + 旋转）
	 *
	 * 适用场景：
	 * - 需要自行处理坐标变换的高级用法
	 * - 批量处理点位后再应用 Transform
	 *
	 * 缓存行为：缓存键包含 Scale（改变缩放会使缓存失效，位置/旋转不影响缓存）
	 *
	 * 注意：当前与 Local 行为相同，保留以便未来扩展为完全未变换的坐标
	 */
	Raw			UMETA(DisplayName = "原始空间")
};

/**
 * 点采样模式枚举
 * 定义各种点阵生成算法
 */
UENUM(BlueprintType)
enum class EPointSamplingMode : uint8
{
	/** 实心矩形阵型 */
	SolidRectangle      UMETA(DisplayName = "实心矩形"),

	/** 空心矩形阵型 */
	HollowRectangle     UMETA(DisplayName = "空心矩形"),

	/** 螺旋矩形阵型 */
	SpiralRectangle     UMETA(DisplayName = "螺旋矩形"),

	/** 实心三角形阵型 */
	SolidTriangle       UMETA(DisplayName = "实心三角形"),

	/** 空心三角形阵型 */
	HollowTriangle      UMETA(DisplayName = "空心三角形"),

	/** 圆形阵型 */
	Circle              UMETA(DisplayName = "圆形"),

	/** 雪花形阵型 */
	Snowflake           UMETA(DisplayName = "雪花形"),

	/** 雪花弧形阵型 */
	SnowflakeArc        UMETA(DisplayName = "雪花弧形"),

	/** 样条线阵型 */
	Spline              UMETA(DisplayName = "样条线"),

	/** 样条线边界泊松采样 */
	SplineBoundary      UMETA(DisplayName = "样条线边界"),

	/** 静态网格体顶点阵型 */
	StaticMeshVertices  UMETA(DisplayName = "静态模型顶点"),

	/** 骨骼插槽阵型 */
	SkeletalSockets     UMETA(DisplayName = "骨骼插槽"),

	/** 图片像素阵型 */
	TexturePixels       UMETA(DisplayName = "图片像素"),

	// ============================================================================
	// 军事阵型 (基于军事战术实践)
	// ============================================================================

	/** 楔形阵型 (V形，适用于突破) */
	Wedge               UMETA(DisplayName = "楔形阵"),

	/** 纵队阵型 (单列，适用于通过狭窄地形) */
	Column              UMETA(DisplayName = "纵队阵"),

	/** 横队阵型 (单排，适用于火力覆盖) */
	Line                UMETA(DisplayName = "横队阵"),

	/** V形阵型 (反转楔形，适用于防御) */
	Vee                 UMETA(DisplayName = "V形阵"),

	/** 梯形阵型 (阶梯形，适用于侧翼攻击) */
	Echelon             UMETA(DisplayName = "梯形阵"),

	/** 左梯形阵型 */
	EchelonLeft         UMETA(DisplayName = "左梯形阵"),

	/** 右梯形阵型 */
	EchelonRight        UMETA(DisplayName = "右梯形阵"),

	// ============================================================================
	// 几何阵型 (基于数学几何)
	// ============================================================================

	/** 蜂巢阵型 (六边形网格，最紧凑的2D填充) */
	HexagonalGrid       UMETA(DisplayName = "蜂巢阵"),

	/** 星形阵型 (五角星，适用于特殊视觉效果) */
	Star                UMETA(DisplayName = "星形阵"),

	/** 阿基米德螺旋阵型 (等距螺旋线) */
	ArchimedeanSpiral   UMETA(DisplayName = "阿基米德螺旋"),

	/** 对数螺旋阵型 (斐波那契螺旋，自然生长模式) */
	LogarithmicSpiral   UMETA(DisplayName = "对数螺旋"),

	/** 心脏形阵型 (心形曲线，适用于浪漫效果) */
	Heart               UMETA(DisplayName = "心脏阵"),

	/** 花瓣阵型 (花朵形状，参数化花瓣数量) */
	Flower              UMETA(DisplayName = "花瓣阵"),

	/** 黄金螺旋阵型 (最自然的螺旋分布) */
	GoldenSpiral        UMETA(DisplayName = "黄金螺旋"),

	/** 圆形网格阵型 (极坐标规则网格) */
	CircularGrid        UMETA(DisplayName = "圆形网格"),

	/** 玫瑰曲线阵型 (数学艺术曲线) */
	RoseCurve           UMETA(DisplayName = "玫瑰曲线"),

	/** 同心圆环阵型 (多层圆环分布) */
	ConcentricRings     UMETA(DisplayName = "同心圆环")
};

/**
 * 圆形/球体采样的分布模式
 */
UENUM(BlueprintType)
enum class ECircleDistributionMode : uint8
{
	/** 圆周均匀角度分布（2D）或经纬度网格（3D） */
	Uniform     UMETA(DisplayName = "均匀分布"),

	/** 黄金角螺旋分布（2D）或斐波那契球面（3D），分布最均匀 */
	Fibonacci   UMETA(DisplayName = "斐波那契"),

	/** 随机但保证最小距离的自然分布 */
	Poisson     UMETA(DisplayName = "泊松分布")
};

/**
 * 纹理采样通道选择
 */
UENUM(BlueprintType)
enum class ETextureSamplingChannel : uint8
{
	/** 自动判断：智能检测Alpha通道是否有效，无效则使用亮度 */
	Auto        UMETA(DisplayName = "自动"),

	/** 使用Alpha通道（透明度）*/
	Alpha       UMETA(DisplayName = "Alpha通道"),

	/** 使用感知亮度（Luminance = 0.299*R + 0.587*G + 0.114*B）*/
	Luminance   UMETA(DisplayName = "亮度"),

	/** 使用红色通道 */
	Red         UMETA(DisplayName = "红色通道"),

	/** 使用绿色通道 */
	Green       UMETA(DisplayName = "绿色通道"),

	/** 使用蓝色通道 */
	Blue        UMETA(DisplayName = "蓝色通道")
};

/**
 * 泊松采样配置结构体
 */
USTRUCT(BlueprintType)
struct POINTSAMPLING_API FPoissonSamplingConfig
{
	GENERATED_BODY()

	/** 点之间的最小距离（<=0时根据目标点数自动计算） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson",
		meta = (DisplayName = "最小距离", ToolTip = "点之间的最小距离，<=0时根据目标点数计算"))
	float MinDistance = 50.0f;

	/** 目标点数量（<=0时忽略，由MinDistance控制；>0时自动计算MinDistance并严格裁剪到目标数量） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson",
		meta = (DisplayName = "目标点数", ToolTip = "期望生成的点数量，0表示由MinDistance控制"))
	int32 TargetPointCount = 0;

	/** 最大尝试次数（在标记一个点为不活跃前的最大尝试次数） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Advanced",
		meta = (DisplayName = "最大尝试次数", ToolTip = "构造函数建议5-10，运行时建议15-30"))
	int32 MaxAttempts = 30;

	/** 坐标空间类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson",
		meta = (DisplayName = "坐标空间"))
	EPoissonCoordinateSpace CoordinateSpace = EPoissonCoordinateSpace::Local;

	/** 扰动强度 0-1（0=无扰动，1=最大扰动） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Advanced",
		meta = (ClampMin = "0", ClampMax = "1", DisplayName = "扰动强度"))
	float JitterStrength = 0.0f;

	/** 是否使用结果缓存 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poisson|Performance",
		meta = (DisplayName = "启用缓存", ToolTip = "构造函数建议开启，运行时可选"))
	bool bUseCache = true;

	/** 构造函数 - 提供合理的默认值 */
	FPoissonSamplingConfig()
		: MinDistance(50.0f)
		, TargetPointCount(0)
		, MaxAttempts(30)
		, CoordinateSpace(EPoissonCoordinateSpace::Local)
		, JitterStrength(0.0f)
		, bUseCache(true)
	{
	}
};

// ============================================================================
// 日志类别声明
// ============================================================================

/** PointSampling模块统一日志类别 */
POINTSAMPLING_API DECLARE_LOG_CATEGORY_EXTERN(LogPointSampling, Log, All);

