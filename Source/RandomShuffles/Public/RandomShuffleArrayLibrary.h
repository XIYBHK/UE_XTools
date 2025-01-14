/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MinIndexQueue.h"
#include "RandomShuffleArrayLibrary.generated.h"

UCLASS(meta=(BlueprintThreadSafe))
class RANDOMSHUFFLES_API URandomShuffleArrayLibrary : public UBlueprintFunctionLibrary 
{
	GENERATED_BODY()

	/** 
	 * 从数组中获取随机采样。
	 *
	 *@param	InputArray		要采样的数组。
	 *@param	Count			要采样的项目数量。最大值为目标数组的大小。
	 *@return	从目标数组中随机采样的项目数组。
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "随机采样", ArrayParm = "Result", ArrayTypeDependentParams="InputArray", ToolTip="从输入数组中获取随机采样"))
	static void Array_UnweightedRandomSample(const TArray<int32>& InputArray, int32 Count, TArray<int32>& Result);

	/** 
	 * 从数组中获取随机采样。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Count			要采样的项目数量。最大值为目标数组的大小。
	 *@param	Stream			要使用的随机流。
	 *@return	从目标数组中随机采样的项目数组。
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "流送中的随机采样", ArrayParm = "Result", ArrayTypeDependentParams="InputArray", ToolTip="从输入数组中获取随机采样，使用指定的随机流"))
	static void Array_UnweightedRandomSampleFromStream(const TArray<int32>& InputArray, int32 Count, UPARAM(Ref) FRandomStream& Stream, TArray<int32>& Result);

	/** 
	 * 从数组中获取加权随机采样。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率。必须至少与输入数组大小相同。
	 *@param	Count			要采样的项目数量。最大值为目标数组的大小。
	 *@return	从目标数组中随机采样的项目数组。
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "加权随机采样", ArrayParm = "Result", ArrayTypeDependentParams="InputArray", ToolTip="从输入数组中获取加权随机采样"))
	static void Array_RandomSample(const TArray<int32>& InputArray, const TArray<float> Weights, int32 Count, TArray<int32>& Result);

	/** 
	 * 从数组中获取流送中的加权随机采样，使用特定的随机流。 
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率。必须至少与输入数组大小相同。
	 *@param	Count			要采样的项目数量。最大值为目标数组的大小。
	 *@param	Stream			要使用的随机流。
	 *@return	从目标数组中随机采样的项目数组。
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "流送中的加权随机采样", ArrayParm = "Result", ArrayTypeDependentParams="InputArray", ToolTip="从输入数组中获取流送中的加权随机采样，使用指定的随机流"))
	static void Array_RandomSampleFromStream(const TArray<int32>& InputArray, const TArray<float> Weights, int32 Count, UPARAM(Ref) FRandomStream& Stream, TArray<int32>& Result);
	
	/** 
	 * 获取数组的加权随机打乱。 
	 *
	 *@param	InputArray		要打乱的数组。
	 *@param	Weights		    相对于其他项目选择每个项目的概率。必须至少与输入数组大小相同。
	 *@return	原始数组的打乱版本。
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "加权随机打乱", ArrayParm = "Result", ArrayTypeDependentParams="InputArray", ToolTip="获取输入数组的加权随机打乱"))
	static void Array_RandomShuffle(const TArray<int32>& InputArray, const TArray<float> Weights, TArray<int32>& Result);

	/** 
	 * 获取数组的流送中的加权随机打乱，使用特定的随机流。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率。必须至少与输入数组大小相同。
	 *@param	Stream			要使用的随机流。
	 *@return	从目标数组中随机采样的项目数组。
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "流送中的加权随机打乱", ArrayParm = "Result", ArrayTypeDependentParams="InputArray", ToolTip="获取输入数组的流送中的加权随机打乱，使用指定的随机流"))
	static void Array_RandomShuffleFromStream(const TArray<int32>& InputArray, const TArray<float> Weights, UPARAM(Ref) FRandomStream& Stream, TArray<int32>& Result);


	static void GenericArray_RandomSample(
        void* InputArray, const FArrayProperty* ArrayProp, 
        void* Weights, const FArrayProperty* WeightsProp, 
        int32 Count, FRandomStream* Stream,
        void* OutputArray, FArrayProperty* OutputProp);


public:

    DECLARE_FUNCTION(execArray_UnweightedRandomSample)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_PROPERTY(FIntProperty, Count);

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		void* Result = Stack.MostRecentPropertyAddress;
		FArrayProperty* ResultProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_RandomSample(ArrayAddr, ArrayProperty, nullptr, nullptr, Count, nullptr, Result, ResultProperty);
		P_NATIVE_END;
	}

    DECLARE_FUNCTION(execArray_UnweightedRandomSampleFromStream)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_GET_PROPERTY(FIntProperty, Count);

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		FRandomStream* RandomStream = (FRandomStream*)Stack.MostRecentPropertyAddress;

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		void* Result = Stack.MostRecentPropertyAddress;
		FArrayProperty* ResultProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_RandomSample(ArrayAddr, ArrayProperty, nullptr, nullptr, Count, RandomStream, Result, ResultProperty);
		P_NATIVE_END;
	}


    DECLARE_FUNCTION(execArray_RandomSample)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* WeightsAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* WeightsProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_GET_PROPERTY(FIntProperty, Count);

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		void* Result = Stack.MostRecentPropertyAddress;
		FArrayProperty* ResultProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_RandomSample(ArrayAddr, ArrayProperty, WeightsAddr, WeightsProperty, Count, nullptr, Result, ResultProperty);
		P_NATIVE_END;
	}

    DECLARE_FUNCTION(execArray_RandomSampleFromStream)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* WeightsAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* WeightsProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_GET_PROPERTY(FIntProperty, Count);

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		FRandomStream* RandomStream = (FRandomStream*)Stack.MostRecentPropertyAddress;

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		void* Result = Stack.MostRecentPropertyAddress;
		FArrayProperty* ResultProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_RandomSample(ArrayAddr, ArrayProperty, WeightsAddr, WeightsProperty, Count, RandomStream, Result, ResultProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_RandomShuffle)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* WeightsAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* WeightsProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!WeightsProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}


		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		void* Result = Stack.MostRecentPropertyAddress;
		FArrayProperty* ResultProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_RandomSample(ArrayAddr, ArrayProperty, WeightsAddr, WeightsProperty, -1, nullptr, Result, ResultProperty);
		P_NATIVE_END;
	}

    DECLARE_FUNCTION(execArray_RandomShuffleFromStream)
	{
		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* ArrayAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!ArrayProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FArrayProperty>(NULL);
		void* WeightsAddr = Stack.MostRecentPropertyAddress;
		FArrayProperty* WeightsProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		if (!WeightsProperty)
		{
			Stack.bArrayContextFailed = true;
			return;
		}


		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		FRandomStream* RandomStream = (FRandomStream*)Stack.MostRecentPropertyAddress;

		Stack.MostRecentProperty = nullptr;
		Stack.StepCompiledIn<FProperty>(nullptr);
		void* Result = Stack.MostRecentPropertyAddress;
		FArrayProperty* ResultProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);

		P_FINISH;
		P_NATIVE_BEGIN;
		GenericArray_RandomSample(ArrayAddr, ArrayProperty, WeightsAddr, WeightsProperty, -1, RandomStream, Result, ResultProperty);
		P_NATIVE_END;
	}
};
