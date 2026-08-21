/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "FormationManagerComponent.h"
#include "Misc/AutomationTest.h"

// 访问方式说明：无法通过"测试子类"暴露 protected CreateCostMatrix——
// UCLASS 不能声明在 .cpp 中（UHT 只处理头文件），无 UCLASS 的 C++ 子类无法经 NewObject 实例化（缺少 StaticClass），
// C++ protected 访问规则也不允许通过基类实例借用派生类访问权。
// 因此采用最小等价方案：头文件中 friend 声明本测试类（编译期授权，不新增生产 API）。
//
// 参数说明：CalculateAbsoluteDistanceCostMatrix 的行列数均取 FromPositions.Num() 并直接索引 ToPositions[j]，
// 假定 |From|==|To|（上游 CalculateOptimalAssignment 已强制等数）。故不使用 From=[A,B]/To=[C]（会越界读 To[1]），
// 改用同样保持"展平序列相同、切分点不同"这一结构碰撞本质的安全参数。

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFormationManagerComponentCacheTests,
    "XTools.Formation.Manager.CostMatrixCacheAvoidsStructuralCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFormationManagerComponentCacheTests::RunTest(const FString& Parameters)
{
    UFormationManagerComponent* Manager = NewObject<UFormationManagerComponent>();
    TestNotNull(TEXT("阵型管理器组件应可创建"), Manager);
    if (!Manager)
    {
        return false;
    }

    // 四个位置共线等距，距离全部为 100，矩阵值确定可读
    const FVector A = FVector::ZeroVector;
    const FVector B = FVector(100.0, 0.0, 0.0);
    const FVector C = FVector(200.0, 0.0, 0.0);
    const FVector D = FVector(300.0, 0.0, 0.0);

    // 第一次调用：From=[A]、To=[B,C,D]，展平序列 [A,B,C,D]
    // 矩阵行列均取 From.Num()=1 → 1x1，值 |A-B|=100
    const TArray<FVector> FirstFrom = { A };
    const TArray<FVector> FirstTo = { B, C, D };
    const TArray<TArray<float>> FirstMatrix = Manager->CreateCostMatrix(FirstFrom, FirstTo, false);

    TestEqual(TEXT("第一次调用应生成 1 行成本矩阵"), FirstMatrix.Num(), 1);
    if (FirstMatrix.Num() == 1)
    {
        TestEqual(TEXT("第一次调用每行应有 1 列"), FirstMatrix[0].Num(), 1);
        TestTrue(TEXT("第一次调用矩阵值应为 |A-B|=100"),
            FMath::IsNearlyEqual(FirstMatrix[0][0], 100.0f));
    }

    // 第二次调用：From=[A,B]、To=[C,D]，展平序列同为 [A,B,C,D]
    // 旧实现哈希只覆盖展平序列 → 与第一次同键（模式相同、TTL 内）→ 误命中第一次的 1x1 缓存
    // 新实现将两数组元素数量纳入哈希 → 键不同 → 重新计算 2x2
    const TArray<FVector> SecondFrom = { A, B };
    const TArray<FVector> SecondTo = { C, D };
    const TArray<TArray<float>> SecondMatrix = Manager->CreateCostMatrix(SecondFrom, SecondTo, false);

    TestEqual(TEXT("结构碰撞输入不应复用缓存，应生成 2 行成本矩阵"), SecondMatrix.Num(), 2);
    if (SecondMatrix.Num() == 2)
    {
        TestEqual(TEXT("第二次调用每行应有 2 列"), SecondMatrix[0].Num(), 2);
        TestEqual(TEXT("第二次调用第二行也应有 2 列"), SecondMatrix[1].Num(), 2);

        // 内容与直接计算一致：[i][j] = |From[i] - To[j]| = [[200, 300], [100, 200]]
        TestTrue(TEXT("矩阵[0][0]应为 |A-C|=200"), FMath::IsNearlyEqual(SecondMatrix[0][0], 200.0f));
        TestTrue(TEXT("矩阵[0][1]应为 |A-D|=300"), FMath::IsNearlyEqual(SecondMatrix[0][1], 300.0f));
        TestTrue(TEXT("矩阵[1][0]应为 |B-C|=100"), FMath::IsNearlyEqual(SecondMatrix[1][0], 100.0f));
        TestTrue(TEXT("矩阵[1][1]应为 |B-D|=200"), FMath::IsNearlyEqual(SecondMatrix[1][1], 200.0f));
    }

    // 第三次调用：与第二次完全相同 → 走正常缓存命中路径，结果仍应正确（防修复退化为禁用缓存）
    const TArray<TArray<float>> ThirdMatrix = Manager->CreateCostMatrix(SecondFrom, SecondTo, false);
    TestEqual(TEXT("相同输入重复调用应保持 2 行成本矩阵"), ThirdMatrix.Num(), 2);
    if (ThirdMatrix.Num() == 2)
    {
        TestEqual(TEXT("相同输入重复调用每行应有 2 列"), ThirdMatrix[0].Num(), 2);
        TestTrue(TEXT("相同输入重复调用矩阵内容应一致"),
            ThirdMatrix == SecondMatrix);
    }

    return true;
}

#endif
