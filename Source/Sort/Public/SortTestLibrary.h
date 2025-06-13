#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SortLibrary.h" // 需要 ECoordinateAxis
#include "SortTestLibrary.generated.h"

UENUM(BlueprintType)
enum class ESortTestType : uint8
{
	Integer				UMETA(DisplayName = "整数"),
	Float				UMETA(DisplayName = "浮点数"),
	String				UMETA(DisplayName = "字符串"),
	Name				UMETA(DisplayName = "命名"),
	Actor_ByDistance	UMETA(DisplayName = "Actor按距离"),
	Actor_ByHeight		UMETA(DisplayName = "Actor按高度"),
	Actor_ByAxisX		UMETA(DisplayName = "Actor按X轴"),
	Actor_ByAngle		UMETA(DisplayName = "Actor按夹角"),
	Actor_ByAzimuth		UMETA(DisplayName = "Actor按方位角"),
	Vector_ByLength		UMETA(DisplayName = "向量按长度"),
	Vector_ByProjection UMETA(DisplayName = "向量按投影"),
	Vector_ByAxisY		UMETA(DisplayName = "向量按Y轴"),
};

UCLASS(meta=(DisplayName="排序测试工具 (优化版)"))
class SORT_API USortTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "XTools|排序测试", meta=(
		DisplayName="执行排序算法测试",
		WorldContext="WorldContextObject",
		Keywords="测试, 排序, 算法, 验证, 日志",
		ArraySize="20",
		bSortAscending="true",
		ToolTip = "执行一个完整的排序算法测试流程：生成数据 -> 排序 -> 验证 -> 输出日志报告。"
	))
	static void ExecuteSortTest(
		UObject* WorldContextObject,
		ESortTestType SortType,
		bool bSortAscending,
		int32 ArraySize,
		FVector SpawnCenter,
		float SpawnRadius
	);

private:
	//~ 辅助函数 - 测试执行器
	// =================================================================================================
	
	template<typename T>
	static void RunBasicTypeTest(const FString& TestName, TFunctionRef<TArray<T>()> DataGenerationLambda, TFunctionRef<void(const TArray<T>&, TArray<T>&, TArray<int32>&)> SortFunctionLambda);

	static void RunActorSortTest(const FString& TestName, const FString& ValueLabel, UObject* WorldContextObject, int32 ArraySize, const FVector& SpawnCenter, float SpawnRadius,
		TFunctionRef<void(const TArray<AActor*>&, TArray<AActor*>&, TArray<int32>&, TArray<float>&)> SortFunctionLambda);

	static void RunVectorSortTest(const FString& TestName, const FString& ValueLabel, int32 ArraySize, float SpawnRadius,
		TFunctionRef<void(const TArray<FVector>&, TArray<FVector>&, TArray<int32>&, TArray<float>&)> SortFunctionLambda);

	//~ 辅助函数 - 数据生成与验证
	// =================================================================================================

	static TArray<int32> GenerateRandomIntArray(int32 Size, int32 Min, int32 Max);
	static TArray<float> GenerateRandomFloatArray(int32 Size, float Min, float Max);
	static TArray<FString> GenerateRandomStringArray(int32 Size, int32 MinLen, int32 MaxLen);
	static TArray<FVector> GenerateRandomVectorArray(int32 Size, float MinCoord, float MaxCoord);
	static TArray<AActor*> GenerateTestActors(UObject* WorldContextObject, int32 Count, const FVector& Center, float Radius, TArray<AActor*>& OutCreatedActors);

	static bool VerifySortOrder(const TArray<int32>& Array, bool bAscending);
	static bool VerifySortOrder(const TArray<float>& Array, bool bAscending);
	static bool VerifySortOrder(const TArray<FString>& Array, bool bAscending);
	static bool VerifySortOrder(const TArray<FName>& Array, bool bAscending);
	
	static FString ActorArrayToString(const TArray<AActor*>& Array, int32 MaxElementsToShow = 15);
};
