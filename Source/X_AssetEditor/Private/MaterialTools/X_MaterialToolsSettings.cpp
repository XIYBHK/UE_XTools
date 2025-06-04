#include "MaterialTools/X_MaterialToolsSettings.h"

UX_MaterialToolsSettings::UX_MaterialToolsSettings()
{
    // 默认设置
    bEnableDebugLog = false;
    bEnableParallelProcessing = true;
    ParallelBatchSize = 50;
} 