/*
 * Copyright (c) 2025 XIYBHK
 * Licensed under UE_XTools License
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "GeometryInstance.generated.h"

/**
 * 几何采样模式
 */
UENUM(BlueprintType, Category = "XTools|几何工具")
enum class EGeometrySamplingMode : uint8
{
    Surface UMETA(DisplayName = "表面采样"),
    Volume UMETA(DisplayName = "体积采样"),
    Boundary UMETA(DisplayName = "边界采样")
};

/**
 * 旋转模式
 */
UENUM(BlueprintType, Category = "XTools|几何工具")
enum class EGeometryRotationMode : uint8
{
    None UMETA(DisplayName = "无旋转"),
    Fixed UMETA(DisplayName = "固定旋转"),
    Random UMETA(DisplayName = "随机旋转"),
    LookAtCenter UMETA(DisplayName = "朝向中心"),
    LookAtOrigin UMETA(DisplayName = "朝向原点")
};

/**
 * 缩放模式
 */
UENUM(BlueprintType, Category = "XTools|几何工具")
enum class EGeometryScaleMode : uint8
{
    Uniform UMETA(DisplayName = "统一缩放"),
    Fixed UMETA(DisplayName = "固定缩放"),
    Random UMETA(DisplayName = "随机缩放")
};

/**
 * 几何采样参数结构体（预留未来使用）
 */
USTRUCT(BlueprintType, Category = "XTools|几何工具")
struct FGeometrySamplingParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础参数", 
        meta = (DisplayName = "点间距", ClampMin = "1.0", UIMin = "1.0", UIMax = "1000.0"))
    float Distance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "基础参数",
        meta = (DisplayName = "噪声强度", ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
    float Noise = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "采样模式", meta = (DisplayName = "采样模式"))
    EGeometrySamplingMode SamplingMode = EGeometrySamplingMode::Surface;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数", meta = (DisplayName = "旋转模式"))
    EGeometryRotationMode RotationMode = EGeometryRotationMode::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数",
        meta = (DisplayName = "固定旋转", EditCondition = "RotationMode == EGeometryRotationMode::Fixed"))
    FRotator FixedRotation = FRotator(0, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数",
        meta = (DisplayName = "随机旋转最小值", EditCondition = "RotationMode == EGeometryRotationMode::Random"))
    FRotator RandomRotationMin = FRotator(0, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数",
        meta = (DisplayName = "随机旋转最大值", EditCondition = "RotationMode == EGeometryRotationMode::Random"))
    FRotator RandomRotationMax = FRotator(0, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数", meta = (DisplayName = "缩放模式"))
    EGeometryScaleMode ScaleMode = EGeometryScaleMode::Uniform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数",
        meta = (DisplayName = "固定缩放", EditCondition = "ScaleMode == EGeometryScaleMode::Fixed"))
    FVector FixedScale = FVector(1, 1, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数",
        meta = (DisplayName = "随机缩放最小值", EditCondition = "ScaleMode == EGeometryScaleMode::Random"))
    FVector RandomScaleMin = FVector(0.5f, 0.5f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数",
        meta = (DisplayName = "随机缩放最大值", EditCondition = "ScaleMode == EGeometryScaleMode::Random"))
    FVector RandomScaleMax = FVector(2.0f, 2.0f, 2.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "变换参数", meta = (DisplayName = "旋转偏移"))
    FRotator RotationDelta = FRotator(0, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "实例化", meta = (DisplayName = "直接添加实例"))
    bool bAddInstances = true;

    FGeometrySamplingParams() = default;
};

/**
 * 几何实例化组件
 * 继承自 UInstancedStaticMeshComponent，提供基于形状组件的点阵生成功能。
 */
UCLASS(ClassGroup=(XTools), meta=(BlueprintSpawnableComponent, DisplayName="几何实例化组件"))
class GEOMETRYTOOL_API UGeometryInstance : public UInstancedStaticMeshComponent
{
    GENERATED_BODY()

public:
    UGeometryInstance();

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    UFUNCTION(BlueprintCallable, Category = "XTools|几何工具|形状采样",
        meta = (DisplayName = "根据形状生成点阵",
            ToolTip = "根据指定的形状组件（Box、Sphere、Capsule）生成点阵。\n\n参数：\nShape - 形状组件\nbIsAddInstance - 是否直接添加为实例\nDistance - 点间距\nNoise - 噪声强度\nbIsUseLookAtOrigin - 是否朝向原点\nRotator_A/B - 随机旋转范围\nbIsUseRandomRotation - 是否使用随机旋转\nSize_A/B - 随机缩放范围\nbIsUseRandomSize - 是否使用随机缩放\nRotator_Delta - 旋转偏移量\n\n返回值：\n生成的变换数组",
            Keywords = "形状,采样,Box,Sphere,Capsule"))
    TArray<FTransform> GetPointsByShape(
        UPARAM(DisplayName="形状组件") UShapeComponent* Shape,
        UPARAM(DisplayName="直接添加实例") bool bIsAddInstance = true,
        UPARAM(DisplayName="点间距") float Distance = 100.f,
        UPARAM(DisplayName="噪声强度") float Noise = 0.f,
        UPARAM(DisplayName="朝向原点") bool bIsUseLookAtOrigin = false,
        UPARAM(DisplayName="旋转最小值") FRotator Rotator_A = FRotator(0, 0, 0),
        UPARAM(DisplayName="旋转最大值") FRotator Rotator_B = FRotator(0, 0, 0),
        UPARAM(DisplayName="使用随机旋转") bool bIsUseRandomRotation = false,
        UPARAM(DisplayName="缩放最小值") FVector Size_A = FVector(1, 1, 1),
        UPARAM(DisplayName="缩放最大值") FVector Size_B = FVector(1, 1, 1),
        UPARAM(DisplayName="使用随机缩放") bool bIsUseRandomSize = false,
        UPARAM(DisplayName="旋转偏移") FRotator Rotator_Delta = FRotator(0, 0, 0));

