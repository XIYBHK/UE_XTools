/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#include "RandomShuffleArrayLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "RandomSample.h"
#include "WeightPoolSample.h"
#include "RandomShuffleLog.h"
#include <functional>
#include <iterator> 
#include <cstddef> 

namespace RandomShuffles {

    namespace detail {
        
        template <typename T>
        struct from_pointer_t {
            static T apply(T* pointer) {
                return *pointer;
            }
        };

        template <typename T>
        struct from_pointer_t<T*> {
            static T* apply(T* pointer) {
                return pointer;
            }
        };

        template <typename T, typename U>
        auto from_pointer(U* pointer) {
            return from_pointer_t<T>::apply(pointer);
        }
    }

    float GetRand(FRandomStream* Stream, float Min=0.f, float Max=1.f) {
        if (Stream) {
            return Stream->FRandRange(Min, Max);
        }
        else {
            return FMath::FRandRange(Min, Max);
        }
    }
    struct FRand {
        FRandomStream* Stream;

        float operator()(float Min, float Max) {
            return RandomShuffles::GetRand(Stream, Min, Max);
        }
    };

    template <typename T>
    class ScriptArrayInputIterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type  = std::ptrdiff_t;
        using value_type = T; //uint8*;
        using pointer = value_type*;  
        using reference = value_type&; 

        ScriptArrayInputIterator(void* TargetArray, const FArrayProperty* ArrayProp, int Index = 0) : 
            Index(Index), ArrayHelper(ArrayProp, TargetArray)
        {
            TakeValue();
        }

        reference operator*() { 
            return Value;
        }
        pointer operator->() { 
            return &Value;
        }

        ScriptArrayInputIterator& operator+=(difference_type diff) { 
            Index += diff;
            TakeValue();
            return *this; 
        }  

        ScriptArrayInputIterator operator+(difference_type diff) { 
            ScriptArrayInputIterator tmp = *this;
            tmp += diff;
            return tmp;
        }  

        ScriptArrayInputIterator& operator++() { 
            Index++; 
            TakeValue();
            return *this; 
        }  

        ScriptArrayInputIterator operator++(int) { 
            ScriptArrayInputIterator tmp = *this; 
            ++(*this); 
            return tmp; 
        }

        friend bool operator== (const ScriptArrayInputIterator& a, const ScriptArrayInputIterator& b) {
             return a.Index == b.Index;
        };

        friend bool operator!= (const ScriptArrayInputIterator& a, const ScriptArrayInputIterator& b) { 
             return a.Index != b.Index;
        }; 
    private:
        void TakeValue() {
            if (Index < ArrayHelper.Num()) {
                auto RawPtr = ArrayHelper.GetRawPtr(Index);
                auto TypedPtr = reinterpret_cast<std::remove_pointer_t<value_type>*>(RawPtr);
                Value = detail::from_pointer<value_type>(TypedPtr);
            }
        }

