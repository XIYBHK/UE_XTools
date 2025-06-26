#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "FormationSystemCore.h"

#include "FormationBlueprintNodes.generated.h"

// 前向声明
class UFormationLibrary;

/**
 * 阵型蓝图节点库
 * 提供专门为蓝图设计的阵型功能节点
 */
UCLASS()
class FORMATIONSYSTEM_API UFormationBlueprintNodes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 快速阵型变换节点（智能分配）
     * 一个节点完成从当前位置到目标阵型的变换，使用智能相对位置匹配，支持阵型变换
     * @param WorldContext 世界上下文
     * @param Units 参与变换的单位数组
     * @param TargetFormationType 目标阵型类型
     * @param FormationTransform 阵型变换（位置、旋转、缩放），可分割结构体引脚
     * @param OutFormationManager 输出的阵型管理器（可用于后续控制）
     * @param FormationSize 阵型尺寸参数
     * @param TransitionDuration 变换持续时间
     * @param TransitionMode 变换模式（推荐使用直接相对位置匹配）
     * @param bShowDebug 是否显示调试信息
     * @return 是否成功开始变换
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation",
        meta = (DisplayName = "快速阵型变换（智能分配）", WorldContext = "WorldContext",
               AdvancedDisplay = "TransitionMode,bShowDebug"))
    static bool QuickFormationTransition(
        const UObject* WorldContext,
        const TArray<AActor*>& Units,
        EFormationType TargetFormationType,
        FTransform FormationTransform,
        UFormationManagerComponent*& OutFormationManager,
        float FormationSize = 200.0f,
        float TransitionDuration = 2.0f,
        EFormationTransitionMode TransitionMode = EFormationTransitionMode::DirectRelativePositionMatching,
        bool bShowDebug = false
    );

    /**
     * 创建阵型管理器
     * 在指定Actor上创建阵型管理器组件
     * @param TargetActor 目标Actor
     * @return 创建的阵型管理器组件
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation", 
        meta = (DisplayName = "创建阵型管理器"))
    static UFormationManagerComponent* CreateFormationManager(AActor* TargetActor);



    /**
     * 阵型变换序列
     * 按顺序执行多个阵型变换
     * @param WorldContext 世界上下文
     * @param Units 参与变换的单位数组
     * @param FormationSequence 阵型序列
     * @param CenterLocation 阵型中心位置
     * @param FormationSize 阵型尺寸
     * @param TransitionDuration 每次变换的持续时间
     * @param SequenceInterval 序列间隔时间
     * @param bLoop 是否循环执行
     * @param bShowDebug 是否显示调试信息
     * @return 创建的阵型管理器组件
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation", 
        meta = (DisplayName = "阵型变换序列", WorldContext = "WorldContext"))
    static UFormationManagerComponent* FormationTransitionSequence(
        const UObject* WorldContext,
        const TArray<AActor*>& Units,
        const TArray<EFormationType>& FormationSequence,
        FVector CenterLocation,
        float FormationSize = 200.0f,
        float TransitionDuration = 2.0f,
        float SequenceInterval = 3.0f,
        bool bLoop = true,
        bool bShowDebug = false
    );

    /**
     * 获取推荐的阵型尺寸
     * 根据单位数量计算推荐的阵型尺寸
     * @param UnitCount 单位数量
     * @param FormationType 阵型类型
     * @param UnitSpacing 单位间距
     * @return 推荐的阵型尺寸
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", 
        meta = (DisplayName = "获取推荐阵型尺寸"))
    static float GetRecommendedFormationSize(
        int32 UnitCount,
        EFormationType FormationType,
        float UnitSpacing = 100.0f
    );



    /**
     * 检查阵型兼容性
     * 检查单位数组是否适合指定的阵型变换
     * @param Units 单位数组
     * @param FromFormationType 起始阵型类型
     * @param ToFormationType 目标阵型类型
     * @param WarningMessage 警告信息（输出）
     * @return 是否兼容
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", 
        meta = (DisplayName = "检查阵型兼容性"))
    static bool CheckFormationCompatibility(
        const TArray<AActor*>& Units,
        EFormationType FromFormationType,
        EFormationType ToFormationType,
        FString& WarningMessage
    );





    /**
     * Character阵型移动（适配移动组件）
     * 专门为Character设计，使用Character自身的移动逻辑而非直接插值，支持阵型变换
     * @param WorldContext 世界上下文
     * @param Characters 参与变换的Character数组
     * @param TargetFormationType 目标阵型类型
     * @param FormationTransform 阵型变换（位置、旋转、缩放），可分割结构体引脚
     * @param OutTargetPositions 输出的目标位置数组（可用于AI移动指令）
     * @param FormationSize 阵型尺寸参数
     * @param TransitionMode 变换模式
     * @param bUseAIMoveTo 是否自动调用AI MoveTo
     * @param AcceptanceRadius AI移动的接受半径
     * @param bShowDebug 是否显示调试信息
     * @return 是否成功计算分配
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation",
        meta = (DisplayName = "Character阵型移动（适配移动组件）", WorldContext = "WorldContext",
               AdvancedDisplay = "TransitionMode,bUseAIMoveTo,AcceptanceRadius,bShowDebug"))
    static bool CharacterFormationMovement(
        const UObject* WorldContext,
        const TArray<class ACharacter*>& Characters,
        EFormationType TargetFormationType,
        FTransform FormationTransform,
        TArray<FVector>& OutTargetPositions,
        float FormationSize = 200.0f,
        EFormationTransitionMode TransitionMode = EFormationTransitionMode::DirectRelativePositionMatching,
        bool bUseAIMoveTo = true,
        float AcceptanceRadius = 50.0f,
        bool bShowDebug = false
    );

    /**
     * RTS群集移动阵型变换
     * 使用改进的RTS游戏算法实现丝滑的群集移动，避免单位穿插碰撞
     * @param WorldContext 世界上下文
     * @param Units 参与变换的单位数组
     * @param TargetFormationType 目标阵型类型
     * @param CenterLocation 阵型中心位置
     * @param OutFormationManager 输出的阵型管理器
     * @param FormationSize 阵型尺寸参数
     * @param TransitionDuration 变换持续时间
     * @param BoidsParams Boids群集移动参数
     * @param bShowDebug 是否显示调试信息
     * @return 是否成功开始变换
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation",
        meta = (DisplayName = "RTS群集移动变换", WorldContext = "WorldContext",
               AdvancedDisplay = "FormationSize,TransitionDuration,BoidsParams,bShowDebug"))
    static bool RTSFlockFormationTransition(
        const UObject* WorldContext,
        const TArray<AActor*>& Units,
        EFormationType TargetFormationType,
        FVector CenterLocation,
        UFormationManagerComponent*& OutFormationManager,
        float FormationSize = 200.0f,
        float TransitionDuration = 3.0f,
        const FBoidsMovementParams& BoidsParams = FBoidsMovementParams(),
        bool bShowDebug = false
    );

    /**
     * 路径感知阵型变换
     * 预测并避免路径冲突的智能阵型变换
     * @param WorldContext 世界上下文
     * @param Units 参与变换的单位数组
     * @param TargetFormationType 目标阵型类型
     * @param CenterLocation 阵型中心位置
     * @param OutFormationManager 输出的阵型管理器
     * @param OutConflictInfo 输出的路径冲突信息
     * @param FormationSize 阵型尺寸参数
     * @param TransitionDuration 变换持续时间
     * @param bShowDebug 是否显示调试信息
     * @return 是否成功开始变换
     */
    UFUNCTION(BlueprintCallable, Category = "XTools|Formation",
        meta = (DisplayName = "路径感知阵型变换", WorldContext = "WorldContext",
               AdvancedDisplay = "FormationSize,TransitionDuration,bShowDebug"))
    static bool PathAwareFormationTransition(
        const UObject* WorldContext,
        const TArray<AActor*>& Units,
        EFormationType TargetFormationType,
        FVector CenterLocation,
        UFormationManagerComponent*& OutFormationManager,
        FPathConflictInfo& OutConflictInfo,
        float FormationSize = 200.0f,
        float TransitionDuration = 3.0f,
        bool bShowDebug = false
    );



    /** 根据阵型类型创建阵型数据 */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "根据类型创建阵型"))
    static FFormationData CreateFormationByType(
        EFormationType FormationType,
        FVector CenterLocation,
        float FormationSize,
        int32 UnitCount
    );

    /**
     * 应用变换到阵型数据
     * 对阵型的所有位置应用旋转和缩放变换
     * @param FormationData 原始阵型数据
     * @param Transform 要应用的变换
     * @return 变换后的阵型数据
     */
    UFUNCTION(BlueprintPure, Category = "XTools|Formation", meta = (DisplayName = "应用阵型变换"))
    static FFormationData ApplyFormationTransform(
        const FFormationData& FormationData,
        const FTransform& Transform
    );

private:
};
