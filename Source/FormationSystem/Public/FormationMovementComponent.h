#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "FormationMovementComponent.generated.h"

/**
 * 阵型移动组件
 * 专门用于处理Character的阵型移动，支持AddMovementInput方式
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Formation), meta=(BlueprintSpawnableComponent, DisplayName="阵型移动组件"))
class FORMATIONSYSTEM_API UFormationMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFormationMovementComponent();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    /**
     * 开始移动到目标位置
     * @param TargetLocation 目标位置
     * @param AcceptanceRadius 接受半径
     * @param MoveSpeed 移动速度倍数（0-1）
     */
    UFUNCTION(BlueprintCallable, Category = "Formation Movement", meta = (DisplayName = "开始移动到位置",
        ToolTip = "开始移动到目标位置。距离判定只比较XY平面（忽略Z轴高度差）；起点已在接受半径内时会立即广播完成事件。\n默认没有卡墙超时——被阻挡时不会自行结束；如需自动终止可开启组件的\"卡住时停止\"：连续\"卡住判定时间\"秒无有效距离进展将自动停止（不广播完成事件），也可随时调用\"停止移动\"手动终止。"))
    void StartMoveToLocation(FVector TargetLocation, float AcceptanceRadius = 50.0f, float MoveSpeed = 1.0f);

    /**
     * 停止移动
     */
    UFUNCTION(BlueprintCallable, Category = "Formation Movement", meta = (DisplayName = "停止移动",
        ToolTip = "立即停止移动并清除状态：重置速度、清除待处理的移动输入、禁用组件Tick。\n不广播完成事件。\"卡住时停止\"触发的自动停止与本函数语义完全一致。"))
    void StopMovement();

    /**
     * 是否正在移动
     */
    UFUNCTION(BlueprintPure, Category = "Formation Movement", meta = (DisplayName = "是否正在移动",
        ToolTip = "是否处于移动中（从开始移动到进入接受半径或停止）。\n默认情况下被阻挡卡住时仍返回true；开启\"卡住时停止\"后，连续无进展达到\"卡住判定时间\"会自动停止并返回false。"))
    bool IsMoving() const { return bIsMoving; }

    /**
     * 获取到目标的距离
     */
    UFUNCTION(BlueprintPure, Category = "Formation Movement", meta = (DisplayName = "获取到目标距离"))
    float GetDistanceToTarget() const;

    /**
     * 移动完成事件
     */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovementCompleted, UFormationMovementComponent*, MovementComponent);
    UPROPERTY(BlueprintAssignable, Category = "Formation Movement", meta = (
        ToolTip = "移动完成事件：仅表示XY平面距离进入接受半径。\n不代表路径可行或过程无阻挡——手动停止与\"卡住时停止\"的自动停止均不广播本事件（组件没有失败事件，停止语义请结合\"是否正在移动\"判断）。"))
    FOnMovementCompleted OnMovementCompleted;

protected:
    /** 目标位置 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Formation Movement")
    FVector TargetLocation;

    /** 接受半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (ClampMin = "1.0", ClampMax = "500.0", ForceUnits = "cm"))
    float AcceptanceRadius = 50.0f;

    /** 移动速度倍数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float MoveSpeed = 1.0f;

    /** 减速区域倍数（相对于接受半径的倍数） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (ClampMin = "1.0", ClampMax = "10.0", DisplayName = "减速区域倍数"))
    float SlowDownDistanceMultiplier = 3.0f;

    /** 减速时的最小速度倍数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (ClampMin = "0.1", ClampMax = "1.0", DisplayName = "最小减速倍数"))
    float MinSlowDownSpeed = 0.2f;

    /** 旋转速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (ClampMin = "1.0", ClampMax = "20.0", DisplayName = "旋转速度"))
    float RotationSpeed = 8.0f;

    /** 是否启用减速 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (DisplayName = "启用减速"))
    bool bEnableSlowDown = true;

    /**
     * 卡住时自动停止（默认关闭，保持既有行为）
     *
     * 开启后：连续 StuckTimeSeconds 秒没有有效距离进展（历史最优XY距离改善超过1cm）时，
     * 按"停止移动"语义自动终止——清理速度/输入、禁用Tick、IsMoving变为false，
     * 但不广播完成事件（完成事件仅表示进入接受半径）。
     * 覆盖场景：被墙阻挡、MovementMode无效、MaxWalkSpeed=0等"永久Tick且IsMoving永远为true"。
     * 已知限制：本组件是直线移动器（无寻路），需要大幅绕行才能接近的目标可能被误判为卡住。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (
        DisplayName = "卡住时停止",
        ToolTip = "默认关闭，关闭时保持既有行为（被阻挡需手动停止）。\n开启后连续\"卡住判定时间\"秒无有效距离进展（XY距离改善超过1cm）时自动停止：清理速度/输入、禁用Tick、不广播完成事件。\n注意：本组件是直线移动器，需要绕行的目标可能被误判为卡住。"))
    bool bStopWhenStuck = false;

    /**
     * 卡住判定时间（秒）
     *
     * 默认值 2.0s 的依据：正常制动全过程（高速→自然减速→速度归零后恢复输入）在数帧内完成，
     * 2s 对正常暂停留有数量级余量，同时卡住响应足够快。
     * Clamp 下限 0.5s 的依据：最慢合法进展 ≈ MoveSpeed下限0.1 × 最小减速倍数下限0.1
     * × 引擎默认 MaxWalkSpeed 600 ≈ 6cm/s，推进 1cm 需约 0.17s，0.5s 为极端慢速留约3倍余量，
     * 低于此值可能把"极慢但正常"误判为卡住；上限 30s 避免误配置为近乎永久等待。
     * （Clamp 同样约束蓝图运行时写入：判定读取时按 0.5-30 钳制。）
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Movement", meta = (
        ClampMin = "0.5", ClampMax = "30.0", ForceUnits = "s",
        DisplayName = "卡住判定时间",
        ToolTip = "连续无有效距离进展达到该时长即判定卡住并自动停止（仅\"卡住时停止\"开启时生效）。\n默认2秒；自定义过慢的移动速度（远低于6cm/s）应相应调大，过短会把极慢但正常的移动误判为卡住。",
        EditCondition = "bStopWhenStuck"))
    float StuckTimeSeconds = 2.0f;

    /** 是否正在移动 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Formation Movement")
    bool bIsMoving = false;

    /** 缓存的Character引用 */
    UPROPERTY()
    ACharacter* OwnerCharacter;

private:
    /** 更新移动逻辑 */
    void UpdateMovement(float DeltaTime);

    /** 检查是否到达目标 */
    bool HasReachedTarget() const;

    /**
     * 卡住检测（仅 bStopWhenStuck 开启时调用）：有有效进展（距离较历史最优改善超过
     * StuckProgressEpsilon）时刷新最优距离并清零计时；否则累计无进展时长，
     * 达到 StuckTimeSeconds（按 0.5-30 钳制）时返回 true。
     */
    bool CheckStuck(float DistanceToTarget, float DeltaTime);

    /** 卡住检测：历史最优（最小）XY目标距离，StartMoveToLocation 时以起点距离复位 */
    float BestDistanceToTarget = 0.0f;

    /** 卡住检测：连续无有效进展的累计时长（秒） */
    float NoProgressElapsed = 0.0f;

    /**
     * 判定为有效进展的最小距离改善（cm）。取 1cm：远小于任何肉眼可见移动，
     * 又能过滤浮点抖动——被墙完全阻挡时位移为0，绝不会误刷新。
     */
    static constexpr float StuckProgressEpsilon = 1.0f;
};
