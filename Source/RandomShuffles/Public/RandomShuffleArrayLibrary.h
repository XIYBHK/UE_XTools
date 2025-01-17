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
	 * 从数组中获取随机采样，每个元素被选中的概率相等。
	 * 采用均匀分布算法，确保每个元素有相同的被选中概率。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Count			要采样的项目数量，可以大于数组长度（有放回采样）
	 *@return	从目标数组中随机采样的项目数组
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "随机采样", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="从数组中随机选择指定数量的元素，每个元素被选中的概率相等。\n使用说明：输入源数组和需要的采样数量，函数会返回一个新数组，包含随机选择的元素"))
	static void Array_UnweightedRandomSample(const TArray<int32>& InputArray, int32 Count, TArray<int32>& Result);

	/** 
	 * 使用指定的随机流从数组中获取随机采样，每个元素被选中的概率相等。
	 * 采用均匀分布算法，确保每个元素有相同的被选中概率。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Count			要采样的项目数量，可以大于数组长度（有放回采样）
	 *@param	Stream			要使用的随机流，用于控制随机数生成
	 *@return	从目标数组中随机采样的项目数组
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "流送中的随机采样", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="使用指定的随机流从数组中随机选择指定数量的元素，每个元素被选中的概率相等。\n使用说明：输入源数组、采样数量和随机流，可以通过相同的随机流获得相同的随机结果"))
	static void Array_UnweightedRandomSampleFromStream(const TArray<int32>& InputArray, int32 Count, UPARAM(Ref) FRandomStream& Stream, TArray<int32>& Result);

	/** 
	 * 从数组中获取加权随机采样，每个元素被选中的概率与其权重成正比。
	 * 采用A-ES算法实现，支持浮点数权重。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率权重，权重为0的元素不会被选中
	 *@param	Count			要采样的项目数量，可以大于数组长度（有放回采样）
	 *@return	从目标数组中随机采样的项目数组
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "加权随机采样", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="从数组中随机选择指定数量的元素，每个元素被选中的概率与其权重成正比。\n使用说明：输入源数组、权重数组和采样数量，权重数组的长度必须与源数组相同，权重为0的元素不会被选中"))
	static void Array_RandomSample(const TArray<int32>& InputArray, const TArray<float> Weights, int32 Count, TArray<int32>& Result);

	/** 
	 * 使用指定的随机流从数组中获取加权随机采样，每个元素被选中的概率与其权重成正比。
	 * 采用A-ES算法实现，支持浮点数权重。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率权重，权重为0的元素不会被选中
	 *@param	Count			要采样的项目数量，可以大于数组长度（有放回采样）
	 *@param	Stream			要使用的随机流，用于控制随机数生成
	 *@return	从目标数组中随机采样的项目数组
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "流送中的加权随机采样", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="使用指定的随机流从数组中随机选择指定数量的元素，每个元素被选中的概率与其权重成正比。\n使用说明：输入源数组、权重数组、采样数量和随机流，可以通过相同的随机流获得相同的随机结果"))
	static void Array_RandomSampleFromStream(const TArray<int32>& InputArray, const TArray<float> Weights, int32 Count, UPARAM(Ref) FRandomStream& Stream, TArray<int32>& Result);
	
	/** 
	 * 获取数组的加权随机打乱版本，每个位置的元素选择概率与其权重成正比。
	 * 采用A-ES算法实现，支持浮点数权重。
	 *
	 *@param	InputArray		要打乱的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率权重，权重为0的元素不会被选中
	 *@return	原始数组的打乱版本
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "加权随机打乱", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="将数组随机打乱，每个位置的元素选择概率与其权重成正比。\n使用说明：输入源数组和权重数组，函数会返回一个新的打乱数组，权重为0的元素会被排除"))
	static void Array_RandomShuffle(const TArray<int32>& InputArray, const TArray<float> Weights, TArray<int32>& Result);

	/** 
	 * 使用指定的随机流获取数组的加权随机打乱版本，每个位置的元素选择概率与其权重成正比。
	 * 采用A-ES算法实现，支持浮点数权重。
	 *
	 *@param	InputArray		要打乱的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率权重，权重为0的元素不会被选中
	 *@param	Stream			要使用的随机流，用于控制随机数生成
	 *@return	原始数组的打乱版本
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "流送中的加权随机打乱", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="使用指定的随机流将数组随机打乱，每个位置的元素选择概率与其权重成正比。\n使用说明：输入源数组、权重数组和随机流，可以通过相同的随机流获得相同的打乱结果"))
	static void Array_RandomShuffleFromStream(const TArray<int32>& InputArray, const TArray<float> Weights, UPARAM(Ref) FRandomStream& Stream, TArray<int32>& Result);

	/** 
	 * 从数组中获取严格权重比例的随机采样，确保采样结果严格符合权重比例。
	 * 采用权重池算法实现，特别适合需要精确权重比例的场景。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率权重，权重为0的元素不会被选中
	 *@param	Count			要采样的项目数量，可以大于数组长度（有放回采样）
	 *@return	从目标数组中随机采样的项目数组
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "严格权重随机采样", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="从数组中随机选择指定数量的元素，严格保证选中次数与权重比例一致。\n使用说明：输入源数组、权重数组和采样数量，采样结果会严格按照权重比例分配，适合需要精确控制概率的场景"))
	static void Array_StrictWeightRandomSample(const TArray<int32>& InputArray, const TArray<float> Weights, int32 Count, TArray<int32>& Result);

	/** 
	 * 使用指定的随机流从数组中获取严格权重比例的随机采样，确保采样结果严格符合权重比例。
	 * 采用权重池算法实现，特别适合需要精确权重比例的场景。
	 *
	 *@param	InputArray		要采样的数组
	 *@param	Weights		    相对于其他项目选择每个项目的概率权重，权重为0的元素不会被选中
	 *@param	Count			要采样的项目数量，可以大于数组长度（有放回采样）
	 *@param	Stream			要使用的随机流，用于控制随机数生成
	 *@return	从目标数组中随机采样的项目数组
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, meta=(DisplayName = "流送中的严格权重随机采样", 
		ArrayParm = "Result", 
		ArrayTypeDependentParams="InputArray", 
		Category="XTools|随机", 
		ToolTip="使用指定的随机流从数组中随机选择指定数量的元素，严格保证选中次数与权重比例一致。\n使用说明：输入源数组、权重数组、采样数量和随机流，可以通过相同的随机流获得相同的采样结果"))
	static void Array_StrictWeightRandomSampleFromStream(const TArray<int32>& InputArray, const TArray<float> Weights, int32 Count, UPARAM(Ref) FRandomStream& Stream, TArray<int32>& Result);

	/** 
	 * 使用PRD(伪随机分布)算法生成布尔值，可以让随机事件的发生更加均衡。
	 * PRD算法会根据连续失败次数动态调整概率，避免运气过好或过差的情况。
	 * 采用DOTA2的标准PRD算法实现，提供更公平的随机体验。
	 * 算法来源：https://gaming.stackexchange.com/questions/161430/calculating-the-constant-c-in-dota-2-pseudo-random-distribution
	 *
	 *@param	BaseChance		基础触发概率[0,1]，实际触发概率会根据连续失败次数动态调整
	 *@param	CurrentCompensation	当前概率补偿计数，记录连续失败的次数，首次调用传入1
	 *@param	OutNextCompensation	输出下一次调用需要传入的概率补偿计数
	 *@param	OutActualChance	输出实际概率
	 *@return	是否触发
	*/
	UFUNCTION(BlueprintCallable, Category="XTools|随机", meta=(
		DisplayName = "伪随机布尔",
		BaseChance="基础概率",
		CurrentCompensation="当前概率补偿",
		OutNextCompensation="下次概率补偿",
		OutActualChance="实际概率",
		ToolTip="使用DOTA2的PRD算法生成布尔值，可以让随机事件的发生更加均衡。\n使用说明：首次调用时当前概率补偿传入1，成功后会重置为1，失败后会+1。基础概率为期望的触发概率[0-1]"))
	static bool PseudoRandomBool(
		UPARAM(DisplayName="基础概率") float BaseChance, 
		UPARAM(DisplayName="当前概率补偿") float CurrentCompensation, 
		UPARAM(DisplayName="下次概率补偿") float& OutNextCompensation,
		UPARAM(DisplayName="实际概率") float& OutActualChance);

	/** 
	 * 使用指定的随机流和PRD(伪随机分布)算法生成布尔值，可以让随机事件的发生更加均衡。
	 * PRD算法会根据连续失败次数动态调整概率，避免运气过好或过差的情况。
	 * 采用DOTA2的标准PRD算法实现，提供更公平的随机体验。
	 * 算法来源：https://gaming.stackexchange.com/questions/161430/calculating-the-constant-c-in-dota-2-pseudo-random-distribution
	 *
	 *@param	BaseChance		基础触发概率[0,1]，实际触发概率会根据连续失败次数动态调整
	 *@param	CurrentCompensation	当前概率补偿计数，记录连续失败的次数，首次调用传入1
	 *@param	Stream			要使用的随机流，用于控制随机数生成
	 *@param	OutNextCompensation	输出下一次调用需要传入的概率补偿计数
	 *@param	OutActualChance	输出实际概率
	 *@return	是否触发
	*/
	UFUNCTION(BlueprintCallable, Category="XTools|随机", meta=(
		DisplayName = "流送中的伪随机布尔",
		BaseChance="基础概率",
		CurrentCompensation="当前概率补偿",
		Stream="随机流",
		OutNextCompensation="下次概率补偿",
		OutActualChance="实际概率",
		ToolTip="使用DOTA2的PRD算法和指定的随机流生成布尔值，可以让随机事件的发生更加均衡。\n使用说明：首次调用时当前概率补偿传入1，成功后会重置为1，失败后会+1。使用随机流可以重现相同的随机序列"))
	static bool PseudoRandomBoolFromStream(
		UPARAM(DisplayName="基础概率") float BaseChance, 
		UPARAM(DisplayName="当前概率补偿") float CurrentCompensation, 
		UPARAM(Ref, DisplayName="随机流") FRandomStream& Stream, 
		UPARAM(DisplayName="下次概率补偿") float& OutNextCompensation,
		UPARAM(DisplayName="实际概率") float& OutActualChance);

	static void GenericArray_RandomSample(
        void* InputArray, const FArrayProperty* ArrayProp, 
        void* Weights, const FArrayProperty* WeightsProp, 
        int32 Count, FRandomStream* Stream,
        void* OutputArray, FArrayProperty* OutputProp);

	static void GenericArray_StrictWeightRandomSample(
        void* TargetArray, const FArrayProperty* ArrayProp, 
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

	DECLARE_FUNCTION(execArray_StrictWeightRandomSample)
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
		GenericArray_StrictWeightRandomSample(ArrayAddr, ArrayProperty, WeightsAddr, WeightsProperty, Count, nullptr, Result, ResultProperty);
		P_NATIVE_END;
	}

	DECLARE_FUNCTION(execArray_StrictWeightRandomSampleFromStream)
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
		GenericArray_StrictWeightRandomSample(ArrayAddr, ArrayProperty, WeightsAddr, WeightsProperty, Count, RandomStream, Result, ResultProperty);
		P_NATIVE_END;
	}
};
