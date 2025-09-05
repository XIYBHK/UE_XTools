// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollisionTools/X_CoACDAdapter.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

namespace
{
    void* GCoACD_DLL = nullptr;
    TCoACD_Run GCoACD_Run = nullptr;
    TCoACD_Free GCoACD_Free = nullptr;

    static FString FindCoACDDll()
    {
        TArray<FString> Candidates;
        if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE_XTools")))
        {
            const FString Base = Plugin->GetBaseDir();
            Candidates.Add(FPaths::Combine(Base, TEXT("ThirdParty/CoACD/DLL/lib_coacd.dll")));
        }
        Candidates.Add(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UE_XTools/ThirdParty/CoACD/DLL/lib_coacd.dll")));
        Candidates.Add(TEXT("lib_coacd.dll"));

        for (const FString& P : Candidates)
        {
            if (FPaths::FileExists(P))
            {
                return P;
            }
        }
        return FString();
    }
}

namespace CoACD
{
    bool Initialize()
    {
#if PLATFORM_WINDOWS
        if (GCoACD_Run)
        {
            return true;
        }
        const FString DllPath = FindCoACDDll();
        GCoACD_DLL = FPlatformProcess::GetDllHandle(*DllPath);
        if (!GCoACD_DLL)
        {
            return false;
        }
        GCoACD_Run = (TCoACD_Run)FPlatformProcess::GetDllExport(GCoACD_DLL, TEXT("CoACD_run"));
        GCoACD_Free = (TCoACD_Free)FPlatformProcess::GetDllExport(GCoACD_DLL, TEXT("CoACD_freeMeshArray"));
        if (!GCoACD_Run || !GCoACD_Free)
        {
            Shutdown();
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    void Shutdown()
    {
#if PLATFORM_WINDOWS
        if (GCoACD_DLL)
        {
            FPlatformProcess::FreeDllHandle(GCoACD_DLL);
        }
        GCoACD_DLL = nullptr;
        GCoACD_Run = nullptr;
        GCoACD_Free = nullptr;
#endif
    }

    bool IsAvailable()
    {
        return GCoACD_Run != nullptr;
    }

    TCoACD_Run GetRun() { return GCoACD_Run; }
    TCoACD_Free GetFree() { return GCoACD_Free; }
}


