// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// ✅ 蓝图库依赖
#include "ObjectPoolLibrary.h"

// ✅ 反射系统依赖
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet/BlueprintFunctionLibrary.h"

/**
 * 蓝图元数据验证辅助工具
 */
class FBlueprintMetadataTestHelpers
{
public:
    /**
     * 验证UFUNCTION的元数据
     */
    static bool ValidateFunctionMetadata(UFunction* Function, const FString& ExpectedDisplayName, const FString& ExpectedCategory)
    {
        if (!Function)
        {
            return false;
        }

        // 检查DisplayName元数据
        FString DisplayName = Function->GetMetaData(TEXT("DisplayName"));
        if (DisplayName != ExpectedDisplayName)
        {
            UE_LOG(LogTemp, Warning, TEXT("DisplayName不匹配: 期望='%s', 实际='%s'"), *ExpectedDisplayName, *DisplayName);
            return false;
        }

        // 检查Category元数据
        FString Category = Function->GetMetaData(TEXT("Category"));
        if (Category != ExpectedCategory)
        {
            UE_LOG(LogTemp, Warning, TEXT("Category不匹配: 期望='%s', 实际='%s'"), *ExpectedCategory, *Category);
            return false;
        }

        return true;
    }

    /**
     * 验证函数是否为BlueprintCallable
     */
    static bool IsBlueprintCallable(UFunction* Function)
    {
        if (!Function)
        {
            return false;
        }

        return Function->HasAnyFunctionFlags(FUNC_BlueprintCallable);
    }

    /**
     * 验证参数的元数据
     */
    static bool ValidateParameterMetadata(UFunction* Function, const FString& ParameterName, const FString& ExpectedDisplayName)
    {
        if (!Function)
        {
            return false;
        }

        FString MetaKey = FString::Printf(TEXT("DisplayName.%s"), *ParameterName);
        FString DisplayName = Function->GetMetaData(*MetaKey);
        
        if (DisplayName != ExpectedDisplayName)
        {
            UE_LOG(LogTemp, Warning, TEXT("参数DisplayName不匹配: 参数='%s', 期望='%s', 实际='%s'"), 
                *ParameterName, *ExpectedDisplayName, *DisplayName);
            return false;
        }

        return true;
    }

    /**
     * 获取函数的所有参数名称
     */
    static TArray<FString> GetFunctionParameterNames(UFunction* Function)
    {
        TArray<FString> ParameterNames;
        
        if (!Function)
        {
            return ParameterNames;
        }

        for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
        {
            FProperty* Property = *PropIt;
            ParameterNames.Add(Property->GetName());
        }

        return ParameterNames;
    }
};

