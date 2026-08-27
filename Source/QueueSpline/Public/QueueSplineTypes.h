#pragma once

#include "CoreMinimal.h"
#include "QueueSplineTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EQueueSplineFillMode : uint8
{
	FromStart UMETA(DisplayName = "从入口进入"),
	PreFilledRatio UMETA(DisplayName = "按比例预填充")
};

UENUM(BlueprintType)
enum class EQueueSplineMovementMode : uint8
{
	ManualTarget UMETA(DisplayName = "只输出目标"),
	AddMovementInput UMETA(DisplayName = "移动输入"),
	DirectInterp UMETA(DisplayName = "直接插值")
};

UENUM(BlueprintType)
enum class EQueueSplineMemberPhase : uint8
{
	Queued UMETA(DisplayName = "排队中"),
	Serving UMETA(DisplayName = "服务中"),
	Exiting UMETA(DisplayName = "离开中")
};

USTRUCT(BlueprintType)
struct QUEUESPLINE_API FQueueSplineMemberHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "ID"))
	FGuid Id;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "代数"))
	int32 Generation = 0;

	bool IsValid() const
	{
		return Id.IsValid() && Generation > 0;
	}

	bool operator==(const FQueueSplineMemberHandle& Other) const
	{
		return Id == Other.Id && Generation == Other.Generation;
	}
};

FORCEINLINE uint32 GetTypeHash(const FQueueSplineMemberHandle& Handle)
{
	return HashCombine(GetTypeHash(Handle.Id), GetTypeHash(Handle.Generation));
}

USTRUCT(BlueprintType)
struct QUEUESPLINE_API FQueueSplineSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "前后间距", ClampMin = "1.0"))
	double Spacing = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "起始方式"))
	EQueueSplineFillMode FillMode = EQueueSplineFillMode::FromStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "预填充比例", ClampMin = "0.0", ClampMax = "1.0"))
	double FillRatio = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "入口距离"))
	double EntryDistance = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "队头朝终点"))
	bool bHeadTowardSplineEnd = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "左右交错"))
	bool bAlternateSides = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "横向偏移"))
	double SideOffset = 35.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "随机横向抖动"))
	double SideJitter = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "随机前后抖动"))
	double DistanceJitter = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "偏移回归速度", ClampMin = "0.0"))
	double OffsetReturnSpeed = 6.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "到达容差", ClampMin = "0.0"))
	double ArrivalTolerance = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "随机种子"))
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "限制到样条范围"))
	bool bClampToSpline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|排队样条线", meta = (DisplayName = "路径前视距离", ToolTip = "角色每次沿样条线向前追踪的距离；不是到达判定点。", ClampMin = "1.0"))
	double ExitLookAheadDistance = 150.0;
};

USTRUCT(BlueprintType)
struct QUEUESPLINE_API FQueueSplineSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "槽位索引"))
	int32 SlotIndex = INDEX_NONE;

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

USTRUCT(BlueprintType)
struct QUEUESPLINE_API FQueueSplineMoveTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "成员句柄"))
	FQueueSplineMemberHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "槽位"))
	FQueueSplineSlot Slot;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "移动方向"))
	FVector MoveDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "到目标距离"))
	double DistanceToTarget = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "是否到达样条终点", ToolTip = "仅当成员到达配置的路径终点时为真。"))
	bool bReachedSlot = false;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "成员阶段"))
	EQueueSplineMemberPhase Phase = EQueueSplineMemberPhase::Queued;
};

USTRUCT(BlueprintType)
struct QUEUESPLINE_API FQueueSplineMemberState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "成员句柄"))
	FQueueSplineMemberHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "成员Actor"))
	AActor* Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "槽位索引"))
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "目标距离"))
	double TargetDistance = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "当前偏移"))
	double CurrentRightOffset = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "目标偏移"))
	double TargetRightOffset = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "是否到达样条终点", ToolTip = "仅当成员到达配置的路径终点时为真。"))
	bool bReachedSlot = false;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|排队样条线", meta = (DisplayName = "成员阶段"))
	EQueueSplineMemberPhase Phase = EQueueSplineMemberPhase::Queued;
};
