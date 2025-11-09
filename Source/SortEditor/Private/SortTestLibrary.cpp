#include "SortTestLibrary.h"
#include "SortLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "Internationalization/Text.h"
#include "Components/SceneComponent.h"
#include "SortEditorModule.h"
//  移除STL包含，使用UE内置类型检查

//~ 辅助函数 - 日志格式化 (命名空间内)
// =================================================================================================

namespace SortTest_Private
{
	/** 将普通数组转换为格式化的字符串，用于日志记录。 */
	template<typename T>
	FString ArrayToString(const TArray<T>& Array, int32 MaxElementsToShow)
	{
		if (Array.IsEmpty()) return TEXT("[]");
		
		FString Result = TEXT("[");
		for (int32 i = 0; i < FMath::Min(Array.Num(), MaxElementsToShow); ++i)
		{
			if (i > 0) Result += TEXT(", ");
			
			if constexpr (std::is_same_v<T, float>)
			{
				Result += FString::Printf(TEXT("%.2f"), Array[i]);
			}
			else if constexpr (std::is_same_v<T, int32>)
			{
				Result += FString::FromInt(Array[i]);
			}
			else if constexpr (std::is_same_v<T, FVector>)
			{
				 Result += Array[i].ToString();
			}
			else if constexpr (std::is_same_v<T, FString>)
			{
				Result += FString::Printf(TEXT("\"%s\""), *Array[i]);
			}
			else
			{
				Result += Array[i].ToString();
			}
		}

		if (Array.Num() > MaxElementsToShow) Result += FString::Printf(TEXT(", ... (总共 %d 个)"), Array.Num());
		
		Result += TEXT("]");
		return Result;
	}

	/** 将Actor数组转换为字符串，并附带上用于排序的数值，使日志更直观。 */
	FString ActorArrayToStringWithValues(const TArray<AActor*>& Array, const TArray<float>& Values, const FString& ValueLabel, int32 MaxElementsToShow)
	{
		if (Array.IsEmpty()) return TEXT("[]");

		FString Result = TEXT("[");
		for (int32 i = 0; i < FMath::Min(Array.Num(), MaxElementsToShow); ++i)
		{
			if (i > 0) Result += TEXT(", ");
			
			if (IsValid(Array[i]) && Values.IsValidIndex(i))
			{
				Result += FString::Printf(TEXT("'%s' at {%s} (%s: %.2f)"), 
					*Array[i]->GetActorLabel(), 
					*Array[i]->GetActorLocation().ToString(),
					*ValueLabel,
					Values[i]
				);
			}
			else
			{
				Result += TEXT("无效的Actor或数据");
			}
		}
		
		if (Array.Num() > MaxElementsToShow) Result += FString::Printf(TEXT(", ... (总共 %d 个)"), Array.Num());
		
		Result += TEXT("]");
		return Result;
	}

	/** 将Vector数组转换为字符串，并附带上用于排序的数值。 */
	FString VectorArrayToStringWithValues(const TArray<FVector>& Array, const TArray<float>& Values, const FString& ValueLabel, int32 MaxElementsToShow)
	{
		if (Array.IsEmpty()) return TEXT("[]");

		FString Result = TEXT("[");
		for (int32 i = 0; i < FMath::Min(Array.Num(), MaxElementsToShow); ++i)
		{
			if (i > 0) Result += TEXT(", ");
			
			if (Values.IsValidIndex(i))
			{
				Result += FString::Printf(TEXT("{%s} (%s: %.2f)"), 
					*Array[i].ToString(),
					*ValueLabel,
					Values[i]
				);
			}
		}
		
		if (Array.Num() > MaxElementsToShow) Result += FString::Printf(TEXT(", ... (总共 %d 个)"), Array.Num());
		
		Result += TEXT("]");
		return Result;
	}