/**
 * 测试蓝图库函数的基本元数据
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryMetadataBasicTest, 
    "ObjectPool.BlueprintLibrary.MetadataBasicTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryMetadataBasicTest::RunTest(const FString& Parameters)
{
    UClass* LibraryClass = UObjectPoolLibrary::StaticClass();
    TestNotNull("蓝图库类应该存在", LibraryClass);

    if (!LibraryClass)
    {
        return false;
    }

    // 1. 测试RegisterActorClass函数的元数据
    UFunction* RegisterFunction = LibraryClass->FindFunctionByName(TEXT("RegisterActorClass"));
    TestNotNull("RegisterActorClass函数应该存在", RegisterFunction);

    if (RegisterFunction)
    {
        TestTrue("RegisterActorClass应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(RegisterFunction));

        TestTrue("RegisterActorClass应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                RegisterFunction, 
                TEXT("注册Actor类"), 
                TEXT("XTools|对象池")
            ));

        // 验证参数元数据
        TestTrue("ActorClass参数应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateParameterMetadata(
                RegisterFunction, 
                TEXT("ActorClass"), 
                TEXT("Actor类")
            ));

        TestTrue("InitialSize参数应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateParameterMetadata(
                RegisterFunction, 
                TEXT("InitialSize"), 
                TEXT("初始大小")
            ));

        TestTrue("HardLimit参数应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateParameterMetadata(
                RegisterFunction, 
                TEXT("HardLimit"), 
                TEXT("硬限制")
            ));
    }

    // 2. 测试SpawnActorFromPool函数的元数据
    UFunction* SpawnFunction = LibraryClass->FindFunctionByName(TEXT("SpawnActorFromPool"));
    TestNotNull("SpawnActorFromPool函数应该存在", SpawnFunction);

    if (SpawnFunction)
    {
        TestTrue("SpawnActorFromPool应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(SpawnFunction));

        TestTrue("SpawnActorFromPool应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                SpawnFunction, 
                TEXT("从池中生成Actor"), 
                TEXT("XTools|对象池")
            ));

        // 验证参数元数据
        TestTrue("ActorClass参数应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateParameterMetadata(
                SpawnFunction, 
                TEXT("ActorClass"), 
                TEXT("Actor类")
            ));

        TestTrue("SpawnTransform参数应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateParameterMetadata(
                SpawnFunction, 
                TEXT("SpawnTransform"), 
                TEXT("生成位置")
            ));
    }

    // 3. 测试ReturnActorToPool函数的元数据
    UFunction* ReturnFunction = LibraryClass->FindFunctionByName(TEXT("ReturnActorToPool"));
    TestNotNull("ReturnActorToPool函数应该存在", ReturnFunction);

    if (ReturnFunction)
    {
        TestTrue("ReturnActorToPool应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(ReturnFunction));

        TestTrue("ReturnActorToPool应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                ReturnFunction, 
                TEXT("归还Actor到池"), 
                TEXT("XTools|对象池")
            ));

        // 验证参数元数据
        TestTrue("Actor参数应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateParameterMetadata(
                ReturnFunction, 
                TEXT("Actor"), 
                TEXT("Actor")
            ));
    }

    return true;
}

/**
 * 测试蓝图库高级函数的元数据
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryMetadataAdvancedTest, 
    "ObjectPool.BlueprintLibrary.MetadataAdvancedTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryMetadataAdvancedTest::RunTest(const FString& Parameters)
{
    UClass* LibraryClass = UObjectPoolLibrary::StaticClass();
    TestNotNull("蓝图库类应该存在", LibraryClass);

    if (!LibraryClass)
    {
        return false;
    }

    // 1. 测试PrewarmPool函数的元数据
    UFunction* PrewarmFunction = LibraryClass->FindFunctionByName(TEXT("PrewarmPool"));
    TestNotNull("PrewarmPool函数应该存在", PrewarmFunction);

    if (PrewarmFunction)
    {
        TestTrue("PrewarmPool应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(PrewarmFunction));

        TestTrue("PrewarmPool应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                PrewarmFunction, 
                TEXT("预热对象池"), 
                TEXT("XTools|对象池")
            ));
    }

    // 2. 测试GetPoolStats函数的元数据
    UFunction* GetStatsFunction = LibraryClass->FindFunctionByName(TEXT("GetPoolStats"));
    TestNotNull("GetPoolStats函数应该存在", GetStatsFunction);

    if (GetStatsFunction)
    {
        TestTrue("GetPoolStats应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(GetStatsFunction));

        TestTrue("GetPoolStats应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                GetStatsFunction, 
                TEXT("获取池统计信息"), 
                TEXT("XTools|对象池")
            ));
    }

    // 3. 测试IsActorClassRegistered函数的元数据
    UFunction* IsRegisteredFunction = LibraryClass->FindFunctionByName(TEXT("IsActorClassRegistered"));
    TestNotNull("IsActorClassRegistered函数应该存在", IsRegisteredFunction);

    if (IsRegisteredFunction)
    {
        TestTrue("IsActorClassRegistered应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(IsRegisteredFunction));

        TestTrue("IsActorClassRegistered应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                IsRegisteredFunction, 
                TEXT("检查类是否已注册"), 
                TEXT("XTools|对象池")
            ));
    }

    // 4. 测试GetObjectPoolSubsystem函数的元数据
    UFunction* GetSubsystemFunction = LibraryClass->FindFunctionByName(TEXT("GetObjectPoolSubsystem"));
    TestNotNull("GetObjectPoolSubsystem函数应该存在", GetSubsystemFunction);

    if (GetSubsystemFunction)
    {
        TestTrue("GetObjectPoolSubsystem应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(GetSubsystemFunction));

        TestTrue("GetObjectPoolSubsystem应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                GetSubsystemFunction, 
                TEXT("获取对象池子系统"), 
                TEXT("XTools|对象池")
            ));
    }

    // 5. 测试GetObjectPoolSubsystemSimplified函数的元数据
    UFunction* GetSimplifiedSubsystemFunction = LibraryClass->FindFunctionByName(TEXT("GetObjectPoolSubsystemSimplified"));
    TestNotNull("GetObjectPoolSubsystemSimplified函数应该存在", GetSimplifiedSubsystemFunction);

    if (GetSimplifiedSubsystemFunction)
    {
        TestTrue("GetObjectPoolSubsystemSimplified应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(GetSimplifiedSubsystemFunction));

        TestTrue("GetObjectPoolSubsystemSimplified应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                GetSimplifiedSubsystemFunction, 
                TEXT("获取简化对象池子系统"), 
                TEXT("XTools|对象池")
            ));
    }

    return true;
}

/**
 * 测试蓝图库批量操作函数的元数据
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectPoolBlueprintLibraryMetadataBatchTest, 
    "ObjectPool.BlueprintLibrary.MetadataBatchTest", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectPoolBlueprintLibraryMetadataBatchTest::RunTest(const FString& Parameters)
{
    UClass* LibraryClass = UObjectPoolLibrary::StaticClass();
    TestNotNull("蓝图库类应该存在", LibraryClass);

    if (!LibraryClass)
    {
        return false;
    }

    // 1. 测试BatchSpawnActors函数的元数据
    UFunction* BatchSpawnFunction = LibraryClass->FindFunctionByName(TEXT("BatchSpawnActors"));
    TestNotNull("BatchSpawnActors函数应该存在", BatchSpawnFunction);

    if (BatchSpawnFunction)
    {
        TestTrue("BatchSpawnActors应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(BatchSpawnFunction));

        TestTrue("BatchSpawnActors应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                BatchSpawnFunction, 
                TEXT("批量生成Actor"), 
                TEXT("XTools|对象池")
            ));
    }

    // 2. 测试BatchReturnActors函数的元数据
    UFunction* BatchReturnFunction = LibraryClass->FindFunctionByName(TEXT("BatchReturnActors"));
    TestNotNull("BatchReturnActors函数应该存在", BatchReturnFunction);

    if (BatchReturnFunction)
    {
        TestTrue("BatchReturnActors应该是BlueprintCallable", 
            FBlueprintMetadataTestHelpers::IsBlueprintCallable(BatchReturnFunction));

        TestTrue("BatchReturnActors应该有正确的DisplayName", 
            FBlueprintMetadataTestHelpers::ValidateFunctionMetadata(
                BatchReturnFunction, 
                TEXT("批量归还Actor"), 
                TEXT("XTools|对象池")
            ));
    }

    // 3. 验证所有函数都有WorldContext元数据
    TArray<UFunction*> AllFunctions;
    for (TFieldIterator<UFunction> FuncIt(LibraryClass); FuncIt; ++FuncIt)
    {
        UFunction* Function = *FuncIt;
        if (Function && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
        {
            AllFunctions.Add(Function);
        }
    }

    TestTrue("应该找到多个BlueprintCallable函数", AllFunctions.Num() > 0);

    for (UFunction* Function : AllFunctions)
    {
        FString WorldContextMeta = Function->GetMetaData(TEXT("WorldContext"));
        if (!WorldContextMeta.IsEmpty())
        {
            TestEqual("WorldContext元数据应该指向正确的参数", WorldContextMeta, TEXT("WorldContext"));
        }
    }

    return true;
}

#endif // WITH_OBJECTPOOL_TESTS
