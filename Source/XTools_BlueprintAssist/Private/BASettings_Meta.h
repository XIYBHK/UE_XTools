// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "BASettings_Meta.generated.h"

/**
 *
 */
UCLASS(config = EditorPerProjectUserSettings, DisplayName = "BA设置 配置文件")
class XTOOLS_BLUEPRINTASSIST_API UBASettings_Meta : public UBASettingsBase
{
	GENERATED_BODY()

public:
	UBASettings_Meta(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, config, Category = "Default", meta = (FilePathFilter = "ini", RelativeToGameDir, ConfigRestartRequired = "true", DisplayName = "自定义设置文件", ToolTip = "选择用于加载和保存 Blueprint Assist 设置的 ini 文件，修改后需要重启编辑器"))
	FFilePath CustomSettingsIniPath;

	FORCEINLINE static const UBASettings_Meta& Get() { return *GetDefault<UBASettings_Meta>(); }
	FORCEINLINE static UBASettings_Meta& GetMutable() { return *GetMutableDefault<UBASettings_Meta>(); }
};