	/** 打印最终的日志报告。 */
	void PrintFinalReport(const FString& TestName, bool bSuccess, double DurationMs, const FString& OriginalArrayStr, const FString& SortedArrayStr)
	{
		const FString SuccessString = bSuccess ? TEXT("成功") : TEXT("失败");
		const FString LogMessage = FString::Printf(
			TEXT("\n\n===== 排序算法测试报告 =====\n")
			TEXT("测试名称: %s\n")
			TEXT("测试结果: %s\n")
			TEXT("排序耗时: %.4f ms\n")
			TEXT("原始数组: %s\n")
			TEXT("排序后数组: %s\n")
			TEXT("============================\n"),
			*TestName, *SuccessString, DurationMs, *OriginalArrayStr, *SortedArrayStr
		);

		UE_LOG(LogSortEditor, Log, TEXT("%s"), *LogMessage);
	}
}

//~ 辅助函数 - 测试执行器
// =================================================================================================
template<typename T>
void USortTestLibrary::RunBasicTypeTest(const FString& TestName, TFunctionRef<TArray<T>()> DataGenerationLambda, TFunctionRef<void(const TArray<T>&, TArray<T>&, TArray<int32>&)> SortFunctionLambda)
{
	TArray<T> OriginalArray = DataGenerationLambda();
	const FString OriginalArrayStr = SortTest_Private::ArrayToString(OriginalArray, 15);
	
	TArray<T> SortedArray;
	TArray<int32> Indices;
	
	const double StartTime = FPlatformTime::Seconds();
	SortFunctionLambda(OriginalArray, SortedArray, Indices);
	const double EndTime = FPlatformTime::Seconds();

	const bool bSuccess = VerifySortOrder(SortedArray, TestName.Contains(TEXT("升序")));
	const FString SortedArrayStr = SortTest_Private::ArrayToString(SortedArray, 15);

	SortTest_Private::PrintFinalReport(TestName, bSuccess, (EndTime - StartTime) * 1000.0, OriginalArrayStr, SortedArrayStr);
}

