#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QueueSplineLibrary.generated.h"

class USplineComponent;

UENUM(BlueprintType)
enum class EXToolsQueueSplineFillMode : uint8
{
	FromStart UMETA(DisplayName = "从起点进入"),
	PreFilledRatio UMETA(DisplayName = "按比例预填充")
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsQueueSplineConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "人数"))
	int32 UnitCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "前后间距", ClampMin = "1.0"))
	double Spacing = 100.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "填充模式"))
	EXToolsQueueSplineFillMode FillMode = EXToolsQueueSplineFillMode::FromStart;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "填充比例", ClampMin = "0.0", ClampMax = "1.0"))
	double FillRatio = 1.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "起始距离"))
	double StartDistance = 0.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "头部在前"))
	bool bHeadAtEnd = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "左右交错"))
	bool bAlternateSides = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "横向偏移"))
	double SideOffset = 35.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "随机横向抖动"))
	double SideJitter = 10.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "随机前后抖动"))
	double DistanceJitter = 15.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "随机种子"))
	int32 RandomSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "XTools|排队样条线", meta = (DisplayName = "限制到样条范围"))
	bool bClampToSpline = true;
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsQueueSplineSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "索引"))
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "样条距离"))
	double Distance = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "向右偏移"))
	double RightOffset = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "中心位置"))
	FVector CenterLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "目标位置"))
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "目标旋转"))
	FRotator TargetRotation = FRotator::ZeroRotator;
};

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UQueueSplineLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线",
		meta = (DisplayName = "生成排队样条线槽位", Keywords = "排队 样条 队列 槽位 位置 Spline Queue Slot",
			ToolTip = "按样条距离生成排队槽位，支持起点进入或按比例预填充，并给每个槽位计算左右交错与随机偏移。"))
	static TArray<FXToolsQueueSplineSlot> GenerateQueueSplineSlots(
		UPARAM(DisplayName = "样条组件") USplineComponent* SplineComponent,
		UPARAM(DisplayName = "配置") const FXToolsQueueSplineConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "XTools|排队样条线",
		meta = (DisplayName = "计算排队样条线槽位", Keywords = "排队 样条 单个 槽位 位置 Spline Queue Slot",
			ToolTip = "计算单个队伍索引对应的样条槽位，适合运行时逐个刷新目标点。"))
	static bool CalculateQueueSplineSlot(
		UPARAM(DisplayName = "样条组件") USplineComponent* SplineComponent,
		UPARAM(DisplayName = "队伍索引") int32 UnitIndex,
		UPARAM(DisplayName = "配置") const FXToolsQueueSplineConfig& Config,
		UPARAM(DisplayName = "槽位") FXToolsQueueSplineSlot& OutSlot);

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线",
		meta = (DisplayName = "插值回归样条偏移", Keywords = "排队 样条 偏移 回归 插值 Spline Queue Offset",
			ToolTip = "将当前横向偏移平滑插值到目标偏移。可在Tick中用于人物自然回到样条线附近。"))
	static double InterpQueueSplineRightOffset(
		UPARAM(DisplayName = "当前偏移") double CurrentRightOffset,
		UPARAM(DisplayName = "目标偏移") double TargetRightOffset,
		UPARAM(DisplayName = "DeltaTime") double DeltaTime,
		UPARAM(DisplayName = "回归速度") double ReturnSpeed = 6.0);

	UFUNCTION(BlueprintPure, Category = "XTools|排队样条线",
		meta = (DisplayName = "排队样条线配置有效", Keywords = "排队 样条 配置 检查 Spline Queue Validate"))
	static bool IsQueueSplineConfigValid(
		UPARAM(DisplayName = "样条组件") USplineComponent* SplineComponent,
		UPARAM(DisplayName = "配置") const FXToolsQueueSplineConfig& Config,
		UPARAM(DisplayName = "错误信息") FString& OutMessage);
};
