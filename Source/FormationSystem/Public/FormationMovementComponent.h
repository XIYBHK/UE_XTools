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
        ToolTip = "开始移动到目标位置。距离判定只比较XY平面（忽略Z轴高度差）；起点已在接受半径内时会立即广播完成事件。\n注意：没有自动卡墙超时或失败事件——被阻挡时不会自行结束，需要时请调用\"停止移动\"手动终止。"))
    void StartMoveToLocation(FVector TargetLocation, float AcceptanceRadius = 50.0f, float MoveSpeed = 1.0f);

    /**
     * 停止移动
     */
    UFUNCTION(BlueprintCallable, Category = "Formation Movement", meta = (DisplayName = "停止移动",
        ToolTip = "立即停止移动并清除状态：重置速度、清除待处理的移动输入、禁用组件Tick。\n不广播完成事件。"))
    void StopMovement();

    /**
     * 是否正在移动
     */
    UFUNCTION(BlueprintPure, Category = "Formation Movement", meta = (DisplayName = "是否正在移动",
        ToolTip = "是否处于移动中（从开始移动到进入接受半径或手动停止）。\n注意：被阻挡卡住时仍返回true——组件没有卡墙检测，需要时请自行结合距离或超时判断。"))
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
        ToolTip = "移动完成事件：仅表示XY平面距离进入接受半径。\n不代表路径可行或过程无阻挡——被卡住时不会广播任何事件（组件没有失败事件，需自行超时判断）。"))
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
};