void USortTestLibrary::RunActorSortTest(const FString& TestName, const FString& ValueLabel, UObject* WorldContextObject, int32 ArraySize, const FVector& SpawnCenter, float SpawnRadius,
    TFunctionRef<void(const TArray<AActor*>&, TArray<AActor*>&, TArray<int32>&, TArray<float>&)> SortFunctionLambda)
{
    TArray<AActor*> CreatedActors;
    TArray<AActor*> OriginalActors = GenerateTestActors(WorldContextObject, ArraySize, SpawnCenter, SpawnRadius, CreatedActors);

    if (OriginalActors.IsEmpty())
    {
        UE_LOG(LogSortEditor, Error, TEXT("测试 [%s] 失败：无法生成用于测试的Actor。"), *TestName);
        return;
    }
    
    const FString OriginalArrayStr = ActorArrayToString(OriginalActors, 15);
    
    TArray<AActor*> SortedActors;
    TArray<int32> Indices;
    TArray<float> SortedValues;

    const double StartTime = FPlatformTime::Seconds();
    SortFunctionLambda(OriginalActors, SortedActors, Indices, SortedValues);
    const double EndTime = FPlatformTime::Seconds();

    const bool bSuccess = VerifySortOrder(SortedValues, TestName.Contains(TEXT("升序")));
    const FString SortedArrayStr = SortTest_Private::ActorArrayToStringWithValues(SortedActors, SortedValues, ValueLabel, 15);

    SortTest_Private::PrintFinalReport(TestName, bSuccess, (EndTime - StartTime) * 1000.0, OriginalArrayStr, SortedArrayStr);

    for (AActor* Actor : CreatedActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
}

void USortTestLibrary::RunVectorSortTest(const FString& TestName, const FString& ValueLabel, int32 ArraySize, float SpawnRadius,
    TFunctionRef<void(const TArray<FVector>&, TArray<FVector>&, TArray<int32>&, TArray<float>&)> SortFunctionLambda)
{
    TArray<FVector> OriginalArray = GenerateRandomVectorArray(ArraySize, -SpawnRadius, SpawnRadius);
    const FString OriginalArrayStr = SortTest_Private::ArrayToString(OriginalArray, 15);

    TArray<FVector> SortedArray;
    TArray<int32> Indices;
    TArray<float> SortedValues;

    const double StartTime = FPlatformTime::Seconds();
    SortFunctionLambda(OriginalArray, SortedArray, Indices, SortedValues);
    const double EndTime = FPlatformTime::Seconds();

    const bool bSuccess = VerifySortOrder(SortedValues, TestName.Contains(TEXT("升序")));
    const FString SortedArrayStr = SortTest_Private::VectorArrayToStringWithValues(SortedArray, SortedValues, ValueLabel, 15);

    SortTest_Private::PrintFinalReport(TestName, bSuccess, (EndTime - StartTime) * 1000.0, OriginalArrayStr, SortedArrayStr);
}


//~ 主要测试函数
// =================================================================================================

void USortTestLibrary::ExecuteSortTest(UObject* WorldContextObject, ESortTestType SortType, bool bSortAscending, int32 ArraySize, FVector SpawnCenter, float SpawnRadius)
{
	const UEnum* EnumPtr = StaticEnum<ESortTestType>();
	const FString SortTypeName = EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(SortType)).ToString();
    const FString DirectionStr = bSortAscending ? TEXT("升序") : TEXT("降序");
    const FString TestName = FString::Printf(TEXT("%s - %s"), *SortTypeName, *DirectionStr);
	
	switch(SortType)
	{
		case ESortTestType::Integer:
			RunBasicTypeTest<int32>(TestName,
				[&](){ return GenerateRandomIntArray(ArraySize, -100, 100); },
				[&](const auto& In, auto& Out, auto& Idx){ USortLibrary::SortIntegerArray(In, bSortAscending, Out, Idx); }
			);
			break;

		case ESortTestType::Float:
			RunBasicTypeTest<float>(TestName,
				[&](){ return GenerateRandomFloatArray(ArraySize, -100.0f, 100.0f); },
				[&](const auto& In, auto& Out, auto& Idx){ USortLibrary::SortFloatArray(In, bSortAscending, Out, Idx); }
			);
			break;

		case ESortTestType::String:
			RunBasicTypeTest<FString>(TestName,
				[&]()
				{
					auto Array = GenerateRandomStringArray(ArraySize, 3, 8);
					if (Array.Num() >= 4)
					{
						Array[0] = TEXT("艾克"); Array[1] = TEXT("卡特琳娜"); Array[2] = TEXT("吉格斯"); Array[3] = TEXT("布隆");
					}
					return Array;
				},
				[&](const auto& In, auto& Out, auto& Idx){ USortLibrary::SortStringArray(In, bSortAscending, Out, Idx); }
			);
			break;
        
		case ESortTestType::Name:
			RunBasicTypeTest<FName>(TestName,
				[]()
				{
					return TArray<FName>{
						FName(TEXT("Banana")), FName(TEXT("Apple")), FName(TEXT("Pear")), FName(TEXT("Orange")),
						FName(TEXT("张三")), FName(TEXT("李四")), FName(TEXT("王五")), FName(TEXT("赵六")), FName(TEXT("孙悟空"))
					};
				},
				[&](const auto& In, auto& Out, auto& Idx){ USortLibrary::SortNameArray(In, bSortAscending, Out, Idx); }
			);
			break;
        
		case ESortTestType::Actor_ByDistance:
			RunActorSortTest(TestName, TEXT("距离"), WorldContextObject, ArraySize, SpawnCenter, SpawnRadius,
				[&](const auto& In, auto& Out, auto& Idx, auto& Vals){ USortLibrary::SortActorsByDistance(In, SpawnCenter, bSortAscending, false, Out, Idx, Vals); }
			);
			break;

		case ESortTestType::Actor_ByHeight:
			{
				// 创建一个包装器Lambda来适配SortActorsByHeight的签名
				auto SortWrapper = [&](const TArray<AActor*>& In, TArray<AActor*>& Out, TArray<int32>& Idx, TArray<float>& Vals)
				{
					USortLibrary::SortActorsByHeight(In, bSortAscending, Out, Idx);
					// 手动填充用于验证和日志记录的高度值
					Vals.Reset(Out.Num());
					for (const AActor* Actor : Out)
					{
						if (Actor)
						{
							Vals.Add(Actor->GetActorLocation().Z);
						}
					}
				};

				RunActorSortTest(TestName, TEXT("高度"), WorldContextObject, ArraySize, SpawnCenter, SpawnRadius, SortWrapper);
			}
			break;

		case ESortTestType::Actor_ByAxisX:
			RunActorSortTest(TestName, TEXT("X坐标"), WorldContextObject, ArraySize, SpawnCenter, SpawnRadius,
				[&](const auto& In, auto& Out, auto& Idx, auto& Vals){ USortLibrary::SortActorsByAxis(In, ECoordinateAxis::X, bSortAscending, Out, Idx, Vals); }
			);
			break;
		
		case ESortTestType::Actor_ByAngle:
			RunActorSortTest(TestName, TEXT("夹角"), WorldContextObject, ArraySize, SpawnCenter, SpawnRadius,
				[&](const auto& In, auto& Out, auto& Idx, auto& Vals){ USortLibrary::SortActorsByAngle(In, SpawnCenter, FVector::ForwardVector, bSortAscending, true, Out, Idx, Vals); }
			);
			break;

		case ESortTestType::Actor_ByAzimuth:
			RunActorSortTest(TestName, TEXT("方位角"), WorldContextObject, ArraySize, SpawnCenter, SpawnRadius,
				[&](const auto& In, auto& Out, auto& Idx, auto& Vals){ USortLibrary::SortActorsByAzimuth(In, SpawnCenter, bSortAscending, Out, Idx, Vals); }
			);
			break;

		case ESortTestType::Vector_ByLength:
			RunVectorSortTest(TestName, TEXT("长度"), ArraySize, SpawnRadius,
				[&](const auto& In, auto& Out, auto& Idx, auto& Vals){ USortLibrary::SortVectorsByLength(In, bSortAscending, Out, Idx, Vals); }
			);
			break;
		
		case ESortTestType::Vector_ByProjection:
			RunVectorSortTest(TestName, TEXT("投影"), ArraySize, SpawnRadius,
				[&](const auto& In, auto& Out, auto& Idx, auto& Vals){ USortLibrary::SortVectorsByProjection(In, FVector(1, 1, 0).GetSafeNormal(), bSortAscending, Out, Idx, Vals); }
			);
			break;

		case ESortTestType::Vector_ByAxisY:
			RunVectorSortTest(TestName, TEXT("Y坐标"), ArraySize, SpawnRadius,
				[&](const auto& In, auto& Out, auto& Idx, auto& Vals){ USortLibrary::SortVectorsByAxis(In, ECoordinateAxis::Y, bSortAscending, Out, Idx, Vals); }
			);
			break;

		default:
			UE_LOG(LogSortEditor, Warning, TEXT("未知的排序测试类型: %s"), *TestName);
			return;
	}
}

