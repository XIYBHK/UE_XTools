#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "SortLibrary.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include <algorithm>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNaturalSort_PrefixOrdering,
    "XTools.Sort.Library.NaturalPrefixOrdering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNaturalSort_PrefixOrdering::RunTest(const FString& Parameters)
{
    // 旧比较器会令 01 等价于 1a 和 1b，却又令 1a < 1b。
    const TArray<FString> Expected = {TEXT("01"), TEXT("0001"), TEXT("1a"), TEXT("1b"), TEXT("2")};
    TArray<int32> Permutation = {0, 1, 2, 3, 4};
    FArrayProperty* Property = new FArrayProperty(GetTransientPackage(), NAME_None, RF_Transient);
    Property->Inner = new FStrProperty(Property, TEXT("Element"), RF_Transient);
    do
    {
        TArray<FString> Input;
        TArray<FName> Names;
        for (int32 Index : Permutation)
        {
            Input.Add(Expected[Index]);
            Names.Add(FName(*Expected[Index]));
        }
        for (bool bAscending : {true, false})
        {
            TArray<FString> Sorted;
            TArray<FName> SortedNames;
            TArray<int32> Indices;
            USortLibrary::SortStringArray(Input, Sorted, Indices, bAscending);
            TestEqual(TEXT("字符串数量守恒"), Sorted.Num(), Expected.Num());
            for (int32 Index = 0; Index < Sorted.Num(); ++Index)
            {
                const FString& Value = Expected[bAscending ? Index : Expected.Num() - 1 - Index];
                TestEqual(TEXT("任意输入排列应产生一致的自然顺序"), Sorted[Index], Value);
                TestEqual(TEXT("原始索引应正确映射"), Input[Indices[Index]], Value);
            }
            USortLibrary::SortNameArray(Names, SortedNames, Indices, bAscending);
            for (int32 Index = 0; Index < SortedNames.Num(); ++Index)
            {
                TestEqual(TEXT("FName 入口与字符串入口一致"), SortedNames[Index], FName(*Sorted[Index]));
            }
            TArray<FString> PropertySorted = Input;
            USortLibrary::GenericSortArrayByProperty(&PropertySorted, Property, NAME_None, bAscending, Indices);
            TestTrue(TEXT("属性排序共享相同的自然顺序"), PropertySorted == Sorted);
        }
    }
    while (std::next_permutation(Permutation.GetData(), Permutation.GetData() + Permutation.Num()));
    delete Property;
    return true;
}

#endif
