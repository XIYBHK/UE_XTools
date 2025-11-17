#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "UObject/Object.h"
#include "BlueprintAssistSettings_Advanced.generated.h"

#define BA_DEBUG_EARLY_EXIT(string) do { if (UBASettings_Advanced::HasDebugSetting(string)) return; } while(0)
#define BA_DEBUG(string) GetDefault<UBASettings_Advanced>()->BlueprintAssistDebug.Contains(string)

UENUM(meta = (ToolTip = "缓存保存位置"))
enum class EBACacheSaveLocation : uint8
{
	/** Save to PluginFolder/NodeSizeCache/PROJECT_ID.json */
	Plugin UMETA(DisplayName = "插件文件夹"),

	/** Save to ProjectFolder/Saved/BlueprintAssist/BlueprintAssistCache.json */
	Project UMETA(DisplayName = "项目文件夹"),
};

UENUM(meta = (ToolTip = "崩溃报告方式"))
enum class EBACrashReportingMethod : uint8
{
	Ask UMETA(DisplayName = "询问"),
	Never UMETA(DisplayName = "从不发送"),
	// Always UMETA(DisplayName = "Always send", Tooltip = "Don't ask and send new crash reports"),
};

UCLASS(config = EditorPerProjectUserSettings, DisplayName = "BA设置 高级")
class XTOOLS_BLUEPRINTASSIST_API UBASettings_Advanced final : public UBASettingsBase
{
	GENERATED_BODY()

public:
	UBASettings_Advanced(const FObjectInitializer& ObjectInitializer);

	////////////////////////////////////////////////////////////
	/// Cache
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = "Cache", meta = (DisplayName = "缓存保存位置", Tooltip = "选择节点大小缓存的保存位置"))
	EBACacheSaveLocation CacheSaveLocation;

	/* Save the node size cache to a file (located in the the plugin folder) */
	UPROPERTY(EditAnywhere, config, Category = "Cache", meta = (DisplayName = "保存缓存到文件", Tooltip = "将节点大小缓存保存到文件(位于插件文件夹)"))
	bool bSaveBlueprintAssistCacheToFile;

	/* Enable slower but more accurate node size caching */
	UPROPERTY(EditAnywhere, config, Category = "Cache", meta = (DisplayName = "启用精确缓存", Tooltip = "启用较慢但更精确的节点大小缓存"))
	bool bSlowButAccurateSizeCaching;

	/* If swapping produced any looping wires, remove them */
	UPROPERTY(EditAnywhere, config, Category = "Commands|Swap Nodes", meta = (DisplayName = "移除交换导致的循环连线", Tooltip = "如果交换节点产生了循环连线则移除它们"))
	bool bRemoveLoopingCausedBySwapping;

	UPROPERTY(EditAnywhere, config, Category = "Commands", meta = (DisplayName = "禁用的命令", Tooltip = "禁用这些命令"))
	TSet<FName> DisabledCommands;

	/* Fix for issue where copy-pasting material nodes will result in their material expressions having the same GUID */
	UPROPERTY(EditAnywhere, config, Category = "Material Graph|Experimental", meta = (DisplayName = "为材质表达式生成唯一GUID", Tooltip = "修复复制粘贴材质节点时材质表达式具有相同GUID的问题"))
	bool bGenerateUniqueGUIDForMaterialExpressions;

	/* Instead of making a json file to store cache data, store it in the blueprint's package meta data */
	UPROPERTY(EditAnywhere, config, Category = "Cache|Experimental", meta = (DisplayName = "在包元数据中存储缓存", Tooltip = "不创建json文件,而是将缓存数据存储在蓝图的包元数据中"))
	bool bStoreCacheDataInPackageMetaData;

	/* Save cache file JSON in a more human-readable format. Useful for debugging, but increases size of cache files.  */
	UPROPERTY(EditAnywhere, config, Category = "Cache", meta = (DisplayName = "美化缓存JSON", Tooltip = "以更易读的格式保存缓存JSON文件,适用于调试,但会增加缓存文件大小"))
	bool bPrettyPrintCacheJSON;

	/* Use a custom blueprint action menu for creating nodes (very prototype, not supported in 5.0 or earlier) */
	UPROPERTY(EditAnywhere, config, Category = "Misc|Experimental", meta = (DisplayName = "使用自定义蓝图动作菜单", Tooltip = "使用自定义蓝图动作菜单创建节点(非常原型,5.0或更早版本不支持)"))
	bool bUseCustomBlueprintActionMenu;

	/* Hacky workaround to ensure that default comment nodes will be correctly resized after formatting */
	UPROPERTY(EditAnywhere, config, Category = "Misc|Experimental", meta = (DisplayName = "格式化后强制刷新图表", Tooltip = "黑科技解决方案,确保默认注释节点在格式化后正确调整大小"))
	bool bForceRefreshGraphAfterFormatting;

	/* Disable the plugin (requires restarting engine) */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta=(ConfigRestartRequired = "true", DisplayName = "禁用插件", Tooltip = "禁用插件(需要重启引擎)"))
	bool bDisableBlueprintAssistPlugin;

	/** Ignore this (setting for custom debugging) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = "Misc", meta = (DisplayName = "调试设置", Tooltip = "忽略此项(用于自定义调试)"))
	TArray<FString> BlueprintAssistDebug;

	/** Draw a red border around bad comment nodes after formatting */
	UPROPERTY(EditAnywhere, config, Category = "Misc", meta = (DisplayName = "高亮错误注释", Tooltip = "格式化后在错误的注释节点周围绘制红色边框"))
	bool bHighlightBadComments;

	/** Determines what to do with Blueprint Assist crash reports when launching the editor */
	UPROPERTY(EditAnywhere, config, Category = "Crash Reporter", meta = (DisplayName = "崩溃报告方式", Tooltip = "确定启动编辑器时如何处理BlueprintAssist崩溃报告"))
	EBACrashReportingMethod CrashReportingMethod = EBACrashReportingMethod::Ask;

	/** When crashing during formatting, the related nodes will be written to Saved/Crashes/BACrashData */
	UPROPERTY(EditAnywhere, config, Category = "Crash Reporter", meta = (DisplayName = "转储格式化崩溃节点", Tooltip = "格式化时崩溃时,相关节点将被写入Saved/Crashes/BACrashData"))
	bool bDumpFormattingCrashNodes;

	/** Include a copy of the node graph used when you crashed while formatting */
	UPROPERTY(config)
	bool bIncludeNodesInCrashReport;

	/** Include your Blueprint Assist formatting settings */
	UPROPERTY(config)
	bool bIncludeSettingsInCrashReport;

	static FORCEINLINE bool HasDebugSetting(const FString& Setting)
	{
		return Get().BlueprintAssistDebug.Contains(Setting);
	}

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	FORCEINLINE static const UBASettings_Advanced& Get() { return *GetDefault<UBASettings_Advanced>(); }
	FORCEINLINE static UBASettings_Advanced& GetMutable() { return *GetMutableDefault<UBASettings_Advanced>(); }
};

class FBASettingsDetails_Advanced final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