        int Index;
        value_type Value;
        FScriptArrayHelper ArrayHelper;
    };

    class ScriptArrayOutputIterator {
    public:
        struct array_elem_ref {
            FProperty* InnerProp;
            uint8* RawPtr;

            inline array_elem_ref& operator=(uint8* Value) {
                InnerProp->CopySingleValueToScriptVM(RawPtr, Value);
                return *this;
            }
        };

        using iterator_category = std::output_iterator_tag;
        using difference_type  = std::ptrdiff_t;
        using value_type = array_elem_ref;
        using pointer = array_elem_ref*;  
        using reference = array_elem_ref&; 

        inline ScriptArrayOutputIterator(void* TargetArray, const FArrayProperty* ArrayProp) : 
            ArrayHelper(ArrayProp, TargetArray),
            Ref{ ArrayProp->Inner, ArrayHelper.GetRawPtr(0) },
            Index(0)
        {
        }

        inline reference operator*() { 
            return Ref;
        }

        inline pointer operator->() { 
            return &Ref;
        }

        inline ScriptArrayOutputIterator& operator++() { 
            Ref.RawPtr = ArrayHelper.GetRawPtr(++Index);
            return *this; 
        }  

        inline ScriptArrayOutputIterator operator++(int) { 
            ScriptArrayOutputIterator tmp = *this; 
            ++(*this); 
            return tmp; 
        }

        inline friend bool operator== (const ScriptArrayOutputIterator& a, const ScriptArrayOutputIterator& b) {
             return a.Ref.RawPtr == b.Ref.RawPtr;
        };

        inline friend bool operator!= (const ScriptArrayOutputIterator& a, const ScriptArrayOutputIterator& b) { 
             return a.Ref.RawPtr != b.Ref.RawPtr;
        }; 
    private:
        FScriptArrayHelper ArrayHelper;
        array_elem_ref Ref;
        int Index;
    };

    struct ConstWeightIterator {
        using iterator_category = std::input_iterator_tag;
        using difference_type  = std::ptrdiff_t;
        using value_type = float;
        using pointer = float*;  
        using reference = float&; 

        inline ConstWeightIterator(float V) : Value(V) {}


        inline reference operator*() { 
            return Value;
        }
        inline pointer operator->() { 
            return &Value;
        }

        inline ConstWeightIterator& operator++() { 
            return *this; 
        }  

        inline ConstWeightIterator operator++(int) { 
            return *this; 
        }
    private:
        float Value;
    };

    // DOTA2的PRD算法C值查找表
    // 来源：https://gaming.stackexchange.com/questions/161430/calculating-the-constant-c-in-dota-2-pseudo-random-distribution
    const TMap<float, float> PRDConstantTable = {
        {0.01f, 0.00014502f},
        {0.02f, 0.00058127f},
        {0.03f, 0.00130897f},
        {0.04f, 0.00232805f},
        {0.05f, 0.00363838f},
        {0.06f, 0.00523982f},
        {0.10f, 0.01445917f},
        {0.15f, 0.03229115f},
        {0.20f, 0.05704645f},
        {0.25f, 0.08872936f},
        {0.30f, 0.12731076f},
        {0.35f, 0.17278758f},
        {0.40f, 0.22516969f},
        {0.50f, 0.34874277f},
        {0.60f, 0.50597414f},
        {0.70f, 0.70062819f},
        {0.75f, 0.81379401f},
        {0.80f, 0.93853302f},
        {0.85f, 1.07575038f},
        {0.90f, 1.22642714f},
        {0.95f, 1.39164174f},
    };

    // 获取最接近的PRD常数C
    float GetPRDConstant(float P)
    {
        // 边界检查
        if (P <= 0.f) return 0.f;
        if (P >= 1.f) return 1.f;

        // 在查找表中找到最接近的值
        float ClosestP = 0.f;
        float MinDiff = FLT_MAX;
        
        for (const auto& Pair : PRDConstantTable)
        {
            float Diff = FMath::Abs(Pair.Key - P);
            if (Diff < MinDiff)
            {
                MinDiff = Diff;
                ClosestP = Pair.Key;
            }
        }

        // 如果找到完全匹配的值，直接返回
        if (FMath::IsNearlyEqual(P, ClosestP, KINDA_SMALL_NUMBER))
        {
            return PRDConstantTable[ClosestP];
        }

        // 否则在最近的两个值之间进行线性插值
        float LowerP = 0.f;
        float UpperP = 1.f;
        float LowerC = 0.f;
        float UpperC = 1.f;

        for (const auto& Pair : PRDConstantTable)
        {
            if (Pair.Key <= P && Pair.Key > LowerP)
            {
                LowerP = Pair.Key;
                LowerC = Pair.Value;
            }
            if (Pair.Key >= P && Pair.Key < UpperP)
            {
                UpperP = Pair.Key;
                UpperC = Pair.Value;
            }
        }

        // 线性插值
        return LowerC + (P - LowerP) * (UpperC - LowerC) / (UpperP - LowerP);
    }
}

void URandomShuffleArrayLibrary::GenericArray_RandomSample(
    void* TargetArray, const FArrayProperty* ArrayProp, 
    void* Weights, const FArrayProperty* WeightsProp, 
    int32 Count, FRandomStream* Stream,
    void* OutputArray, FArrayProperty* OutputProp)
{
    using namespace RandomShuffles;

	if(!TargetArray || !OutputArray) {
        return;
    }

    FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
    if (ArrayHelper.Num() < 1) {
        return;
    }
    
    if (Count < 0) {
        Count = ArrayHelper.Num();
    }

    FScriptArrayHelper OutputHelper(OutputProp, OutputArray);
    OutputHelper.Resize(Count);
    
    auto randFunc = RandomShuffles::FRand{ Stream };
    auto begin = ScriptArrayInputIterator<uint8*>(TargetArray, ArrayProp);
    auto end = ScriptArrayInputIterator<uint8*>(TargetArray, ArrayProp, ArrayHelper.Num());
    auto out = ScriptArrayOutputIterator(OutputArray, OutputProp);

    if (WeightsProp) {
        FScriptArrayHelper WeightsHelper(WeightsProp, Weights);
        if (WeightsHelper.Num() < ArrayHelper.Num()) {
            UE_LOG(LogRandomShuffle, Error, TEXT("Expected %i weights but only found %i"), ArrayHelper.Num(), WeightsHelper.Num());
            return;
        }

        auto wbegin = ScriptArrayInputIterator<float>(Weights, WeightsProp);
        RandomSample(begin, end, wbegin, out, Count, randFunc);
    }
    else {
        auto wbegin = ConstWeightIterator(1.0f);
        RandomSample(begin, end, wbegin, out, Count, randFunc);
    }
}

