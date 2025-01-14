/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#include "RandomShuffleArrayLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "RandomSample.h"
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
    Count = std::min(Count, ArrayHelper.Num());
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
