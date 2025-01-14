/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
 

#include "RandomShuffles.h"
#include "Modules/ModuleManager.h"

void FRandomShufflesModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("RandomShuffles module started!"));
}

void FRandomShufflesModule::ShutdownModule()
{
	UE_LOG(LogTemp, Warning, TEXT("RandomShuffles module shut down!"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRandomShufflesModule, RandomShuffles)