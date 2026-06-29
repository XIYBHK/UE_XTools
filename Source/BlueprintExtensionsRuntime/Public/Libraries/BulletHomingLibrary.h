#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BulletHomingLibrary.generated.h"

class AActor;
class UProjectileMovementComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EXToolsBulletGuidanceMode : uint8
{
	PurePursuit UMETA(DisplayName = "纯追踪"),
	PredictiveIntercept UMETA(DisplayName = "预测拦截"),
	ProportionalNavigation UMETA(DisplayName = "比例导引")
};

UENUM(BlueprintType)
enum class EXToolsBulletHomingStatus : uint8
{
	Invalid UMETA(DisplayName = "无效"),
	Tracking UMETA(DisplayName = "追踪中"),
	TargetInvalid UMETA(DisplayName = "目标无效"),
	Captured UMETA(DisplayName = "已捕获"),
	PassedTarget UMETA(DisplayName = "已越过目标")
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsBulletHomingState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "已初始化"))
	bool bInitialized = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "当前速度"))
	float CurrentSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "当前方向"))
	FVector CurrentDirection = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "最后目标位置"))
	FVector LastTargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "有最后目标位置"))
	bool bHasLastTargetLocation = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "已飞行时间"))
	float ElapsedTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "上次视线方向"))
	FVector LastLineOfSightDirection = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "有上次视线方向"))
	bool bHasLastLineOfSightDirection = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "最近目标距离"))
	float ClosestDistanceToTarget = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "有最近距离"))
	bool bHasClosestDistance = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "上次弹体位置"))
	FVector LastProjectileLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "有上次弹体位置"))
	bool bHasLastProjectileLocation = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "调试轨迹点", AdvancedDisplay))
	TArray<FVector> DebugTrailPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "上次目标Actor", AdvancedDisplay))
	TWeakObjectPtr<AActor> LastTargetActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "上次目标组件", AdvancedDisplay))
	TWeakObjectPtr<USceneComponent> LastTargetComponent;
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsBulletHomingOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "制导模式",
			ToolTip = "纯追踪朝目标当前位置转向；预测拦截会按目标速度提前瞄准；比例导引用相对速度和视线角速度修正速度方向，更适合高速导弹。"))
	EXToolsBulletGuidanceMode GuidanceMode = EXToolsBulletGuidanceMode::PurePursuit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "初始速度", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "首次更新状态时使用的显式初始速度，单位为cm/s。大于0时优先生效；小于等于0时使用ProjectileMovement当前速度、组件InitialSpeed或目标速度。"))
	float InitialSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "目标速度", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "追踪希望达到的速度，单位为cm/s。小于等于0时保持当前速度。"))
	float TargetSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "最大速度", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "速度上限，单位为cm/s。小于等于0时不额外限制；写入ProjectileMovement时会同步到组件MaxSpeed。"))
	float MaxSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "速度插值速度", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "当前速度插值到目标速度的速度。小于等于0时立即使用目标速度。"))
	float SpeedInterpRate = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "方向插值速度", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "当前飞行方向插值到目标方向的速度。小于等于0时立即朝向目标。"))
	float DirectionInterpRate = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "旋转插值速度", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "Actor/外观组件旋转插值到速度方向的速度。小于等于0时立即对齐。"))
	float RotationInterpRate = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "发射直飞时间", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "前多少秒保持发射方向为主，单位为秒。用于模拟导弹先离开发射器再开始强追踪。"))
	float LaunchStraightTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "发射段制导倍率", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0",
			ToolTip = "发射直飞时间内的制导强度。0表示完全直飞，1表示正常制导。"))
	float LaunchGuidanceScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "预测时间倍率", ClampMin = "0.0", UIMin = "0.0",
			EditCondition = "GuidanceMode == EXToolsBulletGuidanceMode::PredictiveIntercept", EditConditionHides,
			ToolTip = "预测拦截模式下，按距离/当前速度估算飞行时间后乘以该倍率。"))
	float PredictionTimeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "最大预测时间", ClampMin = "0.0", UIMin = "0.0",
			EditCondition = "GuidanceMode == EXToolsBulletGuidanceMode::PredictiveIntercept", EditConditionHides,
			ToolTip = "预测拦截模式下允许提前预测的最大秒数。小于等于0时不预测。"))
	float MaxPredictionTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "PN导航增益", ClampMin = "0.0", UIMin = "0.0",
			EditCondition = "GuidanceMode == EXToolsBulletGuidanceMode::ProportionalNavigation", EditConditionHides,
			ToolTip = "比例导引模式的导航增益，常用范围约3到5。值越大转向越积极，但过高可能抖动或过冲。"))
	float NavigationGain = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "最大制导加速度", ClampMin = "0.0", UIMin = "0.0",
			EditCondition = "GuidanceMode == EXToolsBulletGuidanceMode::ProportionalNavigation", EditConditionHides,
			ToolTip = "比例导引模式每秒可施加的最大横向速度修正，单位为cm/s²。小于等于0时不额外限制。"))
	float MaxGuidanceAcceleration = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "末端收束距离", ClampMin = "0.0", UIMin = "0.0", AdvancedDisplay,
			EditCondition = "GuidanceMode == EXToolsBulletGuidanceMode::ProportionalNavigation", EditConditionHides,
			ToolTip = "比例导引模式下，距离目标小于该值时逐步压低切向速度并增强朝向目标的径向速度，减少高速近目标绕圈。小于等于0时不启用。"))
	float TerminalConvergenceDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "末端径向拉力", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", AdvancedDisplay,
			EditCondition = "GuidanceMode == EXToolsBulletGuidanceMode::ProportionalNavigation && TerminalConvergenceDistance > 0", EditConditionHides,
			ToolTip = "末端收束时朝目标方向的最低径向速度比例。1表示接近目标时尽量把当前速度转为朝目标飞行；0表示不额外增强径向速度。"))
	float TerminalRadialPullStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "末端切向阻尼", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0", AdvancedDisplay,
			EditCondition = "GuidanceMode == EXToolsBulletGuidanceMode::ProportionalNavigation && TerminalConvergenceDistance > 0", EditConditionHides,
			ToolTip = "末端收束时削减绕目标切线方向速度的强度。0不削减；1在最接近目标时最大程度压低绕圈趋势。"))
	float TerminalTangentialDamping = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "捕获半径", ClampMin = "0.0", UIMin = "0.0",
			ToolTip = "距离目标小于等于该半径时输出已捕获，单位为cm。小于等于0时不启用。"))
	float CaptureRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "越过目标视为捕获",
			ToolTip = "为真时，弹体从目标附近掠过并开始远离时输出已捕获。适合导弹/飞剑等末端不希望绕圈的追踪弹。"))
	bool bCaptureWhenPassedTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "捕获后停止运动",
			ToolTip = "为真时，输出已捕获或已越过目标后立即输出零速度，并停止写入的ProjectileMovement速度，避免蓝图未立即销毁子弹时继续绕圈。"))
	bool bStopMovementOnTerminalStatus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "检测越过目标",
			ToolTip = "为真时，弹体曾进入越过判定距离后又远离目标，会输出已越过目标。"))
	bool bDetectPassedTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "越过判定距离", ClampMin = "0.0", UIMin = "0.0",
			EditCondition = "bDetectPassedTarget || bCaptureWhenPassedTarget", EditConditionHides,
			ToolTip = "启用越过目标检测或越过视为捕获时，最近距离曾小于等于该值且当前距离开始变大时触发终端状态，单位为cm。"))
	float PassedTargetDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "目标无效后继续飞行",
			ToolTip = "目标无效时继续使用最后目标位置或最后飞行方向，不立刻停止追踪更新。"))
	bool bContinueAfterTargetInvalid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "写入ProjectileMovement",
			ToolTip = "为真时将计算出的世界速度写入ProjectileMovementComponent::Velocity，并同步节点最大速度到组件MaxSpeed。若ProjectileMovement已启用原生Homing，通常应关闭其中一方。"))
	bool bApplyVelocityToProjectileMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "更新Actor旋转",
			ToolTip = "为真时将Actor旋转平滑对齐到新的世界速度方向。若ProjectileMovement已启用RotationFollowsVelocity，通常应关闭其中一方。"))
	bool bUpdateActorRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "更新外观组件旋转",
			ToolTip = "为真且传入外观组件时，将外观组件世界旋转对齐到速度方向并叠加外观旋转偏移。外观组件通常是子弹的模型、特效或其父级场景组件。"))
	bool bUpdateVisualComponentRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "外观旋转偏移",
			ToolTip = "外观组件在速度方向基础上的旋转偏移，用于模型本地轴不是X前向的情况。"))
	FRotator VisualRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "绘制调试",
			ToolTip = "为真时在世界中绘制目标方向、瞄准点、速度方向和捕获半径。仅用于调试。"))
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "调试持续时间", ClampMin = "0.0", UIMin = "0.0",
			EditCondition = "bDrawDebug", EditConditionHides,
			ToolTip = "调试线条保留时间，单位为秒。0表示只显示当前帧；调大后轨迹会在停止追踪后继续保留一段时间。"))
	float DebugDrawTime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "速度线长度", ClampMin = "0.0", UIMin = "0.0",
			EditCondition = "bDrawDebug", EditConditionHides,
			ToolTip = "调试绘制中速度方向线的长度，单位为cm。"))
	float DebugVelocityLineLength = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "绘制轨迹线",
			EditCondition = "bDrawDebug", EditConditionHides,
			ToolTip = "为真时绘制弹体历史轨迹线。轨迹点保存在状态中，重置状态会清空轨迹。"))
	bool bDrawDebugTrail = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "轨迹最大点数", ClampMin = "2", UIMin = "2", UIMax = "512",
			EditCondition = "bDrawDebug && bDrawDebugTrail", EditConditionHides,
			ToolTip = "调试轨迹最多保留的点数。数值越大轨迹越长，但绘制成本更高。"))
	int32 MaxDebugTrailPoints = 256;
};

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UBulletHomingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "更新追踪弹运动",
			Keywords = "Bullet Projectile Homing Tracking Missile Velocity Rotation 子弹 追踪 导弹 速度 旋转",
			ToolTip = "执行一次自定义追踪弹运动更新：支持纯追踪、预测拦截和比例导引，可选写入ProjectileMovement并更新Actor/外观组件旋转。适合放在Timer或Tick中替换复杂蓝图追踪数学；不要与ProjectileMovement原生Homing同时控制同一个速度。",
			AdvancedDisplay = "TargetComponent,VisualComponent"))
	static bool UpdateHomingProjectileMovement(
		UPARAM(DisplayName = "子弹Actor") AActor* ProjectileActor,
		UPARAM(DisplayName = "ProjectileMovement") UProjectileMovementComponent* ProjectileMovement,
		UPARAM(DisplayName = "目标Actor") AActor* TargetActor,
		UPARAM(DisplayName = "目标组件") USceneComponent* TargetComponent,
		UPARAM(DisplayName = "外观组件") USceneComponent* VisualComponent,
		UPARAM(DisplayName = "DeltaTime") float DeltaTime,
		UPARAM(DisplayName = "选项") const FXToolsBulletHomingOptions& Options,
		UPARAM(ref, DisplayName = "状态") FXToolsBulletHomingState& State,
		UPARAM(DisplayName = "结果状态") EXToolsBulletHomingStatus& OutStatus,
		UPARAM(DisplayName = "新世界速度") FVector& OutWorldVelocity,
		UPARAM(DisplayName = "新Actor旋转") FRotator& OutActorRotation,
		UPARAM(DisplayName = "新外观旋转") FRotator& OutVisualRotation);

	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|子弹",
		meta = (DisplayName = "重置追踪弹状态",
			Keywords = "Bullet Projectile Homing Reset State 子弹 追踪 重置 状态",
			ToolTip = "重置更新追踪弹运动节点使用的运行时状态。通常在生成子弹、切换目标或重新开始追踪前调用。"))
	static void ResetHomingProjectileState(
		UPARAM(ref, DisplayName = "状态") FXToolsBulletHomingState& State);
};
