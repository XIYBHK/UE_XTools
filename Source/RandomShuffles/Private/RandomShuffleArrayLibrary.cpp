/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#include "RandomShuffleArrayLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "RandomSample.h"
#include "WeightPoolSample.h"
#include "RandomShuffleLog.h"

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

    // DOTA2的PRD算法C值查找表 - 使用数组优化查找性能
    // 来源：https://gaming.stackexchange.com/questions/161430/calculating-the-constant-c-in-dota-2-pseudo-random-distribution
    struct FPRDEntry
    {
        float Probability;
        float Constant;
    };

    // 按概率排序的PRD常数表，便于二分查找
    const TArray<FPRDEntry> PRDConstantTable = {
        {0.01f, 0.000156f}, {0.02f, 0.000620f}, {0.03f, 0.001386f}, {0.04f, 0.002449f}, {0.05f, 0.003802f},
        {0.06f, 0.005440f}, {0.07f, 0.007359f}, {0.08f, 0.009552f}, {0.09f, 0.012016f}, {0.10f, 0.014746f},
        {0.11f, 0.017736f}, {0.12f, 0.020983f}, {0.13f, 0.024482f}, {0.14f, 0.028230f}, {0.15f, 0.032221f},
        {0.16f, 0.036452f}, {0.17f, 0.040920f}, {0.18f, 0.045620f}, {0.19f, 0.050549f}, {0.20f, 0.055704f},
        {0.21f, 0.061081f}, {0.22f, 0.066676f}, {0.23f, 0.072488f}, {0.24f, 0.078511f}, {0.25f, 0.084744f},
        {0.26f, 0.091183f}, {0.27f, 0.097826f}, {0.28f, 0.104670f}, {0.29f, 0.111712f}, {0.30f, 0.118949f},
        {0.31f, 0.126379f}, {0.32f, 0.134001f}, {0.33f, 0.141805f}, {0.34f, 0.149810f}, {0.35f, 0.157983f},
        {0.36f, 0.166329f}, {0.37f, 0.174909f}, {0.38f, 0.183625f}, {0.39f, 0.192486f}, {0.40f, 0.201547f},
        {0.41f, 0.210920f}, {0.42f, 0.220365f}, {0.43f, 0.229899f}, {0.44f, 0.239540f}, {0.45f, 0.249307f},
        {0.46f, 0.259872f}, {0.47f, 0.270453f}, {0.48f, 0.281008f}, {0.49f, 0.291552f}, {0.50f, 0.302103f},
        {0.51f, 0.312677f}, {0.52f, 0.323291f}, {0.53f, 0.334120f}, {0.54f, 0.347370f}, {0.55f, 0.360398f},
        {0.56f, 0.373217f}, {0.57f, 0.385840f}, {0.58f, 0.398278f}, {0.59f, 0.410545f}, {0.60f, 0.422650f},
        {0.61f, 0.434604f}, {0.62f, 0.446419f}, {0.63f, 0.458104f}, {0.64f, 0.469670f}, {0.65f, 0.481125f},
        {0.66f, 0.492481f}, {0.67f, 0.507463f}, {0.68f, 0.529412f}, {0.69f, 0.550725f}, {0.70f, 0.571429f},
        {0.71f, 0.591549f}, {0.72f, 0.611111f}, {0.73f, 0.630137f}, {0.74f, 0.648649f}, {0.75f, 0.666667f},
        {0.76f, 0.684211f}, {0.77f, 0.701299f}, {0.78f, 0.717949f}, {0.79f, 0.734177f}, {0.80f, 0.750000f},
        {0.81f, 0.765432f}, {0.82f, 0.780488f}, {0.83f, 0.795181f}, {0.84f, 0.809524f}, {0.85f, 0.823529f},
        {0.86f, 0.837209f}, {0.87f, 0.850575f}, {0.88f, 0.863636f}, {0.89f, 0.876404f}, {0.90f, 0.888889f},
        {0.91f, 0.901099f}, {0.92f, 0.913043f}, {0.93f, 0.924731f}, {0.94f, 0.936170f}, {0.95f, 0.947368f},
        {0.96f, 0.958333f}, {0.97f, 0.969072f}, {0.98f, 0.979592f}, {0.99f, 0.989899f}
    };

    // 获取最接近的PRD常数C - 使用二分查找优化性能
    float GetPRDConstant(float P)
    {
        // 边界检查
        if (P <= 0.f) return 0.f;
        if (P >= 1.f) return 1.f;

        // 使用二分查找找到合适的区间
        int32 Left = 0;
        int32 Right = PRDConstantTable.Num() - 1;

        // 检查是否完全匹配
        while (Left <= Right)
        {
            int32 Mid = (Left + Right) / 2;
            const float MidP = PRDConstantTable[Mid].Probability;

            if (FMath::IsNearlyEqual(MidP, P, 0.001f))
            {
                return PRDConstantTable[Mid].Constant;
            }
            else if (MidP < P)
            {
                Left = Mid + 1;
            }
            else
            {
                Right = Mid - 1;
            }
        }

        // 找到插值区间
        int32 LowerIndex = FMath::Clamp(Right, 0, PRDConstantTable.Num() - 1);
        int32 UpperIndex = FMath::Clamp(Left, 0, PRDConstantTable.Num() - 1);

        if (LowerIndex == UpperIndex)
        {
            return PRDConstantTable[LowerIndex].Constant;
        }

        // 线性插值
        const float LowerP = PRDConstantTable[LowerIndex].Probability;
        const float UpperP = PRDConstantTable[UpperIndex].Probability;
        const float LowerC = PRDConstantTable[LowerIndex].Constant;
        const float UpperC = PRDConstantTable[UpperIndex].Constant;

        const float Alpha = (P - LowerP) / (UpperP - LowerP);
        return LowerC + Alpha * (UpperC - LowerC);
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

namespace RandomShuffles
{
    // 通用PRD计算逻辑，避免重复代码
    bool CalculatePRD(float BaseChance, int32 FailureCount, int32& OutFailureCount, float& OutActualChance, TFunction<float()> RandomFunc)
    {
        // 边界检查和参数规范化
        const float P = FMath::Clamp(BaseChance, 0.f, 1.f);
        FailureCount = FMath::Max(FailureCount, 0);

        // 特殊情况处理
        if (P <= 0.f)
        {
            OutFailureCount = 0;
            OutActualChance = 0.f;
            return false;
        }
        if (P >= 1.f)
        {
            OutFailureCount = 0;
            OutActualChance = 1.f;
            return true;
        }

        // 获取DOTA2 PRD常数C
        const float C = GetPRDConstant(P);

        // 计算当前实际触发概率 = (失败次数 + 1) * PRD常数C
        OutActualChance = FMath::Min(static_cast<float>(FailureCount + 1) * C, 1.f);

        // 生成随机数并判断是否触发
        const bool bSuccess = RandomFunc() < OutActualChance;

        // 更新失败次数
        OutFailureCount = bSuccess ? 0 : FailureCount + 1;

        return bSuccess;
    }
}

// 静态成员定义 - 线程安全的PRD状态管理
TMap<FName, int32> URandomShuffleArrayLibrary::PRDStateMap;
FCriticalSection URandomShuffleArrayLibrary::PRDStateLock;
FPRDPerformanceStats URandomShuffleArrayLibrary::PerformanceStats;

// 简单版本 - 自动状态管理
bool URandomShuffleArrayLibrary::PseudoRandomBool(float BaseChance, FString StateID)
{
    // 输入验证
    BaseChance = FMath::Clamp(BaseChance, RandomShufflesConfig::MinValidChance, RandomShufflesConfig::MaxValidChance);

    int32& FailureCount = GetOrCreatePRDState(StateID);
    float ActualChance = 0.0f;

    const bool bResult = RandomShuffles::CalculatePRD(BaseChance, FailureCount, FailureCount, ActualChance,
        []() { return FMath::FRand(); });

    // 更新性能统计
    UpdatePerformanceStats(FailureCount);

    return bResult;
}

// 高级版本 - 完全手动控制
bool URandomShuffleArrayLibrary::PseudoRandomBoolAdvanced(float BaseChance, int32& OutFailureCount, float& OutActualChance, FString StateID, int32 FailureCount)
{
    return RandomShuffles::CalculatePRD(BaseChance, FailureCount, OutFailureCount, OutActualChance,
        []() { return FMath::FRand(); });
}

// 简单版本 - 随机流 + 自动状态管理
bool URandomShuffleArrayLibrary::PseudoRandomBoolFromStream(float BaseChance, FRandomStream& Stream, FString StateID)
{
    int32& FailureCount = GetOrCreatePRDState(StateID);
    float ActualChance = 0.0f;
    return RandomShuffles::CalculatePRD(BaseChance, FailureCount, FailureCount, ActualChance,
        [&Stream]() { return Stream.FRand(); });
}

// 高级版本 - 随机流 + 完全手动控制
bool URandomShuffleArrayLibrary::PseudoRandomBoolFromStreamAdvanced(float BaseChance, FRandomStream& Stream, int32& OutFailureCount, float& OutActualChance, FString StateID, int32 FailureCount)
{
    return RandomShuffles::CalculatePRD(BaseChance, FailureCount, OutFailureCount, OutActualChance,
        [&Stream]() { return Stream.FRand(); });
}

// PRD状态管理函数实现 - 线程安全版本
int32& URandomShuffleArrayLibrary::GetOrCreatePRDState(const FString& StateID)
{
    FScopeLock Lock(&PRDStateLock);

    // 首次使用时预分配内存
    if (PRDStateMap.Num() == 0)
    {
        PRDStateMap.Reserve(RandomShufflesConfig::DefaultStateMapReserve);
    }

    const FName StateKey(*StateID);
    if (!PRDStateMap.Contains(StateKey))
    {
        // 检查状态映射大小限制
        if (PRDStateMap.Num() >= RandomShufflesConfig::MaxStateMapSize)
        {
            UE_LOG(LogRandomShuffle, Warning,
                TEXT("PRD状态映射已达到最大大小限制 (%d)，无法添加新状态: %s"),
                RandomShufflesConfig::MaxStateMapSize, *StateID);

            // 返回默认状态（使用"Default"键）
            const FName DefaultKey(TEXT("Default"));
            if (!PRDStateMap.Contains(DefaultKey))
            {
                PRDStateMap.Add(DefaultKey, 0);
            }
            return PRDStateMap[DefaultKey];
        }

        PRDStateMap.Add(StateKey, 0);
    }
    return PRDStateMap[StateKey];
}

void URandomShuffleArrayLibrary::ClearPRDState(FString StateID)
{
    FScopeLock Lock(&PRDStateLock);
    const FName StateKey(*StateID);
    PRDStateMap.Remove(StateKey);
}

void URandomShuffleArrayLibrary::ClearAllPRDStates()
{
    FScopeLock Lock(&PRDStateLock);
    PRDStateMap.Empty();

    // 重置性能统计
    PerformanceStats = FPRDPerformanceStats();
}

// 性能统计实现
FPRDPerformanceStats URandomShuffleArrayLibrary::GetPRDPerformanceStats()
{
    FScopeLock Lock(&PRDStateLock);

    // 更新当前状态映射大小
    PerformanceStats.StateMapSize = PRDStateMap.Num();

    return PerformanceStats;
}

void URandomShuffleArrayLibrary::ResetPRDPerformanceStats()
{
    FScopeLock Lock(&PRDStateLock);
    PerformanceStats = FPRDPerformanceStats();
}

void URandomShuffleArrayLibrary::UpdatePerformanceStats(int32 FailureCount)
{
    // 注意：此函数假设已经在锁保护下调用
    PerformanceStats.TotalCalls++;
    PerformanceStats.MaxFailureCount = FMath::Max(PerformanceStats.MaxFailureCount, FailureCount);

    // 计算平均失败次数（简单移动平均）
    const float Alpha = 0.1f; // 平滑因子
    PerformanceStats.AverageFailureCount =
        PerformanceStats.AverageFailureCount * (1.0f - Alpha) + FailureCount * Alpha;
}
