// Fill out your copyright notice in the Description page of Project Settings.

// ✅ 条件编译保护 - 只在非Shipping版本中编译测试
#if WITH_OBJECTPOOL_TESTS

// ✅ 遵循IWYU原则的头文件包含
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "ObjectPoolTypes.h"
#include "ActorPool.h"

/**
 * 验证修复后的测试管理器功能
 */
void ValidateTestObjectPoolManager()
{
    UE_LOG(LogTemp, Warning, TEXT("=== 验证测试对象池管理器 ==="));

    // ✅ 测试池创建
    TSubclassOf<AActor> TestActorClass = AActor::StaticClass();
    
    // 创建Actor池
    TSharedPtr<FActorPool> Pool = MakeShared<FActorPool>(TestActorClass, 3);
    if (Pool.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Actor池创建成功"));
        
        // 预热池
        if (GWorld)
        {
            Pool->PrewarmPool(GWorld, 3);
            UE_LOG(LogTemp, Log, TEXT("  - 池预热完成"));
            
            // 获取统计信息
            FObjectPoolStats Stats = Pool->GetStats();
            UE_LOG(LogTemp, Log, TEXT("  - 池大小: %d"), Stats.PoolSize);
            UE_LOG(LogTemp, Log, TEXT("  - 可用数量: %d"), Stats.CurrentAvailable);
            UE_LOG(LogTemp, Log, TEXT("  - 活跃数量: %d"), Stats.CurrentActive);
            
            // 测试获取Actor
            AActor* TestActor = Pool->GetActor(GWorld);
            if (TestActor)
            {
                UE_LOG(LogTemp, Warning, TEXT("✅ 从池中获取Actor成功"));
                
                // 获取更新后的统计信息
                FObjectPoolStats StatsAfterGet = Pool->GetStats();
                UE_LOG(LogTemp, Log, TEXT("  - 获取后可用数量: %d"), StatsAfterGet.CurrentAvailable);
                UE_LOG(LogTemp, Log, TEXT("  - 获取后活跃数量: %d"), StatsAfterGet.CurrentActive);
                
                // 测试归还Actor
                bool bReturned = Pool->ReturnActor(TestActor);
                if (bReturned)
                {
                    UE_LOG(LogTemp, Warning, TEXT("✅ 归还Actor到池成功"));
                    
                    // 获取归还后的统计信息
                    FObjectPoolStats StatsAfterReturn = Pool->GetStats();
                    UE_LOG(LogTemp, Log, TEXT("  - 归还后可用数量: %d"), StatsAfterReturn.CurrentAvailable);
                    UE_LOG(LogTemp, Log, TEXT("  - 归还后活跃数量: %d"), StatsAfterReturn.CurrentActive);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ 归还Actor失败"));
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ 从池中获取Actor失败"));
            }
            
            // 清理池
            Pool->ClearPool();
            UE_LOG(LogTemp, Log, TEXT("  - 池清理完成"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ GWorld不可用，无法预热池"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Actor池创建失败"));
    }

    UE_LOG(LogTemp, Warning, TEXT("=== 测试对象池管理器验证完成 ==="));
}

// ✅ 在模块启动时自动运行验证
static class FTestValidationRunner
{
public:
    FTestValidationRunner()
    {
        // 延迟执行验证，确保引擎完全初始化
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime) -> bool
        {
            ValidateTestObjectPoolManager();
            return false; // 只执行一次
        }), 3.0f); // 3秒后执行，在基础测试之后
    }
} GTestValidationRunner;

#endif // WITH_OBJECTPOOL_TESTS