    UFUNCTION(BlueprintCallable, Category = "XTools|几何工具|矩形采样",
        meta = (DisplayName = "自定义矩形区域点阵",
            ToolTip = "在指定的自定义矩形区域内生成规则点阵。\n\n参数：\nOriginTransform - 原点变换\nCounts3D - 三维点数量\nDistance3D - 三维间距\nbIsUseWorldSpace - 是否使用世界坐标系\nRotator_A/B - 随机旋转范围\nbIsUseRandomRotation - 是否使用随机旋转\nSize_A/B - 随机缩放范围\nbIsUseRandomSize - 是否使用随机缩放",
            Keywords = "矩形,自定义,3D"))
    void GetPointsByCustomRect(
        UPARAM(DisplayName="原点变换") FTransform OriginTransform,
        UPARAM(DisplayName="三维点数量") FVector Counts3D = FVector(5.f, 5.f, 1.f),
        UPARAM(DisplayName="三维间距") FVector Distance3D = FVector(100.f, 100.f, 100.f),
        UPARAM(DisplayName="使用世界坐标系") bool bIsUseWorldSpace = true,
        UPARAM(DisplayName="旋转最小值") FRotator Rotator_A = FRotator(0, 0, 0),
        UPARAM(DisplayName="旋转最大值") FRotator Rotator_B = FRotator(0, 0, 0),
        UPARAM(DisplayName="使用随机旋转") bool bIsUseRandomRotation = false,
        UPARAM(DisplayName="缩放最小值") FVector Size_A = FVector(1, 1, 1),
        UPARAM(DisplayName="缩放最大值") FVector Size_B = FVector(1, 1, 1),
        UPARAM(DisplayName="使用随机缩放") bool bIsUseRandomSize = false);

    UFUNCTION(BlueprintCallable, Category = "XTools|几何工具|圆形采样",
        meta = (DisplayName = "圆形多层次点阵",
            ToolTip = "生成多层次圆形点阵，从中心向外扩展。\n\n参数：\nInitCount - 初始圆环点数\nInitAngle - 起始角度\nLevel - 层数\nRadiusDelta - 半径增量\n\n返回值：\n生成的变换数组",
            Keywords = "圆形,多层,螺旋"))
    TArray<FTransform> GetPointsByCircle(
        UPARAM(DisplayName="初始圆环点数") int32 InitCount = 5,
        UPARAM(DisplayName="起始角度") float InitAngle = 0.f,
        UPARAM(DisplayName="层数") int32 Level = 3,
        UPARAM(DisplayName="半径增量") float RadiusDelta = 100.0f);

private:
    TArray<FTransform> GenerateSpherePoints(
        class USphereComponent* Sphere,
        float Distance,
        float Noise,
        bool bIsUseLookAtOrigin,
        const FRotator& Rotator_A,
        const FRotator& Rotator_B,
        bool bIsUseRandomRotation,
        const FVector& Size_A,
        const FVector& Size_B,
        bool bIsUseRandomSize,
        const FRotator& Rotator_Delta);

    TArray<FTransform> GenerateBoxPoints(
        class UBoxComponent* Box,
        float Distance,
        float Noise,
        bool bIsUseLookAtOrigin,
        const FRotator& Rotator_A,
        const FRotator& Rotator_B,
        bool bIsUseRandomRotation,
        const FVector& Size_A,
        const FVector& Size_B,
        bool bIsUseRandomSize,
        const FRotator& Rotator_Delta);

    TArray<FTransform> GenerateCapsulePoints(
        class UCapsuleComponent* Capsule,
        float Distance,
        float Noise,
        bool bIsUseLookAtOrigin,
        const FRotator& Rotator_A,
        const FRotator& Rotator_B,
        bool bIsUseRandomRotation,
        const FVector& Size_A,
        const FVector& Size_B,
        bool bIsUseRandomSize,
        const FRotator& Rotator_Delta);

    void ApplyTransformParameters(
        FTransform& OutTransform,
        bool bIsUseLookAtOrigin,
        bool bIsUseRandomRotation,
        bool bIsUseRandomSize,
        const FRotator& Rotator_A,
        const FRotator& Rotator_B,
        const FVector& Size_A,
        const FVector& Size_B,
        const FRotator& Rotator_Delta,
        const FVector& Origin,
        const FVector& PointLocation) const;
};