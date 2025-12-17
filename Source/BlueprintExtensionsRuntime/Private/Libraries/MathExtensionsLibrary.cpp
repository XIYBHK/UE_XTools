#include "Libraries/MathExtensionsLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/UnrealType.h"
#include "UObject/Interface.h"

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region StableFrame

float UMathExtensionsLibrary::StableFrame(float DeltaTime, const TArray<float>& PastDeltaTime)
{
	const float MaxDeviation = 1.0f;
	const int32 MaxHistorySize = 5;

	if (DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return DeltaTime;
	}

	if (PastDeltaTime.Num() == 0)
	{
		return DeltaTime;
	}

	float Frame = 1.0f / DeltaTime;
	TArray<float> PastFrames;
	PastFrames.Reserve(PastDeltaTime.Num());
	for (float PastTime : PastDeltaTime)
	{
		if (PastTime > KINDA_SMALL_NUMBER)
		{
			PastFrames.Add(1.0f / PastTime);
		}
	}

	if (PastFrames.Num() == 0)
	{
		return DeltaTime;
	}

	float Sum = 0.0f;
	for (float Value : PastFrames)
	{
		Sum += Value;
	}
	float Average = Sum / PastFrames.Num();

	float ClampedFrame = FMath::Clamp(Frame, Average - MaxDeviation, Average + MaxDeviation);
	ClampedFrame = FMath::Max(ClampedFrame, 30.0f);

	TArray<float> UpdatedPastFrames = PastFrames;
	UpdatedPastFrames.Add(ClampedFrame);
	if (UpdatedPastFrames.Num() > MaxHistorySize)
	{
		UpdatedPastFrames.RemoveAt(0);
	}

	float StableDeltaTime = 1.0f / ClampedFrame;
	return StableDeltaTime;
}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region KeepDecimals

	float UMathExtensionsLibrary::KeepDecimals_Float(float Value, int32 DecimalPlaces)
	{
		const float Multiplier = FMath::Pow(10.0f, static_cast<float>(DecimalPlaces));
		return FMath::RoundToFloat(Value * Multiplier) / Multiplier;
	}

	FString UMathExtensionsLibrary::KeepDecimals_FloatString(float Value, int32 DecimalPlaces)
	{
		// 首先进行小数位数的处理
		const float RoundedValue = KeepDecimals_Float(Value, DecimalPlaces);
	    
		// 转换为字符串，强制使用指定的小数位数
		return FString::Printf(TEXT("%.*f"), DecimalPlaces, RoundedValue);
	}

	FVector2D UMathExtensionsLibrary::KeepDecimals_Vec2(FVector2D Value, int32 DecimalPlaces)
	{
		return FVector2D(
			KeepDecimals_Float(Value.X, DecimalPlaces),
			KeepDecimals_Float(Value.Y, DecimalPlaces)
		);
	}

	FVector UMathExtensionsLibrary::KeepDecimals_Vec3(FVector Value, int32 DecimalPlaces)
	{
		return FVector(
			KeepDecimals_Float(Value.X, DecimalPlaces),
			KeepDecimals_Float(Value.Y, DecimalPlaces),
			KeepDecimals_Float(Value.Z, DecimalPlaces)
		);
	}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Sort

	void UMathExtensionsLibrary::SortInsertFloat(UPARAM(ref) TArray<double>& InOutArray, double InsertElement, bool SortAsendant)
		{
			int32 Left = 0;
			int32 Right = InOutArray.Num();
	    
			while (Left < Right)
			{
				const int32 Mid = (Left + Right) / 2;
				if (SortAsendant ? InOutArray[Mid] <= InsertElement : InOutArray[Mid] >= InsertElement)
				{
					Left = Mid + 1;
				}
				else
				{
					Right = Mid;
				}
			}
	    
			InOutArray.Insert(InsertElement, Left);
		}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region Units

	void UMathExtensionsLibrary::UnitValueScale(FProperty* ValueProperty, void* ValueAddr, double Scale)
	{
		if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(ValueProperty))
		{
			double Value = DoubleProperty->GetPropertyValue(ValueAddr);
			DoubleProperty->SetPropertyValue(ValueAddr, Value * Scale);
		}
		else if (FIntProperty* IntProperty = CastField<FIntProperty>(ValueProperty))
		{
			int32 Value = IntProperty->GetPropertyValue(ValueAddr);
			IntProperty->SetPropertyValue(ValueAddr, Value * Scale);
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(ValueProperty))
		{
			if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
			{
				FVector2D* Vec2 = static_cast<FVector2D*>(ValueAddr);
				Vec2->X *= Scale;
				Vec2->Y *= Scale;
			}
			else if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				FVector* Vec = static_cast<FVector*>(ValueAddr);
				Vec->X *= Scale;
				Vec->Y *= Scale;
				Vec->Z *= Scale;
			}
		}
	}

	void UMathExtensionsLibrary::UnitValueAcosD(FProperty* ValueProperty, void* ValueAddr)
		{
			if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(ValueProperty))
			{
				double Value = DoubleProperty->GetPropertyValue(ValueAddr);
				DoubleProperty->SetPropertyValue(ValueAddr, FMath::RadiansToDegrees(FMath::Acos(Value)));
			}
			else if (FStructProperty* StructProperty = CastField<FStructProperty>(ValueProperty))
			{
				if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
				{
					FVector2D* Vec2 = static_cast<FVector2D*>(ValueAddr);
					Vec2->X = FMath::RadiansToDegrees(FMath::Acos(Vec2->X));
					Vec2->Y = FMath::RadiansToDegrees(FMath::Acos(Vec2->Y));
				}
				else if (StructProperty->Struct == TBaseStructure<FVector>::Get())
				{
					FVector* Vec = static_cast<FVector*>(ValueAddr);
					Vec->X = FMath::RadiansToDegrees(FMath::Acos(Vec->X));
					Vec->Y = FMath::RadiansToDegrees(FMath::Acos(Vec->Y));
					Vec->Z = FMath::RadiansToDegrees(FMath::Acos(Vec->Z));
				}
			}
		}

	DEFINE_FUNCTION(UMathExtensionsLibrary::execMeterToUnit)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		void* ValueAddr = Stack.MostRecentPropertyAddress;
		FProperty* ValueProperty = Stack.MostRecentProperty;

		if (!ValueProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;

		P_NATIVE_BEGIN;
		UnitValueScale(ValueProperty, ValueAddr, 100.0);
		P_NATIVE_END;
	}

	DEFINE_FUNCTION(UMathExtensionsLibrary::execMphToUnit)
		{
			Stack.MostRecentProperty = nullptr;
			Stack.StepCompiledIn<FProperty>(nullptr);
			void* ValueAddr = Stack.MostRecentPropertyAddress;
			FProperty* ValueProperty = Stack.MostRecentProperty;

			if (!ValueProperty)
			{
				Stack.bArrayContextFailed = true;
				return;
			}

			P_FINISH;

			P_NATIVE_BEGIN;
			UnitValueScale(ValueProperty, ValueAddr, 27.7777778);
			P_NATIVE_END;
		}

	DEFINE_FUNCTION(UMathExtensionsLibrary::execCosToDegree)
		{
			Stack.MostRecentProperty = nullptr;
			Stack.StepCompiledIn<FProperty>(nullptr);
			void* ValueAddr = Stack.MostRecentPropertyAddress;
			FProperty* ValueProperty = Stack.MostRecentProperty;

			if (!ValueProperty)
			{
				Stack.bArrayContextFailed = true;
				return;
			}

			P_FINISH;

			P_NATIVE_BEGIN;
			UnitValueAcosD(ValueProperty, ValueAddr);
			P_NATIVE_END;
		}

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————
