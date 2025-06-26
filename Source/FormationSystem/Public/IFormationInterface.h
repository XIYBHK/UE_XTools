#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FormationTypes.h"
#include "IFormationInterface.generated.h"

/**
 * 阵型接口
 * 实现此接口的对象可以参与阵型变换系统
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UFormationInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * 阵型接口实现类
 * 提供阵型变换相关的回调函数
 */
class FORMATIONSYSTEM_API IFormationInterface
{
    GENERATED_BODY()

public:
    /**
     * 当单位被分配到阵型位置时调用
     * @param TargetPosition 目标位置
     * @param TransitionConfig 变换配置
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Formation")
    void OnFormationPositionAssigned(const FVector& TargetPosition, const FFormationTransitionConfig& TransitionConfig);

    /**
     * 当单位开始阵型变换时调用
     * @param StartPosition 起始位置
     * @param TargetPosition 目标位置
     * @param TransitionConfig 变换配置
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Formation")
    void OnFormationTransitionStarted(const FVector& StartPosition, const FVector& TargetPosition, const FFormationTransitionConfig& TransitionConfig);

    /**
     * 当单位完成阵型变换时调用
     * @param FinalPosition 最终位置
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Formation")
    void OnFormationTransitionCompleted(const FVector& FinalPosition);

    /**
     * 检查单位是否可以参与阵型变换
     * @return 是否可以参与变换
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Formation")
    bool CanParticipateInFormation() const;
    virtual bool CanParticipateInFormation_Implementation() const { return true; }
}; 