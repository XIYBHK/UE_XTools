#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplineFollowLibrary.generated.h"

class AActor;
class APawn;
class ACharacter;
class USplineComponent;

UENUM(BlueprintType)
enum class EXToolsSplineFollowStatus : uint8
{
	Invalid UMETA(DisplayName = "无效"),
	Moving UMETA(DisplayName = "移动中"),
	ReachedEnd UMETA(DisplayName = "到达终点")
};

UENUM(BlueprintType)
enum class EXToolsSplineFollowSpeedMode : uint8
{
	None UMETA(DisplayName = "不补偿"),
	OffsetPathLength UMETA(DisplayName = "自身路径倍率"),
	MatchReferenceOffset UMETA(DisplayName = "按参考偏移对齐")
};

UENUM(BlueprintType)
enum class EXToolsSplineFollowEndBehavior : uint8
{
	Stop UMETA(DisplayName = "停止"),
	PingPong UMETA(DisplayName = "往返"),
	Loop UMETA(DisplayName = "循环")
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsSplineFollowState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "已有距离缓存"))
	bool bHasDistanceCache = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "上次目标距离"))
	double LastTargetDistance = 0.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "上次是否反向"))
	bool bLastReverse = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "已有运行方向"))
	bool bHasRuntimeReverse = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "运行时反向"))
	bool bRuntimeReverse = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "开放循环过渡中"))
	bool bOpenLoopTransitionActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "开放循环目标为终点"))
	bool bOpenLoopTransitionToEnd = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "已捕获基础行走速度"))
	bool bHasCapturedBaseMaxWalkSpeed = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "基础最大行走速度"))
	float CapturedBaseMaxWalkSpeed = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "已捕获基础加速度"))
	bool bHasCapturedBaseMaxAcceleration = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|样条线|移动", meta = (DisplayName = "基础最大加速度"))
	float CapturedBaseMaxAcceleration = 0.0f;
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsSplineFollowResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "状态"))
	EXToolsSplineFollowStatus Status = EXToolsSplineFollowStatus::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "目标位置"))
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "移动方向"))
	FVector MoveDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "最近样条位置"))
	FVector CurrentSplineLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "前视位置"))
	FVector LookAheadLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "当前样条距离"))
	double CurrentDistance = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "目标样条距离"))
	double TargetDistance = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "样条总长度"))
	double SplineLength = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "速度倍率"))
	float SpeedScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "当前偏移路径长度"))
	double CurrentOffsetPathLength = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "参考路径长度"))
	double ReferencePathLength = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "原始速度倍率"))
	float RawSpeedScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|样条线|移动", meta = (DisplayName = "是否到达终点"))
	bool bReachedEnd = false;
};

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API USplineFollowLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 重置样条跟随状态。
	 * 保留已捕获的角色移动基准，停止速度补偿时请先恢复角色移动。
	 *
	 * @param State 每个跟随对象独立持有的跨帧状态。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "重置样条跟随状态", Keywords = "样条 跟随 移动 重置 状态 Spline Follow Reset",
			ToolTip = "重置样条距离和方向缓存。\n保留已捕获的角色移动速度和加速度基准。"))
	static void ResetSplineFollowState(UPARAM(ref, DisplayName = "跟随状态", meta = (Tooltip = "每个跟随对象独立持有。")) FXToolsSplineFollowState& State);

	/**
	 * 计算Actor相对样条中心线的初始右偏移。
	 * 建议在BeginPlay捕获一次，再接入后续跟随节点。
	 *
	 * @param TargetActor 要测量的Actor。
	 * @param SplineComponent 作为路径中心线的样条组件。
	 * @param OutRightOffset 输出右侧为正的横向偏移。
	 * @param OutDistance 输出Actor最近点的样条距离。
	 * @param OutSplineLocation 输出样条上最近点的世界位置。
	 * @param bUseSplineScaleForRightOffset 开启后按样条Y缩放换算偏移。
	 * @param bClampRightOffsetToSplineScaleRange 开启后按缩放范围半宽限制输出偏移。
	 * @param SplineScaleRangeHalfWidth 限制用的基础半宽，0表示不限制。
	 * @param bConstrainToXY 开启后忽略高度差。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "计算样条初始右偏移", Keywords = "样条 跟随 移动 初始 偏移 车道 队形 Spline Follow Initial Offset",
			ToolTip = "计算Actor相对样条中心线的初始右偏移。\n建议BeginPlay捕获一次，再接入跟随节点。",
			AdvancedDisplay = "bUseSplineScaleForRightOffset,bClampRightOffsetToSplineScaleRange,bConstrainToXY"))
	static bool CalculateInitialSplineRightOffset(
		UPARAM(DisplayName = "目标Actor", meta = (Tooltip = "要测量的Actor。")) AActor* TargetActor,
		UPARAM(DisplayName = "样条组件", meta = (Tooltip = "作为路径中心线的样条。")) USplineComponent* SplineComponent,
		UPARAM(DisplayName = "向右偏移", meta = (Tooltip = "输出右侧为正的横向距离。")) double& OutRightOffset,
		UPARAM(DisplayName = "样条距离", meta = (Tooltip = "输出最近点的样条距离。")) double& OutDistance,
		UPARAM(DisplayName = "最近样条位置", meta = (Tooltip = "输出样条最近点世界位置。")) FVector& OutSplineLocation,
		UPARAM(DisplayName = "偏移乘以样条缩放", meta = (Tooltip = "按样条Y缩放换算偏移。")) bool bUseSplineScaleForRightOffset = true,
		UPARAM(DisplayName = "限制到缩放范围", meta = (Tooltip = "开启后按缩放范围半宽限制偏移。")) bool bClampRightOffsetToSplineScaleRange = false,
		UPARAM(DisplayName = "缩放范围半宽") double SplineScaleRangeHalfWidth = 0.0,
		UPARAM(DisplayName = "限制到XY平面", meta = (Tooltip = "忽略高度差，适合地面角色。")) bool bConstrainToXY = true);

	/**
	 * 计算样条前视目标，不主动移动。
	 *
	 * @param TargetActor 要跟随的Actor。
	 * @param SplineComponent 作为移动路径的样条组件。
	 * @param State 每个Actor独立持有的跨帧状态。
	 * @param OutResult 输出目标点、移动方向、速度倍率和调试数值。
	 * @param RightOffset 目标点相对样条右方向的横向距离，右侧为正。
	 * @param LookAheadDistance 沿样条前方预测目标的距离。
	 * @param EndBehavior 到达端点后的行为：停止、往返或循环。
	 * @param SpeedMode 根据偏移通道路径长度调整速度，用于内外侧对齐。
	 * @param bReverse 沿样条距离递减方向移动。
	 * @param EndAcceptanceDistance 距离端点小于该值时视为到达。
	 * @param bConstrainToXY 移动方向忽略Z轴，适合地面角色。
	 * @param bUseSplineScaleForRightOffset 开启后右偏移会随样条Y缩放变化。
	 * @param bClampRightOffsetToSplineScaleRange 开启后按缩放范围半宽限制右偏移。
	 * @param SplineScaleRangeHalfWidth 限制用的基础半宽，实际边界会再乘样条Y缩放；0表示不限制。
	 * @param ReferenceRightOffset 参考通道的右偏移，仅“按参考偏移对齐”使用。
	 * @param SpeedSampleDistance 用于比较偏移路径长度的局部窗口距离。
	 * @param SpeedSampleSegments 路径长度采样分段数，越高越准但更耗时。
	 * @param MinSpeedScale 速度补偿允许的最低倍率。
	 * @param MaxSpeedScale 速度补偿允许的最高倍率。
	 * @param bDrawDebug 绘制最近点、前视点和目标点。
	 * @param DebugDrawTime 调试图形保留时间，0表示仅当前帧。
	 * @param DebugPointSize 调试点和箭头的显示尺寸。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "计算样条跟随目标", Keywords = "样条 跟随 移动 目标 AI MoveTo Spline Follow Target",
			ToolTip = "计算样条前视目标，不主动移动。\n可配合速度补偿保持不同偏移轨道的行进节奏。",
			AdvancedDisplay = "bReverse,EndAcceptanceDistance,bConstrainToXY,bUseSplineScaleForRightOffset,ReferenceRightOffset,SpeedSampleDistance,SpeedSampleSegments,MinSpeedScale,MaxSpeedScale,bDrawDebug,DebugDrawTime,DebugPointSize"))
	static EXToolsSplineFollowStatus CalculateSplineFollowTarget(
		UPARAM(DisplayName = "目标Actor", meta = (Tooltip = "要跟随样条的Actor。")) AActor* TargetActor,
		UPARAM(DisplayName = "样条组件", meta = (Tooltip = "作为移动路径的样条。")) USplineComponent* SplineComponent,
		UPARAM(ref, DisplayName = "跟随状态", meta = (Tooltip = "每个Actor独立保存一份。")) FXToolsSplineFollowState& State,
		UPARAM(DisplayName = "结果", meta = (Tooltip = "输出目标点、方向和速度倍率。")) FXToolsSplineFollowResult& OutResult,
		UPARAM(DisplayName = "向右偏移", meta = (Tooltip = "目标轨道横向距离，右侧为正。")) double RightOffset = 0.0,
		UPARAM(DisplayName = "前视距离", meta = (Tooltip = "沿移动方向提前取目标点的距离。")) double LookAheadDistance = 100.0,
		UPARAM(DisplayName = "终点行为", meta = (Tooltip = "到端点后停止、往返或循环。")) EXToolsSplineFollowEndBehavior EndBehavior = EXToolsSplineFollowEndBehavior::Stop,
		UPARAM(DisplayName = "速度补偿模式", meta = (Tooltip = "按偏移轨道长度修正速度。")) EXToolsSplineFollowSpeedMode SpeedMode = EXToolsSplineFollowSpeedMode::None,
		UPARAM(DisplayName = "反向", meta = (Tooltip = "沿样条距离递减方向移动。")) bool bReverse = false,
		UPARAM(DisplayName = "终点容差", meta = (Tooltip = "小于该距离视为到达端点。")) double EndAcceptanceDistance = 25.0,
		UPARAM(DisplayName = "限制到XY平面", meta = (Tooltip = "移动方向忽略Z轴。")) bool bConstrainToXY = true,
		UPARAM(DisplayName = "偏移乘以样条缩放", meta = (Tooltip = "右偏移随样条Y缩放变化。")) bool bUseSplineScaleForRightOffset = true,
		UPARAM(DisplayName = "限制到缩放范围", meta = (Tooltip = "开启后按缩放范围半宽限制右偏移。")) bool bClampRightOffsetToSplineScaleRange = false,
		UPARAM(DisplayName = "缩放范围半宽") double SplineScaleRangeHalfWidth = 0.0,
		UPARAM(DisplayName = "参考偏移距离", meta = (Tooltip = "参考通道右偏移，仅参考偏移速度模式使用。")) double ReferenceRightOffset = 0.0,
		UPARAM(DisplayName = "速度采样距离", meta = (Tooltip = "比较局部路径长度的窗口大小。")) double SpeedSampleDistance = 100.0,
		UPARAM(DisplayName = "速度采样段数", meta = (Tooltip = "路径长度采样精度，越高越耗时。")) int32 SpeedSampleSegments = 4,
		UPARAM(DisplayName = "最小速度倍率", meta = (Tooltip = "速度补偿最低倍率。")) float MinSpeedScale = 0.5f,
		UPARAM(DisplayName = "最大速度倍率", meta = (Tooltip = "速度补偿最高倍率。")) float MaxSpeedScale = 2.0f,
		UPARAM(DisplayName = "调试绘制", meta = (Tooltip = "绘制最近点、前视点和目标点。")) bool bDrawDebug = false,
		UPARAM(DisplayName = "调试持续时间", meta = (Tooltip = "0表示仅当前帧。")) float DebugDrawTime = 0.0f,
		UPARAM(DisplayName = "调试点大小", meta = (Tooltip = "调试点和箭头大小。")) float DebugPointSize = 18.0f);

	/**
	 * 计算样条前视目标，并对Pawn调用AddMovementInput。
	 *
	 * @param Pawn 要移动的Pawn。
	 * @param SplineComponent 作为移动路径的样条组件。
	 * @param State 每个Pawn独立持有的跨帧状态。
	 * @param OutResult 输出目标点、移动方向、速度倍率和调试数值。
	 * @param RightOffset 目标点相对样条右方向的横向距离，右侧为正。
	 * @param LookAheadDistance 沿样条前方预测目标的距离。
	 * @param EndBehavior 到达端点后的行为：停止、往返或循环。
	 * @param SpeedMode 根据偏移通道路径长度调整速度，用于内外侧对齐。
	 * @param MovementScale 传给AddMovementInput的输入强度，通常保持1。
	 * @param bReverse 沿样条距离递减方向移动。
	 * @param EndAcceptanceDistance 距离端点小于该值时视为到达。
	 * @param bConstrainToXY 移动方向忽略Z轴，适合地面角色。
	 * @param bUseSplineScaleForRightOffset 开启后右偏移会随样条Y缩放变化。
	 * @param bClampRightOffsetToSplineScaleRange 开启后按缩放范围半宽限制右偏移。
	 * @param SplineScaleRangeHalfWidth 限制用的基础半宽，实际边界会再乘样条Y缩放；0表示不限制。
	 * @param ReferenceRightOffset 参考通道的右偏移，仅“按参考偏移对齐”使用。
	 * @param SpeedSampleDistance 用于比较偏移路径长度的局部窗口距离。
	 * @param SpeedSampleSegments 路径长度采样分段数，越高越准但更耗时。
	 * @param MinSpeedScale 速度补偿允许的最低倍率。
	 * @param MaxSpeedScale 速度补偿允许的最高倍率。
	 * @param bForce 忽略Pawn移动输入是否被禁用，强制添加输入。
	 * @param bApplySpeedToCharacterMovement Pawn是Character时自动写入MaxWalkSpeed。
	 * @param bScaleCharacterAcceleration 速度倍率大于1时同步提高MaxAcceleration。
	 * @param BaseMaxWalkSpeed 速度补偿基准。0表示首次调用时自动捕获。
	 * @param SpeedUpdateTolerance 速度差小于该值时不写入移动组件。
	 * @param MinWalkSpeed 写入CharacterMovement的最低MaxWalkSpeed。
	 * @param MaxWalkSpeed 写入CharacterMovement的最高MaxWalkSpeed，0表示不限制。
	 * @param bDrawDebug 绘制最近点、前视点和目标点。
	 * @param DebugDrawTime 调试图形保留时间，0表示仅当前帧。
	 * @param DebugPointSize 调试点和箭头的显示尺寸。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "沿样条线添加移动输入", Keywords = "样条 跟随 移动 Pawn AddMovementInput Spline Follow",
			ToolTip = "计算样条前视目标，并对Pawn调用AddMovementInput。\n可选自动写入CharacterMovement速度以配合轨道速度补偿。",
			AdvancedDisplay = "bReverse,EndAcceptanceDistance,bConstrainToXY,bUseSplineScaleForRightOffset,ReferenceRightOffset,SpeedSampleDistance,SpeedSampleSegments,MinSpeedScale,MaxSpeedScale,bForce,bApplySpeedToCharacterMovement,bScaleCharacterAcceleration,BaseMaxWalkSpeed,SpeedUpdateTolerance,MinWalkSpeed,MaxWalkSpeed,bDrawDebug,DebugDrawTime,DebugPointSize"))
	static EXToolsSplineFollowStatus AddMovementInputAlongSpline(
		UPARAM(DisplayName = "Pawn", meta = (Tooltip = "要移动的Pawn。")) APawn* Pawn,
		UPARAM(DisplayName = "样条组件", meta = (Tooltip = "作为移动路径的样条。")) USplineComponent* SplineComponent,
		UPARAM(ref, DisplayName = "跟随状态", meta = (Tooltip = "每个Pawn独立保存一份。")) FXToolsSplineFollowState& State,
		UPARAM(DisplayName = "结果", meta = (Tooltip = "输出目标点、方向和速度倍率。")) FXToolsSplineFollowResult& OutResult,
		UPARAM(DisplayName = "向右偏移", meta = (Tooltip = "目标轨道横向距离，右侧为正。")) double RightOffset = 0.0,
		UPARAM(DisplayName = "前视距离", meta = (Tooltip = "沿移动方向提前取目标点的距离。")) double LookAheadDistance = 100.0,
		UPARAM(DisplayName = "终点行为", meta = (Tooltip = "到端点后停止、往返或循环。")) EXToolsSplineFollowEndBehavior EndBehavior = EXToolsSplineFollowEndBehavior::Stop,
		UPARAM(DisplayName = "速度补偿模式", meta = (Tooltip = "按偏移轨道长度修正速度。")) EXToolsSplineFollowSpeedMode SpeedMode = EXToolsSplineFollowSpeedMode::None,
		UPARAM(DisplayName = "移动权重", meta = (Tooltip = "传给AddMovementInput的输入强度。")) float MovementScale = 1.0f,
		UPARAM(DisplayName = "反向", meta = (Tooltip = "沿样条距离递减方向移动。")) bool bReverse = false,
		UPARAM(DisplayName = "终点容差", meta = (Tooltip = "小于该距离视为到达端点。")) double EndAcceptanceDistance = 25.0,
		UPARAM(DisplayName = "限制到XY平面", meta = (Tooltip = "移动方向忽略Z轴。")) bool bConstrainToXY = true,
		UPARAM(DisplayName = "偏移乘以样条缩放", meta = (Tooltip = "右偏移随样条Y缩放变化。")) bool bUseSplineScaleForRightOffset = true,
		UPARAM(DisplayName = "限制到缩放范围", meta = (Tooltip = "开启后按缩放范围半宽限制右偏移。")) bool bClampRightOffsetToSplineScaleRange = false,
		UPARAM(DisplayName = "缩放范围半宽") double SplineScaleRangeHalfWidth = 0.0,
		UPARAM(DisplayName = "参考偏移距离", meta = (Tooltip = "参考通道右偏移，仅参考偏移速度模式使用。")) double ReferenceRightOffset = 0.0,
		UPARAM(DisplayName = "速度采样距离", meta = (Tooltip = "比较局部路径长度的窗口大小。")) double SpeedSampleDistance = 100.0,
		UPARAM(DisplayName = "速度采样段数", meta = (Tooltip = "路径长度采样精度，越高越耗时。")) int32 SpeedSampleSegments = 4,
		UPARAM(DisplayName = "最小速度倍率", meta = (Tooltip = "速度补偿最低倍率。")) float MinSpeedScale = 0.5f,
		UPARAM(DisplayName = "最大速度倍率", meta = (Tooltip = "速度补偿最高倍率。")) float MaxSpeedScale = 2.0f,
		UPARAM(DisplayName = "强制输入", meta = (Tooltip = "忽略移动输入禁用状态。")) bool bForce = false,
		UPARAM(DisplayName = "自动应用角色速度", meta = (Tooltip = "Character会自动写MaxWalkSpeed。")) bool bApplySpeedToCharacterMovement = true,
		UPARAM(DisplayName = "同步角色加速度", meta = (Tooltip = "加速时同步提高MaxAcceleration。")) bool bScaleCharacterAcceleration = true,
		UPARAM(DisplayName = "基础最大行走速度", meta = (Tooltip = "速度倍率为1时的行走速度；0自动捕获。")) float BaseMaxWalkSpeed = 0.0f,
		UPARAM(DisplayName = "速度更新容差", meta = (Tooltip = "速度差小于该值时不写入。")) float SpeedUpdateTolerance = 1.0f,
		UPARAM(DisplayName = "最小行走速度", meta = (Tooltip = "写入角色移动的最低速度。")) float MinWalkSpeed = 0.0f,
		UPARAM(DisplayName = "最大行走速度", meta = (Tooltip = "写入角色移动的最高速度；0不限制。")) float MaxWalkSpeed = 0.0f,
		UPARAM(DisplayName = "调试绘制", meta = (Tooltip = "绘制最近点、前视点和目标点。")) bool bDrawDebug = false,
		UPARAM(DisplayName = "调试持续时间", meta = (Tooltip = "0表示仅当前帧。")) float DebugDrawTime = 0.0f,
		UPARAM(DisplayName = "调试点大小", meta = (Tooltip = "调试点和箭头大小。")) float DebugPointSize = 18.0f);

	/**
	 * 计算AI MoveTo目的地，不主动移动。
	 *
	 * @param TargetActor AI控制的Actor。
	 * @param SplineComponent 作为移动路径的样条组件。
	 * @param State 每个Actor独立持有的跨帧状态。
	 * @param OutDestination 输出可接入AI MoveTo的Destination。
	 * @param OutResult 输出目标点、移动方向、速度倍率和调试数值。
	 * @param RightOffset 目标点相对样条右方向的横向距离，右侧为正。
	 * @param LookAheadDistance 沿样条前方预测目标的距离。
	 * @param EndBehavior 到达端点后的行为：停止、往返或循环。
	 * @param SpeedMode 根据偏移通道路径长度调整速度，用于内外侧对齐。
	 * @param bReverse 沿样条距离递减方向移动。
	 * @param EndAcceptanceDistance 距离端点小于该值时视为到达。
	 * @param bConstrainToXY 移动方向忽略Z轴，适合地面角色。
	 * @param bUseSplineScaleForRightOffset 开启后右偏移会随样条Y缩放变化。
	 * @param bClampRightOffsetToSplineScaleRange 开启后按缩放范围半宽限制右偏移。
	 * @param SplineScaleRangeHalfWidth 限制用的基础半宽，实际边界会再乘样条Y缩放；0表示不限制。
	 * @param ReferenceRightOffset 参考通道的右偏移，仅“按参考偏移对齐”使用。
	 * @param SpeedSampleDistance 用于比较偏移路径长度的局部窗口距离。
	 * @param SpeedSampleSegments 路径长度采样分段数，越高越准但更耗时。
	 * @param MinSpeedScale 速度补偿允许的最低倍率。
	 * @param MaxSpeedScale 速度补偿允许的最高倍率。
	 * @param bDrawDebug 绘制最近点、前视点和目标点。
	 * @param DebugDrawTime 调试图形保留时间，0表示仅当前帧。
	 * @param DebugPointSize 调试点和箭头的显示尺寸。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "获取样条AI移动目标", Keywords = "样条 跟随 AI MoveTo 目标 Spline Follow Destination",
			ToolTip = "计算可接入AI MoveTo的样条目的地。\n只计算目标位置，不主动移动Actor。",
			AdvancedDisplay = "bReverse,EndAcceptanceDistance,bConstrainToXY,bUseSplineScaleForRightOffset,ReferenceRightOffset,SpeedSampleDistance,SpeedSampleSegments,MinSpeedScale,MaxSpeedScale,bDrawDebug,DebugDrawTime,DebugPointSize"))
	static EXToolsSplineFollowStatus GetSplineAIMoveToDestination(
		UPARAM(DisplayName = "目标Actor", meta = (Tooltip = "AI控制或移动的Actor。")) AActor* TargetActor,
		UPARAM(DisplayName = "样条组件", meta = (Tooltip = "作为移动路径的样条。")) USplineComponent* SplineComponent,
		UPARAM(ref, DisplayName = "跟随状态", meta = (Tooltip = "每个Actor独立保存一份。")) FXToolsSplineFollowState& State,
		UPARAM(DisplayName = "目标位置", meta = (Tooltip = "输出给AI MoveTo的目的地。")) FVector& OutDestination,
		UPARAM(DisplayName = "结果", meta = (Tooltip = "输出目标点、方向和速度倍率。")) FXToolsSplineFollowResult& OutResult,
		UPARAM(DisplayName = "向右偏移", meta = (Tooltip = "目标轨道横向距离，右侧为正。")) double RightOffset = 0.0,
		UPARAM(DisplayName = "前视距离", meta = (Tooltip = "沿移动方向提前取目标点的距离。")) double LookAheadDistance = 100.0,
		UPARAM(DisplayName = "终点行为", meta = (Tooltip = "到端点后停止、往返或循环。")) EXToolsSplineFollowEndBehavior EndBehavior = EXToolsSplineFollowEndBehavior::Stop,
		UPARAM(DisplayName = "速度补偿模式", meta = (Tooltip = "按偏移轨道长度修正速度。")) EXToolsSplineFollowSpeedMode SpeedMode = EXToolsSplineFollowSpeedMode::None,
		UPARAM(DisplayName = "反向", meta = (Tooltip = "沿样条距离递减方向移动。")) bool bReverse = false,
		UPARAM(DisplayName = "终点容差", meta = (Tooltip = "小于该距离视为到达端点。")) double EndAcceptanceDistance = 25.0,
		UPARAM(DisplayName = "限制到XY平面", meta = (Tooltip = "移动方向忽略Z轴。")) bool bConstrainToXY = true,
		UPARAM(DisplayName = "偏移乘以样条缩放", meta = (Tooltip = "右偏移随样条Y缩放变化。")) bool bUseSplineScaleForRightOffset = true,
		UPARAM(DisplayName = "限制到缩放范围", meta = (Tooltip = "开启后按缩放范围半宽限制右偏移。")) bool bClampRightOffsetToSplineScaleRange = false,
		UPARAM(DisplayName = "缩放范围半宽") double SplineScaleRangeHalfWidth = 0.0,
		UPARAM(DisplayName = "参考偏移距离", meta = (Tooltip = "参考通道右偏移，仅参考偏移速度模式使用。")) double ReferenceRightOffset = 0.0,
		UPARAM(DisplayName = "速度采样距离", meta = (Tooltip = "比较局部路径长度的窗口大小。")) double SpeedSampleDistance = 100.0,
		UPARAM(DisplayName = "速度采样段数", meta = (Tooltip = "路径长度采样精度，越高越耗时。")) int32 SpeedSampleSegments = 4,
		UPARAM(DisplayName = "最小速度倍率", meta = (Tooltip = "速度补偿最低倍率。")) float MinSpeedScale = 0.5f,
		UPARAM(DisplayName = "最大速度倍率", meta = (Tooltip = "速度补偿最高倍率。")) float MaxSpeedScale = 2.0f,
		UPARAM(DisplayName = "调试绘制", meta = (Tooltip = "绘制最近点、前视点和目标点。")) bool bDrawDebug = false,
		UPARAM(DisplayName = "调试持续时间", meta = (Tooltip = "0表示仅当前帧。")) float DebugDrawTime = 0.0f,
		UPARAM(DisplayName = "调试点大小", meta = (Tooltip = "调试点和箭头大小。")) float DebugPointSize = 18.0f);

	/**
	 * 将样条跟随速度倍率应用到CharacterMovement。
	 *
	 * @param Character 要写入CharacterMovement速度的角色。
	 * @param BaseMaxWalkSpeed 速度倍率为1时的MaxWalkSpeed。
	 * @param SpeedScale 最终速度倍率，通常来自样条跟随结果。
	 * @param UpdateTolerance 速度差小于该值时不写入移动组件。
	 * @param MinWalkSpeed 写入CharacterMovement的最低MaxWalkSpeed。
	 * @param MaxWalkSpeed 写入CharacterMovement的最高MaxWalkSpeed，0表示不限制。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "应用样条跟随速度到角色移动", Keywords = "样条 跟随 队形 速度 CharacterMovement MaxWalkSpeed Spline Follow Speed",
			ToolTip = "将样条跟随速度倍率写入CharacterMovement。\n用于内外圈速度补偿后的角色速度同步。",
			AdvancedDisplay = "UpdateTolerance,MinWalkSpeed,MaxWalkSpeed"))
	static bool ApplySplineFollowSpeedToCharacter(
		UPARAM(DisplayName = "角色", meta = (Tooltip = "要写入CharacterMovement的角色。")) ACharacter* Character,
		UPARAM(DisplayName = "基础最大行走速度", meta = (Tooltip = "速度倍率为1时的MaxWalkSpeed。")) float BaseMaxWalkSpeed,
		UPARAM(DisplayName = "速度倍率", meta = (Tooltip = "最终速度倍率。")) float SpeedScale,
		UPARAM(DisplayName = "更新容差", meta = (Tooltip = "速度差小于该值时不写入。")) float UpdateTolerance = 1.0f,
		UPARAM(DisplayName = "最小行走速度", meta = (Tooltip = "最低MaxWalkSpeed。")) float MinWalkSpeed = 0.0f,
		UPARAM(DisplayName = "最大行走速度", meta = (Tooltip = "最高MaxWalkSpeed；0不限制。")) float MaxWalkSpeed = 0.0f);

	/**
	 * 恢复自动样条跟随改过的CharacterMovement数值。
	 *
	 * @param Character 要恢复CharacterMovement数值的角色。
	 * @param State 包含已捕获基础速度和加速度的状态变量。
	 * @param bRestoreWalkSpeed 恢复自动捕获的MaxWalkSpeed。
	 * @param bRestoreAcceleration 恢复自动捕获的MaxAcceleration。
	 * @param bClearCapturedValues 恢复后清空状态里的速度和加速度基准。
	 */
	UFUNCTION(BlueprintCallable, Category = "XTools|样条线|移动",
		meta = (DisplayName = "恢复样条跟随角色移动", Keywords = "样条 跟随 队形 速度 恢复 CharacterMovement MaxWalkSpeed MaxAcceleration Spline Follow Restore",
			ToolTip = "恢复样条跟随自动修改过的角色移动数值。\n停止速度补偿或销毁前调用。",
			AdvancedDisplay = "bRestoreWalkSpeed,bRestoreAcceleration,bClearCapturedValues"))
	static bool RestoreSplineFollowCharacterMovement(
		UPARAM(DisplayName = "角色", meta = (Tooltip = "要恢复CharacterMovement的角色。")) ACharacter* Character,
		UPARAM(ref, DisplayName = "跟随状态", meta = (Tooltip = "包含捕获到的基础速度。")) FXToolsSplineFollowState& State,
		UPARAM(DisplayName = "恢复行走速度", meta = (Tooltip = "恢复MaxWalkSpeed。")) bool bRestoreWalkSpeed = true,
		UPARAM(DisplayName = "恢复加速度", meta = (Tooltip = "恢复MaxAcceleration。")) bool bRestoreAcceleration = true,
		UPARAM(DisplayName = "清空捕获状态", meta = (Tooltip = "恢复后清空缓存。")) bool bClearCapturedValues = true);
};