//~ 数据生成与验证的实现
// =================================================================================================

TArray<int32> USortTestLibrary::GenerateRandomIntArray(int32 Size, int32 Min, int32 Max)
{
    TArray<int32> Result;
    if (Size <= 0) return Result;
    Result.Reserve(Size);
    for (int32 i = 0; i < Size; ++i)
    {
        Result.Add(FMath::RandRange(Min, Max));
    }
    return Result;
}

TArray<float> USortTestLibrary::GenerateRandomFloatArray(int32 Size, float Min, float Max)
{
    TArray<float> Result;
    if (Size <= 0) return Result;
    Result.Reserve(Size);
    for (int32 i = 0; i < Size; ++i)
    {
        Result.Add(FMath::FRandRange(Min, Max));
    }
    return Result;
}

TArray<FString> USortTestLibrary::GenerateRandomStringArray(int32 Size, int32 MinLen, int32 MaxLen)
{
    TArray<FString> Result;
    if (Size <= 0) return Result;
    Result.Reserve(Size);
    const FString Chars = TEXT("abcdefghijklmnopqrstuvwxyz");
    for (int32 i = 0; i < Size; ++i)
    {
        int32 Length = FMath::RandRange(MinLen, MaxLen);
        FString RandomString;
        for (int32 j = 0; j < Length; ++j)
        {
            RandomString += Chars[FMath::RandRange(0, Chars.Len() - 1)];
        }
        Result.Add(RandomString);
    }
    return Result;
}

