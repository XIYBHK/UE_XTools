#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FormationSystem.h"
#include "FormationLibrary.h"
#include "FormationTestActor.generated.h"

/**
 * 阵型测试Actor
 * 用于演示和测试阵型变换功能
 */
UCLASS(BlueprintType, Blueprintable)
class FORMATIONSYSTEM_API AFormationTestActor : public AActor
{
    GENERATED_BODY()

public:
    AFormationTestActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    /** 阵型管理器组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Formation", meta = (DisplayName = "阵型管理器"))
    UFormationManagerComponent* FormationManager;

    /** 测试用的单位Actor数组 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (DisplayName = "测试单位"))
    TArray<AActor*> TestUnits;

    /** 当前阵型索引 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Formation", meta = (DisplayName = "当前阵型索引"))
    int32 CurrentFormationIndex = 0;

    /** 预定义的阵型数组 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Formation", meta = (DisplayName = "预定义阵型"))
    TArray<FFormationData> PredefinedFormations;

    /** 单位间距 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (DisplayName = "单位间距", ClampMin = "50.0", ClampMax = "500.0"))
    float UnitSpacing = 100.0f;

    /** 变换持续时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (DisplayName = "变换时间", ClampMin = "0.5", ClampMax = "10.0"))
    float TransitionDuration = 3.0f;

    /** 是否自动循环变换 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (DisplayName = "自动循环"))
    bool bAutoLoop = false;

    /** 自动循环间隔时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (DisplayName = "循环间隔", EditCondition = "bAutoLoop", ClampMin = "1.0", ClampMax = "30.0"))
    float LoopInterval = 5.0f;

    /** 是否显示调试信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (DisplayName = "显示调试"))
    bool bShowDebug = true;

    /** 变换模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (DisplayName = "变换模式"))
    EFormationTransitionMode TransitionMode = EFormationTransitionMode::OptimizedAssignment;

private:
    /** 上次变换时间 */
    float LastTransitionTime = 0.0f;

    /** 是否已初始化 */
    bool bInitialized = false;

public:
    /**
     * 初始化预定义阵型
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "初始化预定义阵型"))
    void InitializePredefinedFormations();

    /**
     * 切换到下一个阵型
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "切换到下一个阵型"))
    void SwitchToNextFormation();

    /**
     * 切换到指定阵型
     * @param FormationIndex 阵型索引
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "切换到指定阵型"))
    void SwitchToFormation(int32 FormationIndex);

    /**
     * 创建测试单位
     * @param UnitCount 单位数量
     * @param UnitClass 单位类型
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "创建测试单位"))
    void CreateTestUnits(int32 UnitCount = 16, TSubclassOf<AActor> UnitClass = nullptr);

    /**
     * 清理测试单位
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "清理测试单位"))
    void ClearTestUnits();

    /**
     * 开始演示
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "开始演示"))
    void StartDemo();

    /**
     * 停止演示
     */
    UFUNCTION(BlueprintCallable, Category = "Formation", meta = (DisplayName = "停止演示"))
    void StopDemo();

    /**
     * 获取阵型名称
     * @param FormationIndex 阵型索引
     * @return 阵型名称
     */
    UFUNCTION(BlueprintPure, Category = "Formation", meta = (DisplayName = "获取阵型名称"))
    FString GetFormationName(int32 FormationIndex) const;

    /**
     * 获取当前阵型名称
     */
    UFUNCTION(BlueprintPure, Category = "Formation", meta = (DisplayName = "获取当前阵型名称"))
    FString GetCurrentFormationName() const;

    /**
     * 检查是否正在变换
     */
    UFUNCTION(BlueprintPure, Category = "Formation", meta = (DisplayName = "是否正在变换"))
    bool IsTransitioning() const;

    /**
     * 获取变换进度
     */
    UFUNCTION(BlueprintPure, Category = "Formation", meta = (DisplayName = "获取变换进度"))
    float GetTransitionProgress() const;

private:
    /** 更新自动循环逻辑 */
    void UpdateAutoLoop(float DeltaTime);

    /** 生成默认单位Actor */
    AActor* CreateDefaultUnit(FVector Location);
};
