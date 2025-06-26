#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FormationSystemCore.h"
#include "FormationLibrary.generated.h"

/**
 * 阵型函数库
 * 提供蓝图可调用的阵型生成和管理功能
 */
UCLASS()
class FORMATIONSYSTEM_API UFormationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 创建方形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Spacing 单位间距
     * @param RowCount 行数（0表示自动计算）
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建方形阵型"))
    static FFormationData CreateSquareFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Spacing = 100.0f,
        int32 RowCount = 0
    );

    /**
     * 创建圆形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Radius 圆形半径
     * @param StartAngle 起始角度（度）
     * @param bClockwise 是否顺时针排列
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建圆形阵型"))
    static FFormationData CreateCircleFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Radius = 200.0f,
        float StartAngle = 0.0f,
        bool bClockwise = true
    );

    /**
     * 创建线形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Spacing 单位间距
     * @param bVertical 是否垂直排列
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建线形阵型"))
    static FFormationData CreateLineFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Spacing = 100.0f,
        bool bVertical = false
    );

    /**
     * 创建三角形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Spacing 单位间距
     * @param bInverted 是否倒三角
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建三角形阵型"))
    static FFormationData CreateTriangleFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Spacing = 100.0f,
        bool bInverted = false
    );

    /**
     * 创建箭头形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Spacing 单位间距
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建箭头形阵型"))
    static FFormationData CreateArrowFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Spacing = 100.0f
    );

    /**
     * 创建螺旋线形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Radius 螺旋半径
     * @param Turns 螺旋圈数
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建螺旋线形阵型"))
    static FFormationData CreateSpiralFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Radius = 200.0f,
        float Turns = 2.0f
    );

    /**
     * 创建实心圆形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Radius 最大半径
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建实心圆形阵型"))
    static FFormationData CreateSolidCircleFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Radius = 200.0f
    );

    /**
     * 创建折线形阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param UnitCount 单位数量
     * @param Spacing 单位间距
     * @param ZigzagAmplitude 折线幅度
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建折线形阵型"))
    static FFormationData CreateZigzagFormation(
        FVector CenterLocation,
        FRotator Rotation,
        int32 UnitCount,
        float Spacing = 100.0f,
        float ZigzagAmplitude = 100.0f
    );

    /**
     * 创建自定义阵型
     * @param CenterLocation 阵型中心位置
     * @param Rotation 阵型旋转
     * @param RelativePositions 相对位置数组
     * @return 生成的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "创建自定义阵型"))
    static FFormationData CreateCustomFormation(
        FVector CenterLocation,
        FRotator Rotation,
        const TArray<FVector>& RelativePositions
    );

    /**
     * 从Actor数组获取当前阵型
     * @param Units Actor数组
     * @param CenterLocation 阵型中心位置（输出）
     * @return 当前阵型数据
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation", meta = (DisplayName = "从Actor获取当前阵型"))
    static FFormationData GetCurrentFormationFromActors(
        const TArray<AActor*>& Units,
        FVector& CenterLocation
    );

    /**
     * 计算阵型的包围盒
     * @param Formation 阵型数据
     * @return 包围盒
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "获取阵型包围盒"))
    static FBox GetFormationBounds(const FFormationData& Formation);

    /**
     * 缩放阵型
     * @param Formation 原始阵型
     * @param Scale 缩放比例
     * @return 缩放后的阵型
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "缩放阵型"))
    static FFormationData ScaleFormation(const FFormationData& Formation, float Scale);

    /**
     * 旋转阵型
     * @param Formation 原始阵型
     * @param AdditionalRotation 额外旋转
     * @return 旋转后的阵型
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "旋转阵型"))
    static FFormationData RotateFormation(const FFormationData& Formation, FRotator AdditionalRotation);

    /**
     * 移动阵型
     * @param Formation 原始阵型
     * @param NewCenterLocation 新的中心位置
     * @return 移动后的阵型
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "移动阵型"))
    static FFormationData MoveFormation(const FFormationData& Formation, FVector NewCenterLocation);

    /**
     * 调整阵型单位数量
     * @param Formation 原始阵型
     * @param NewUnitCount 新的单位数量
     * @return 调整后的阵型
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation", meta = (DisplayName = "调整阵型单位数量"))
    static FFormationData ResizeFormation(const FFormationData& Formation, int32 NewUnitCount);

    /**
     * 绘制阵型调试信息
     * @param WorldContext 世界上下文
     * @param Formation 阵型数据
     * @param Duration 显示持续时间
     * @param Color 显示颜色
     * @param Thickness 线条粗细
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation", 
        meta = (DisplayName = "绘制阵型调试", WorldContext = "WorldContext"))
    static void DrawFormationDebug(
        const UObject* WorldContext,
        const FFormationData& Formation,
        float Duration = 5.0f,
        FLinearColor Color = FLinearColor::Red,
        float Thickness = 2.0f
    );

    /**
     * 验证阵型数据
     * @param Formation 要验证的阵型数据
     * @param ErrorMessage 错误信息（输出）
     * @return 是否有效
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "验证阵型数据"))
    static bool ValidateFormationData(const FFormationData& Formation, FString& ErrorMessage);

    /**
     * 计算两个阵型之间的变换成本
     * @param FromFormation 起始阵型
     * @param ToFormation 目标阵型
     * @param TransitionMode 变换模式
     * @return 变换成本
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "计算变换成本"))
    static float CalculateTransitionCost(
        const FFormationData& FromFormation,
        const FFormationData& ToFormation,
        EFormationTransitionMode TransitionMode = EFormationTransitionMode::OptimizedAssignment
    );

private:
    /** 计算最佳行列数 */
    static void CalculateOptimalRowsCols(int32 UnitCount, int32& OutRows, int32& OutCols);

    /** 生成三角形的行分布 */
    static TArray<int32> GenerateTriangleRowDistribution(int32 UnitCount, bool bInverted);
};
