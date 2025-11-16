#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BlueprintAssistSettings_Advanced.generated.h"

UENUM()
enum class EBACrashReportingMethod : uint8
{
	Ask UMETA(DisplayName = "Ask", Tooltip = "Always ask when finding unsent crash reports"),
	Never UMETA(DisplayName = "Never send", Tooltip = "Don't check for crash reports"),
};

UCLASS(config = EditorPerProjectUserSettings)
class XTOOLS_BLUEPRINTASSIST_API UBASettings_Advanced final : public UObject
{
	GENERATED_BODY()

public:
	UBASettings_Advanced(const FObjectInitializer& ObjectInitializer);

	/* If swapping produced any looping wires, remove them */
	UPROPERTY(EditAnywhere, config, Category = "Commands|Swap Nodes", meta = (DisplayName = "移除交换导致的循环连线", Tooltip = "如果交换节点产生了循环连线，则移除它们"))
	bool bRemoveLoopingCausedBySwapping;

	UPROPERTY(EditAnywhere, config, Category = "Commands", meta = (DisplayName = "禁用的命令", Tooltip = "禁用这些命令"))
	TSet<FName> DisabledCommands;

	/* Potential issue where pins can get stuck in a hovered state on the material graph */
	UPROPERTY(EditAnywhere, config, Category = "Material Graph|Experimental", meta = (DisplayName = "启用材质图表引脚悬停修复", Tooltip = "修复材质图表中引脚可能卡在悬停状态的潜在问题"))
	bool bEnableMaterialGraphPinHoverFix;

	/* Fix for issue where copy-pasting material nodes will result in their material expressions having the same GUID */
	UPROPERTY(EditAnywhere, config, Category = "Material Graph|Experimental", DisplayName="为材质表达式生成唯一GUID", meta = (Tooltip = "修复复制粘贴材质节点导致材质表达式具有相同 GUID 的问题"))
	bool bGenerateUniqueGUIDForMaterialExpressions;

	/* Instead of making a json file to store cache data, store it in the blueprint's package meta data */
	UPROPERTY(EditAnywhere, config, Category = "Cache|Experimental", meta = (DisplayName = "在包元数据中存储缓存数据", Tooltip = "不创建 JSON 文件存储缓存数据，而是存储在蓝图的包元数据中"))
	bool bStoreCacheDataInPackageMetaData;

	/* Save cache file JSON in a more human-readable format. Useful for debugging, but increases size of cache files.  */
	UPROPERTY(EditAnywhere, config, Category = "Cache", meta = (DisplayName = "美化缓存JSON", Tooltip = "以更易读的格式保存缓存文件 JSON，便于调试，但会增加缓存文件大小"))
	bool bPrettyPrintCacheJSON;

	/* Use a custom blueprint action menu for creating nodes (very prototype, not supported in 5.0 or earlier) */
	UPROPERTY(EditAnywhere, config, Category = "Misc|Experimental", meta = (DisplayName = "使用自定义蓝图操作菜单", Tooltip = "使用自定义蓝图操作菜单创建节点（非常原型，不支持 5.0 或更早版本）"))
	bool bUseCustomBlueprintActionMenu;

	/* Hacky workaround to ensure that default comment nodes will be correctly resized after formatting */
	UPROPERTY(EditAnywhere, config, Category = "Misc|Experimental", meta = (DisplayName = "格式化后强制刷新图表", Tooltip = "确保默认注释节点在格式化后正确调整大小的临时解决方案"))
	bool bForceRefreshGraphAfterFormatting;

	/** Include a copy of the node graph used when you crashed while formatting */
	UPROPERTY(config)
	bool bIncludeNodesInCrashReport;

	/** Include your Blueprint Assist formatting settings */
	UPROPERTY(config)
	bool bIncludeSettingsInCrashReport;

	/** Determines what to do with Blueprint Assist crash reports when launching the editor */
	UPROPERTY(EditAnywhere, config, Category = "Crash Reporter")
	EBACrashReportingMethod CrashReportingMethod = EBACrashReportingMethod::Ask;

	FORCEINLINE static const UBASettings_Advanced& Get() { return *GetDefault<UBASettings_Advanced>(); }
	FORCEINLINE static UBASettings_Advanced& GetMutable() { return *GetMutableDefault<UBASettings_Advanced>(); }
};
