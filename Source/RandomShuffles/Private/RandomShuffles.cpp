/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#include "RandomShuffles.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "RandomShuffleArrayLibrary.h"

void FRandomShufflesModule::StartupModule()
{
	// 注册世界清理委托：当最后一个 Game/PIE 世界随会话结束销毁时自动清空 PRD 状态，
	// 避免静态 PRDStateMap 跨 PIE 会话、跨关卡残留（M-7）
	PRDWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&URandomShuffleArrayLibrary::HandleWorldCleanup);
}

void FRandomShufflesModule::ShutdownModule()
{
	// 模块卸载（含 Live Coding 热重载）时必须移除委托，避免悬空回调
	if (PRDWorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(PRDWorldCleanupHandle);
		PRDWorldCleanupHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRandomShufflesModule, RandomShuffles)