TArray<FVector> USortTestLibrary::GenerateRandomVectorArray(int32 Size, float MinCoord, float MaxCoord)
{
    TArray<FVector> Result;
    if (Size <= 0) return Result;
    Result.Reserve(Size);
    for (int32 i = 0; i < Size; ++i)
    {
        Result.Add(FVector(
            FMath::FRandRange(MinCoord, MaxCoord),
            FMath::FRandRange(MinCoord, MaxCoord),
            FMath::FRandRange(MinCoord, MaxCoord)
        ));
    }
    return Result;
}

TArray<AActor*> USortTestLibrary::GenerateTestActors(UObject* WorldContextObject, int32 Count, const FVector& Center, float Radius, TArray<AActor*>& OutCreatedActors)
{
    OutCreatedActors.Empty();
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World || Count <= 0) return {};

    OutCreatedActors.Reserve(Count);
    for (int32 i = 0; i < Count; ++i)
    {
        const FVector RandomLocation = Center + FMath::VRand() * FMath::FRandRange(0.0f, Radius);
        
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
        
        if (NewActor)
        {
            USceneComponent* RootComp = NewObject<USceneComponent>(NewActor, TEXT("RootComponent"));
            if (RootComp)
            {
                RootComp->RegisterComponent();
                NewActor->SetRootComponent(RootComp);
                NewActor->SetActorLocation(RandomLocation);
            }
            
            NewActor->SetActorLabel(FString::Printf(TEXT("测试Actor_%d"), i));
            OutCreatedActors.Add(NewActor);
        }
    }
    return OutCreatedActors;
}

bool USortTestLibrary::VerifySortOrder(const TArray<int32>& Array, bool bAscending)
{
    for (int32 i = 0; i < Array.Num() - 1; ++i)
    {
        if (bAscending ? (Array[i] > Array[i+1]) : (Array[i] < Array[i+1]))
        {
            return false;
        }
    }
    return true;
}

bool USortTestLibrary::VerifySortOrder(const TArray<float>& Array, bool bAscending)
{
    for (int32 i = 0; i < Array.Num() - 1; ++i)
    {
        if (FMath::IsNaN(Array[i]) || FMath::IsNaN(Array[i+1])) continue;
        if (bAscending ? (Array[i] > Array[i+1] + KINDA_SMALL_NUMBER) : (Array[i] < Array[i+1] - KINDA_SMALL_NUMBER))
        {
            return false;
        }
    }
    return true;
}

bool USortTestLibrary::VerifySortOrder(const TArray<FString>& Array, bool bAscending)
{
    for (int32 i = 0; i < Array.Num() - 1; ++i)
    {
        int32 CompareResult = FText::FromString(Array[i]).CompareTo(FText::FromString(Array[i+1]));
        if (bAscending ? (CompareResult > 0) : (CompareResult < 0))
        {
            return false;
        }
    }
    return true;
}

bool USortTestLibrary::VerifySortOrder(const TArray<FName>& Array, bool bAscending)
{
	for (int32 i = 0; i < Array.Num() - 1; ++i)
    {
        int32 CompareResult = FText::FromName(Array[i]).CompareTo(FText::FromName(Array[i+1]));
        if (bAscending ? (CompareResult > 0) : (CompareResult < 0))
        {
            return false;
        }
    }
    return true;
}

FString USortTestLibrary::ActorArrayToString(const TArray<AActor*>& Array, int32 MaxElementsToShow)
{
	if (Array.IsEmpty()) return TEXT("[]");

    FString Result = TEXT("[");
    for (int32 i = 0; i < FMath::Min(Array.Num(), MaxElementsToShow); ++i)
    {
        if (i > 0) Result += TEXT(", ");
        
        if (IsValid(Array[i]))
        {
            Result += FString::Printf(TEXT("'%s' at {%s}"), *Array[i]->GetActorLabel(), *Array[i]->GetActorLocation().ToString());
        }
        else
        {
            Result += TEXT("无效的Actor");
        }
    }
    
    if (Array.Num() > MaxElementsToShow) Result += FString::Printf(TEXT(", ... (总共 %d 个)"), Array.Num());
    
    Result += TEXT("]");
    return Result;
}
