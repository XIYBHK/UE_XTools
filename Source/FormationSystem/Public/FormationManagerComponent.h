#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FormationTypes.h"
#include "FormationManagerComponent.generated.h"

// 前向声明
class IFormationInterface;

/**
 * 阵型管理器组件
 * 负责管理阵型变换的核心逻辑
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent, DisplayName = "阵型管理器"))
class FORMATIONSYSTEM_API UFormationManagerComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UFormationManagerComponent();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    /** 当前阵型变换状态 */
    UPROPERTY(BlueprintReadOnly, Category = "Formation", meta = (DisplayName = "变换状态"))
    FFormationTransitionState TransitionState;

    /**
     * 检测两个阵型是否为相同阵型的平移关系
     * @param FromPositions 起始阵型位置
     * @param ToPositions 目标阵型位置
     * @return 是否为相同阵型平移
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "检测相同阵型平移"))
    bool IsFormationTranslation(const TArray<FVector>& FromPositions, const TArray<FVector>& ToPositions);

    /**
     * 开始阵型变换
     * @param Units 参与变换的单位数组
     * @param FromFormation 起始阵型
     * @param ToFormation 目标阵型
     * @param Config 变换配置
     * @return 是否成功开始变换
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "开始阵型变换"))
    bool StartFormationTransition(
        const TArray<AActor*>& Units,
        const FFormationData& FromFormation,
        const FFormationData& ToFormation,
        const FFormationTransitionConfig& Config = FFormationTransitionConfig()
    );

    /**
     * 开始阵型变换（支持接口）
     * 此版本会检查单位是否实现了IFormationInterface接口，并调用相应回调
     * @param Units 参与变换的单位数组
     * @param FromFormation 起始阵型
     * @param ToFormation 目标阵型
     * @param Config 变换配置
     * @return 是否成功开始变换
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "开始阵型变换（接口支持）", AdvancedDisplay = "Config"))
    bool StartFormationTransitionWithInterface(
        const TArray<AActor*>& Units,
        const FFormationData& FromFormation,
        const FFormationData& ToFormation,
        const FFormationTransitionConfig& Config = FFormationTransitionConfig()
    );

    /**
     * 停止当前的阵型变换
     * @param bSnapToTarget 是否立即移动到目标位置
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "停止阵型变换"))
    void StopFormationTransition(bool bSnapToTarget = false);

    /**
     * 检查是否正在进行阵型变换
     */
    UFUNCTION(BlueprintPure, Category = "Formation", meta = (DisplayName = "是否正在变换"))
    bool IsTransitioning() const { return TransitionState.bIsTransitioning; }

    /**
     * 获取变换进度
     */
    UFUNCTION(BlueprintPure, Category = "Formation", meta = (DisplayName = "获取变换进度"))
    float GetTransitionProgress() const { return TransitionState.OverallProgress; }

    /** 获取变换状态 */
    UFUNCTION(BlueprintPure, Category = "Formation", meta = (DisplayName = "获取变换状态"))
    FFormationTransitionState GetTransitionState() const { return TransitionState; }

    /** 计算最优分配方案（公开接口，供外部调用） */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "计算最优分配"))
    virtual TArray<int32> CalculateOptimalAssignment(const TArray<FVector>& FromPositions, const TArray<FVector>& ToPositions, EFormationTransitionMode Mode);

    /** 设置Boids群集移动参数 */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "设置群集移动参数"))
    void SetBoidsMovementParams(const FBoidsMovementParams& NewParams);

    /** 获取Boids群集移动参数 */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "获取群集移动参数"))
    FBoidsMovementParams GetBoidsMovementParams() const;

    /** 检测阵型路径冲突 */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "检测路径冲突"))
    FPathConflictInfo CheckFormationPathConflicts(const TArray<FVector>& FromPositions, const TArray<FVector>& ToPositions);

protected:
    /** 🎯 核心算法委托 - 简化架构，提高可维护性 */

    /** 计算分配方案的核心接口 */
    TArray<int32> CalculateAssignmentByMode(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions,
        EFormationTransitionMode Mode);

    /** 🔧 算法工具函数 */

    /** 创建成本矩阵 */
    TArray<TArray<float>> CreateCostMatrix(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions,
        bool bUseRelativePosition = true);

    /** 🚀 成本矩阵计算函数 */
    TArray<TArray<float>> CalculateRelativePositionCostMatrix(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    TArray<TArray<float>> CalculateAbsoluteDistanceCostMatrix(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    /** 应用算法特定的成本修正 */
    void ApplyCostModifications(
        TArray<TArray<float>>& CostMatrix,
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions,
        EFormationTransitionMode Mode);

    /** 匈牙利算法求解分配问题 */
    TArray<int32> SolveAssignmentHungarian(const TArray<TArray<float>>& CostMatrix);

    /** 贪心算法求解分配问题 */
    TArray<int32> SolveAssignmentGreedy(const TArray<TArray<float>>& CostMatrix);

    /** 通用分配问题求解器 */
    TArray<int32> SolveAssignmentProblem(const TArray<TArray<float>>& CostMatrix);

    /** 🚀 特殊分配算法 */
    TArray<int32> CalculateDirectRelativePositionMatching(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    TArray<int32> CalculateRTSFlockMovementAssignment(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    TArray<int32> CalculatePathAwareAssignment(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    TArray<int32> CalculateSpatialOrderMapping(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    TArray<int32> CalculateDistancePriorityAssignment(
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    /** 检测路径冲突 */
    FPathConflictInfo DetectPathConflicts(const TArray<int32>& Assignment, const TArray<FVector>& FromPositions, const TArray<FVector>& ToPositions);

    /** 🚀 辅助函数 */
    bool DetectSpiralFormation(const TArray<FSpatialSortData>& SortedData);

    float CalculateSpiralParameter(float Angle, float Distance);

    float CalculateFlockingBonus(
        int32 FromIndex,
        int32 ToIndex,
        const TArray<FVector>& FromPositions,
        const TArray<FVector>& ToPositions);

    /** 更新单位位置 */
    void UpdateUnitPositions(float DeltaTime);

    /** 绘制调试信息 */
    void DrawDebugInfo() const;

    /** 应用缓动 */
    float ApplyEasing(float Progress, float Strength) const;

    /** 通知实现了IFormationInterface接口的单位 */
    void NotifyFormationInterfaceActors(const TArray<AActor*>& Units, const TArray<int32>& Assignment, const FFormationData& ToFormation);

private:
    /** 🚀 性能优化：智能缓存系统 */

    /** 成本矩阵缓存结构 */
    struct FCostMatrixCache
    {
        uint32 PositionsHash = 0;
        EFormationTransitionMode Mode = EFormationTransitionMode::OptimizedAssignment;
        TArray<TArray<float>> CostMatrix;
        double CacheTime = 0.0;

        bool IsValid(uint32 NewHash, EFormationTransitionMode NewMode, double CurrentTime) const;
        void UpdateCache(uint32 NewHash, EFormationTransitionMode NewMode, const TArray<TArray<float>>& NewMatrix, double CurrentTime);
    };

    /** 成本矩阵缓存 */
    mutable FCostMatrixCache CostMatrixCache;

    /** 计算位置数组的哈希值 */
    uint32 CalculatePositionsHash(const TArray<FVector>& FromPositions, const TArray<FVector>& ToPositions) const;
};