void URandomShuffleArrayLibrary::GenericArray_StrictWeightRandomSample(
    void* TargetArray, const FArrayProperty* ArrayProp, 
    void* Weights, const FArrayProperty* WeightsProp, 
    int32 Count, FRandomStream* Stream,
    void* OutputArray, FArrayProperty* OutputProp)
{
    using namespace RandomShuffles;

	if(!TargetArray || !OutputArray) {
        return;
    }

    FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
    if (ArrayHelper.Num() < 1) {
        return;
    }
    
    if (Count < 0) {
        Count = ArrayHelper.Num();
    }

    FScriptArrayHelper OutputHelper(OutputProp, OutputArray);
    OutputHelper.Resize(Count);
    
    auto randFunc = RandomShuffles::FRand{ Stream };
    auto begin = ScriptArrayInputIterator<uint8*>(TargetArray, ArrayProp);
    auto end = ScriptArrayInputIterator<uint8*>(TargetArray, ArrayProp, ArrayHelper.Num());
    auto out = ScriptArrayOutputIterator(OutputArray, OutputProp);

    if (WeightsProp) {
        FScriptArrayHelper WeightsHelper(WeightsProp, Weights);
        if (WeightsHelper.Num() < ArrayHelper.Num()) {
            UE_LOG(LogRandomShuffle, Error, TEXT("Expected %i weights but only found %i"), ArrayHelper.Num(), WeightsHelper.Num());
            return;
        }

        auto wbegin = ScriptArrayInputIterator<float>(Weights, WeightsProp);
        WeightPoolSample(begin, end, wbegin, out, Count, randFunc);
    }
    else {
        auto wbegin = ConstWeightIterator(1.0f);
        WeightPoolSample(begin, end, wbegin, out, Count, randFunc);
    }
}

bool URandomShuffleArrayLibrary::PseudoRandomBool(float BaseChance, float CurrentCompensation, float& OutNextCompensation, float& OutActualChance)
{
    // 边界检查
    const float P = FMath::Clamp(BaseChance, 0.f, 1.f);
    if (P <= 0.f)
    {
        OutNextCompensation = 1.f;
        OutActualChance = 0.f;
        return false;
    }
    if (P >= 1.f)
    {
        OutNextCompensation = 1.f;
        OutActualChance = 1.f;
        return true;
    }

    // 获取DOTA2 PRD常数C
    const float C = RandomShuffles::GetPRDConstant(P);
    
    // 计算当前实际触发概率
    OutActualChance = FMath::Min(C * CurrentCompensation, 1.0f);
    
    // 生成随机数并判断是否触发
    const bool bSuccess = FMath::FRand() < OutActualChance;

    // 更新补偿值：成功重置为1，失败则+1
    OutNextCompensation = bSuccess ? 1.f : CurrentCompensation + 1.f;

    return bSuccess;
}

bool URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(float BaseChance, float CurrentCompensation, FRandomStream& Stream, float& OutNextCompensation, float& OutActualChance)
{
    // 边界检查
    const float P = FMath::Clamp(BaseChance, 0.f, 1.f);
    if (P <= 0.f)
    {
        OutNextCompensation = 1.f;
        OutActualChance = 0.f;
        return false;
    }
    if (P >= 1.f)
    {
        OutNextCompensation = 1.f;
        OutActualChance = 1.f;
        return true;
    }

    // 获取DOTA2 PRD常数C
    const float C = RandomShuffles::GetPRDConstant(P);
    
    // 计算当前实际触发概率
    OutActualChance = FMath::Min(C * CurrentCompensation, 1.0f);
    
    // 使用随机流生成随机数并判断是否触发
    const bool bSuccess = Stream.FRand() < OutActualChance;

    // 更新补偿值：成功重置为1，失败则+1
    OutNextCompensation = bSuccess ? 1.f : CurrentCompensation + 1.f;

    return bSuccess;
}
