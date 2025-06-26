#pragma once

#include "CoreMinimal.h"
#include "FormationTypes.h"

/**
 * 阵型系统数学工具
 * 提供阵型计算和路径检测相关的数学函数
 */
class FORMATIONSYSTEM_API FFormationMathUtils
{
public:
    /**
     * 检测两条路径是否相交
     * @param Start1 第一条路径的起点
     * @param End1 第一条路径的终点
     * @param Start2 第二条路径的起点
     * @param End2 第二条路径的终点
     * @param Threshold 相交检测阈值
     * @return 是否相交
     */
    static bool DoPathsIntersect(
        const FVector& Start1, const FVector& End1,
        const FVector& Start2, const FVector& End2,
        float Threshold = 50.0f);

    /**
     * 计算分离力（Boids算法）
     * @param UnitIndex 单位索引
     * @param Positions 所有单位的位置数组
     * @param Params Boids参数
     * @return 分离力向量
     */
    static FVector CalculateSeparationForce(
        int32 UnitIndex,
        const TArray<FVector>& Positions,
        const FBoidsMovementParams& Params);

    /**
     * 计算对齐力（Boids算法）
     * @param UnitIndex 单位索引
     * @param Positions 所有单位的位置数组
     * @param Velocities 所有单位的速度数组
     * @param Params Boids参数
     * @return 对齐力向量
     */
    static FVector CalculateAlignmentForce(
        int32 UnitIndex,
        const TArray<FVector>& Positions,
        const TArray<FVector>& Velocities,
        const FBoidsMovementParams& Params);

    /**
     * 计算聚合力（Boids算法）
     * @param UnitIndex 单位索引
     * @param Positions 所有单位的位置数组
     * @param Params Boids参数
     * @return 聚合力向量
     */
    static FVector CalculateCohesionForce(
        int32 UnitIndex,
        const TArray<FVector>& Positions,
        const FBoidsMovementParams& Params);

    /**
     * 计算寻找力（Boids算法）
     * @param CurrentPos 当前位置
     * @param TargetPos 目标位置
     * @param CurrentVelocity 当前速度
     * @param Params Boids参数
     * @return 寻找力向量
     */
    static FVector CalculateSeekForce(
        const FVector& CurrentPos,
        const FVector& TargetPos,
        const FVector& CurrentVelocity,
        const FBoidsMovementParams& Params);

    /**
     * 限制向量大小
     * @param Vector 输入向量
     * @param MaxMagnitude 最大大小
     * @return 限制后的向量
     */
    static FVector LimitVector(const FVector& Vector, float MaxMagnitude);

    /**
     * 应用缓动函数
     * @param Progress 原始进度 (0.0-1.0)
     * @param Strength 缓动强度
     * @return 缓动后的进度
     */
    static float ApplyEasing(float Progress, float Strength);
}; 