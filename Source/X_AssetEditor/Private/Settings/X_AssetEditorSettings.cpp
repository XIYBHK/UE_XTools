/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "Settings/X_AssetEditorSettings.h"
#include "AssetNaming/X_AssetNamingManager.h"
#include "X_AssetEditor.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "X_AssetEditorSettings"

UX_AssetEditorSettings::UX_AssetEditorSettings()
	: PluginLogVerbosity(EXToolsLogVerbosity::Warning)
	, bAutoRenameOnImport(true)
	, bAutoRenameOnCreate(true)
	, bAutoFixupRedirectors(true)
	// 子系统开关默认值
	, bEnableObjectPoolSubsystem(false)
	, bEnableEnhancedCodeFlowSubsystem(true)
	, bEnableBlueprintLibraryCleanup(true)
{
	// 默认排除关卡地图
	ExcludedAssetClasses.Add(TEXT("World"));

	// 注意：自动重命名功能仅处理 /Game/ 路径下的项目内容
	// 所有引擎内容和插件内容已在代码层面自动排除
	// ExcludedFolders 用于在 /Game/ 内排除特定文件夹，例如：
	// ExcludedFolders.Add(TEXT("/Game/ThirdParty/"));

	// 初始化内置前缀规则（用户可在设置中修改）
	InitializeDefaultPrefixMappings();
}

void UX_AssetEditorSettings::InitializeDefaultPrefixMappings()
{
	// 只在Map为空时初始化默认值
	// 如果用户已经有配置（即使删除了部分条目），也不会自动补全
	// 用户可以通过删除配置文件来重置为默认值
	if (AssetPrefixMappings.Num() > 0 && ParentClassPrefixMappings.Num() > 0)
	{
		return;  // 已有配置，不覆盖用户修改
	}

	// ========== 初始化资产前缀映射 ==========
	if (AssetPrefixMappings.Num() == 0)
	{
		InitializeAssetPrefixMappings();
	}

	// ========== 初始化父类前缀映射 ==========
	if (ParentClassPrefixMappings.Num() == 0)
	{
		InitializeParentClassPrefixMappings();
	}
}

void UX_AssetEditorSettings::InitializeAssetPrefixMappings()
{

	// ========== 核心与通用 ==========
	AssetPrefixMappings.Add(TEXT("Blueprint"), TEXT("BP_"));
	AssetPrefixMappings.Add(TEXT("World"), TEXT("Map_"));

	// ========== 网格体与几何体 ==========
	AssetPrefixMappings.Add(TEXT("StaticMesh"), TEXT("SM_"));
	AssetPrefixMappings.Add(TEXT("SkeletalMesh"), TEXT("SK_"));
	AssetPrefixMappings.Add(TEXT("GeometryCollection"), TEXT("GC_"));
	AssetPrefixMappings.Add(TEXT("DestructibleMesh"), TEXT("DM_"));
	AssetPrefixMappings.Add(TEXT("ProceduralMeshComponent"), TEXT("PMC_"));

	// ========== 物理 ==========
	AssetPrefixMappings.Add(TEXT("PhysicsAsset"), TEXT("PHYS_"));
	AssetPrefixMappings.Add(TEXT("PhysicalMaterial"), TEXT("PM_"));
	AssetPrefixMappings.Add(TEXT("Skeleton"), TEXT("SKEL_"));

	// ========== Chaos 物理 ==========
	AssetPrefixMappings.Add(TEXT("ChaosCacheCollection"), TEXT("CC_"));
	AssetPrefixMappings.Add(TEXT("ChaosPhysicalMaterial"), TEXT("CPM_"));

	// ========== 材质与纹理 ==========
	AssetPrefixMappings.Add(TEXT("Material"), TEXT("M_"));
	AssetPrefixMappings.Add(TEXT("MaterialInstanceConstant"), TEXT("MI_"));
	AssetPrefixMappings.Add(TEXT("MaterialInstanceDynamic"), TEXT("MID_"));
	AssetPrefixMappings.Add(TEXT("MaterialFunction"), TEXT("MF_"));
	AssetPrefixMappings.Add(TEXT("MaterialFunctionInstance"), TEXT("MFI_"));
	AssetPrefixMappings.Add(TEXT("MaterialFunctionMaterialLayer"), TEXT("ML_"));
	AssetPrefixMappings.Add(TEXT("MaterialParameterCollection"), TEXT("MPC_"));
	AssetPrefixMappings.Add(TEXT("SubsurfaceProfile"), TEXT("SSP_"));
	AssetPrefixMappings.Add(TEXT("SpecularProfile"), TEXT("SP_"));
	AssetPrefixMappings.Add(TEXT("MaterialLayer"), TEXT("ML_"));
	AssetPrefixMappings.Add(TEXT("Texture2D"), TEXT("T_"));
	AssetPrefixMappings.Add(TEXT("TextureCube"), TEXT("TC_"));
	AssetPrefixMappings.Add(TEXT("TextureRenderTarget2D"), TEXT("RT_"));
	AssetPrefixMappings.Add(TEXT("TextureRenderTargetCube"), TEXT("RTC_"));
	AssetPrefixMappings.Add(TEXT("VolumeTexture"), TEXT("VT_"));
	AssetPrefixMappings.Add(TEXT("MediaTexture"), TEXT("MT_"));

	// ========== UI ==========
	AssetPrefixMappings.Add(TEXT("WidgetBlueprint"), TEXT("WBP_"));
	AssetPrefixMappings.Add(TEXT("Font"), TEXT("Font_"));
	AssetPrefixMappings.Add(TEXT("FontFace"), TEXT("FontFace_"));
	AssetPrefixMappings.Add(TEXT("SlateWidgetStyle"), TEXT("Style_"));
	AssetPrefixMappings.Add(TEXT("SlateBrushAsset"), TEXT("Brush_"));
	AssetPrefixMappings.Add(TEXT("ButtonStyleAsset"), TEXT("ButtonStyle_"));
	AssetPrefixMappings.Add(TEXT("CheckboxStyleAsset"), TEXT("CheckboxStyle_"));

	// ========== 数据与配置 ==========
	AssetPrefixMappings.Add(TEXT("DataTable"), TEXT("DT_"));
	AssetPrefixMappings.Add(TEXT("CurveTable"), TEXT("CT_"));
	AssetPrefixMappings.Add(TEXT("CompositeDataTable"), TEXT("CDT_"));
	AssetPrefixMappings.Add(TEXT("CompositeCurveTable"), TEXT("CCT_"));
	AssetPrefixMappings.Add(TEXT("CurveFloat"), TEXT("Curve_"));
	AssetPrefixMappings.Add(TEXT("CurveVector"), TEXT("CurveVec_"));
	AssetPrefixMappings.Add(TEXT("CurveLinearColor"), TEXT("CurveColor_"));
	AssetPrefixMappings.Add(TEXT("UserDefinedStruct"), TEXT("S_"));
	AssetPrefixMappings.Add(TEXT("UserDefinedEnum"), TEXT("E_"));
	AssetPrefixMappings.Add(TEXT("DataAsset"), TEXT("DA_"));
	AssetPrefixMappings.Add(TEXT("PrimaryDataAsset"), TEXT("PDA_"));
	AssetPrefixMappings.Add(TEXT("PrimaryAssetLabel"), TEXT("PAL_"));

	// ========== 音频 ==========
	AssetPrefixMappings.Add(TEXT("SoundCue"), TEXT("SC_"));
	AssetPrefixMappings.Add(TEXT("SoundWave"), TEXT("SW_"));
	AssetPrefixMappings.Add(TEXT("SoundAttenuation"), TEXT("SA_"));
	AssetPrefixMappings.Add(TEXT("SoundClass"), TEXT("SCL_"));
	AssetPrefixMappings.Add(TEXT("SoundMix"), TEXT("SMix_"));
	AssetPrefixMappings.Add(TEXT("ReverbEffect"), TEXT("Reverb_"));
	AssetPrefixMappings.Add(TEXT("DialogueWave"), TEXT("DW_"));
	AssetPrefixMappings.Add(TEXT("DialogueVoice"), TEXT("DV_"));
	AssetPrefixMappings.Add(TEXT("SoundSubmix"), TEXT("SS_"));
	AssetPrefixMappings.Add(TEXT("SoundEffectSourcePreset"), TEXT("SESP_"));
	AssetPrefixMappings.Add(TEXT("SoundEffectSourcePresetChain"), TEXT("SESPC_"));
	AssetPrefixMappings.Add(TEXT("SoundEffectSubmixPreset"), TEXT("SESMP_"));
	AssetPrefixMappings.Add(TEXT("AudioBus"), TEXT("AB_"));
	AssetPrefixMappings.Add(TEXT("SoundConcurrency"), TEXT("SCon_"));

	// ========== 粒子与特效 (Cascade) ==========
	AssetPrefixMappings.Add(TEXT("ParticleSystem"), TEXT("PS_"));

	// ========== Niagara 特效系统 ==========
	AssetPrefixMappings.Add(TEXT("NiagaraSystem"), TEXT("NS_"));
	AssetPrefixMappings.Add(TEXT("NiagaraEmitter"), TEXT("NE_"));
	AssetPrefixMappings.Add(TEXT("NiagaraParameterCollection"), TEXT("NPC_"));
	AssetPrefixMappings.Add(TEXT("NiagaraEffectType"), TEXT("NET_"));

	// ========== AI ==========
	AssetPrefixMappings.Add(TEXT("BehaviorTree"), TEXT("BT_"));
	AssetPrefixMappings.Add(TEXT("BlackboardData"), TEXT("BB_"));
	AssetPrefixMappings.Add(TEXT("EnvironmentQuery"), TEXT("EQS_"));
	AssetPrefixMappings.Add(TEXT("EnvQuery"), TEXT("EQS_"));

	// ========== 动画 ==========
	AssetPrefixMappings.Add(TEXT("AnimBlueprint"), TEXT("ABP_"));
	AssetPrefixMappings.Add(TEXT("AnimSequence"), TEXT("A_"));
	AssetPrefixMappings.Add(TEXT("AnimMontage"), TEXT("AM_"));
	AssetPrefixMappings.Add(TEXT("AnimComposite"), TEXT("ACmp_"));  // 修改：避免与 BPAC_ (Blueprint Actor Component) 冲突
	AssetPrefixMappings.Add(TEXT("BlendSpace"), TEXT("BS_"));
	AssetPrefixMappings.Add(TEXT("BlendSpace1D"), TEXT("BS1D_"));
	AssetPrefixMappings.Add(TEXT("AimOffsetBlendSpace"), TEXT("AO_"));
	AssetPrefixMappings.Add(TEXT("AimOffsetBlendSpace1D"), TEXT("AO1D_"));
	AssetPrefixMappings.Add(TEXT("PoseAsset"), TEXT("Pose_"));
	AssetPrefixMappings.Add(TEXT("ControlRig"), TEXT("CR_"));
	AssetPrefixMappings.Add(TEXT("MirrorDataTable"), TEXT("MDT_"));
	AssetPrefixMappings.Add(TEXT("AnimDataModel"), TEXT("ADM_"));
	AssetPrefixMappings.Add(TEXT("BoneMaskFilter"), TEXT("BMF_"));
	AssetPrefixMappings.Add(TEXT("VariableFrameStrippingSettings"), TEXT("VFSS_"));
	
	// ========== 蒙太奇通知资产 ==========
	AssetPrefixMappings.Add(TEXT("AnimNotify"), TEXT("AN_"));
	AssetPrefixMappings.Add(TEXT("AnimNotifyState"), TEXT("ANS_"));

	// ========== MetaHuman ==========
	AssetPrefixMappings.Add(TEXT("MetaHumanIdentity"), TEXT("MHI_"));
	AssetPrefixMappings.Add(TEXT("Groom"), TEXT("Groom_"));
	AssetPrefixMappings.Add(TEXT("GroomAsset"), TEXT("Groom_"));
	AssetPrefixMappings.Add(TEXT("GroomCache"), TEXT("GC_"));
	AssetPrefixMappings.Add(TEXT("GroomBindingAsset"), TEXT("GB_"));

	// ========== 相机与镜头 ==========
	AssetPrefixMappings.Add(TEXT("CameraAnim"), TEXT("CA_"));
	AssetPrefixMappings.Add(TEXT("CameraShakeBase"), TEXT("CS_"));

	// ========== 媒体与视频 ==========
	AssetPrefixMappings.Add(TEXT("MediaPlayer"), TEXT("MP_"));
	AssetPrefixMappings.Add(TEXT("MediaSource"), TEXT("MS_"));
	AssetPrefixMappings.Add(TEXT("FileMediaSource"), TEXT("FMS_"));
	AssetPrefixMappings.Add(TEXT("MediaPlaylist"), TEXT("MPL_"));

	// ========== Sequencer ==========
	AssetPrefixMappings.Add(TEXT("LevelSequence"), TEXT("LS_"));
	AssetPrefixMappings.Add(TEXT("TemplateSequence"), TEXT("TS_"));
	AssetPrefixMappings.Add(TEXT("CameraAnimationSequence"), TEXT("CAS_"));

	// ========== Paper2D ==========
	AssetPrefixMappings.Add(TEXT("PaperSprite"), TEXT("SPR_"));
	AssetPrefixMappings.Add(TEXT("PaperTileSet"), TEXT("PTS_"));
	AssetPrefixMappings.Add(TEXT("PaperFlipbook"), TEXT("PFB_"));
	AssetPrefixMappings.Add(TEXT("PaperTileMap"), TEXT("PTM_"));

	// ========== 蓝图特殊类型 (通过 BlueprintType Tag 识别) ==========
	// 注意: Blueprint、BlueprintFunctionLibrary、BlueprintInterface、BlueprintMacroLibrary
	// 共享相同的 AssetClass (/Script/Engine.Blueprint)
	// 需要通过 AssetData.TagsAndValues 中的 "BlueprintType" 或 "ParentClass" 来区分
	AssetPrefixMappings.Add(TEXT("BlueprintFunctionLibrary"), TEXT("BPFL_"));
	AssetPrefixMappings.Add(TEXT("BlueprintInterface"), TEXT("BPI_"));
	AssetPrefixMappings.Add(TEXT("BlueprintMacroLibrary"), TEXT("BPML_"));
	AssetPrefixMappings.Add(TEXT("EditorUtilityBlueprint"), TEXT("EUBP_"));
	AssetPrefixMappings.Add(TEXT("EditorUtilityWidget"), TEXT("EUW_"));
	AssetPrefixMappings.Add(TEXT("EditorUtilityWidgetBlueprint"), TEXT("EUW_"));

	// ========== 输入系统 ==========
	AssetPrefixMappings.Add(TEXT("InputAction"), TEXT("IA_"));
	AssetPrefixMappings.Add(TEXT("InputMappingContext"), TEXT("IMC_"));
	AssetPrefixMappings.Add(TEXT("PlayerMappableInputConfig"), TEXT("PMIC_"));
	
	// ========== 渲染与后处理 ==========
	AssetPrefixMappings.Add(TEXT("PostProcessVolume"), TEXT("PPV_"));
	AssetPrefixMappings.Add(TEXT("LensFlare"), TEXT("LF_"));
	AssetPrefixMappings.Add(TEXT("ColorGradingLUT"), TEXT("LUT_"));
	
	// ========== 虚拟纹理 ==========
	AssetPrefixMappings.Add(TEXT("VirtualTexture"), TEXT("VT_"));
	AssetPrefixMappings.Add(TEXT("RuntimeVirtualTexture"), TEXT("RVT_"));
	AssetPrefixMappings.Add(TEXT("VirtualTextureBuilder"), TEXT("VTB_"));
	
	// ========== 稀疏体积纹理 ==========
	AssetPrefixMappings.Add(TEXT("SparseVolumeTexture"), TEXT("SVT_"));
	
	// ========== 其他常用类型 ==========
	AssetPrefixMappings.Add(TEXT("FoliageType"), TEXT("FT_"));
	AssetPrefixMappings.Add(TEXT("FoliageType_Actor"), TEXT("FT_"));
	AssetPrefixMappings.Add(TEXT("LandscapeGrassType"), TEXT("LGT_"));
	AssetPrefixMappings.Add(TEXT("SubUVAnimation"), TEXT("SubUV_"));
	AssetPrefixMappings.Add(TEXT("VectorField"), TEXT("VF_"));
	AssetPrefixMappings.Add(TEXT("TouchInterface"), TEXT("TI_"));
	AssetPrefixMappings.Add(TEXT("HapticFeedbackEffect"), TEXT("HFE_"));
}

FName UX_AssetEditorSettings::GetContainerName() const
{
	return FName("Project");
}

FName UX_AssetEditorSettings::GetCategoryName() const
{
	return FName("Plugins");
}

FName UX_AssetEditorSettings::GetSectionName() const
{
	return FName("XTools");
}

#if WITH_EDITOR
FText UX_AssetEditorSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "XTools");
}

FText UX_AssetEditorSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription", "配置 XTools 插件的设置，包括资产命名规则、子系统开关、对象池配置和调试选项。");
}

void UX_AssetEditorSettings::InitializeParentClassPrefixMappings()
{
	// ========== Framework Classes (框架类) ==========
	ParentClassPrefixMappings.Add(TEXT("GameModeBase"), TEXT("BP_GM_"));
	ParentClassPrefixMappings.Add(TEXT("GameMode"), TEXT("BP_GM_"));
	ParentClassPrefixMappings.Add(TEXT("GameStateBase"), TEXT("BP_GS_"));
	ParentClassPrefixMappings.Add(TEXT("GameState"), TEXT("BP_GS_"));
	ParentClassPrefixMappings.Add(TEXT("PlayerController"), TEXT("BP_PC_"));
	ParentClassPrefixMappings.Add(TEXT("PlayerState"), TEXT("BP_PS_"));
	ParentClassPrefixMappings.Add(TEXT("HUD"), TEXT("BP_HUD_"));
	ParentClassPrefixMappings.Add(TEXT("CheatManager"), TEXT("BP_Cheat_"));
	ParentClassPrefixMappings.Add(TEXT("PlayerCameraManager"), TEXT("BP_PCM_"));

	// ========== Component Classes (组件类) ==========
	ParentClassPrefixMappings.Add(TEXT("SceneComponent"), TEXT("BPSC_"));
	ParentClassPrefixMappings.Add(TEXT("ActorComponent"), TEXT("BPAC_"));
	ParentClassPrefixMappings.Add(TEXT("StaticMeshComponent"), TEXT("BPSMC_"));
	ParentClassPrefixMappings.Add(TEXT("SkeletalMeshComponent"), TEXT("BPSKC_"));
	ParentClassPrefixMappings.Add(TEXT("CameraComponent"), TEXT("BPCamera_"));
	ParentClassPrefixMappings.Add(TEXT("SpringArmComponent"), TEXT("BPSpringArm_"));
	ParentClassPrefixMappings.Add(TEXT("LightComponent"), TEXT("BPLight_"));
	ParentClassPrefixMappings.Add(TEXT("AudioComponent"), TEXT("BPAudio_"));
	ParentClassPrefixMappings.Add(TEXT("ParticleSystemComponent"), TEXT("BPPSC_"));
	ParentClassPrefixMappings.Add(TEXT("WidgetComponent"), TEXT("BPWidget_"));

	// ========== AI Classes (AI类) ==========
	ParentClassPrefixMappings.Add(TEXT("AIController"), TEXT("BP_AIC_"));
	ParentClassPrefixMappings.Add(TEXT("BTTask"), TEXT("BP_BTTask_"));
	ParentClassPrefixMappings.Add(TEXT("BTDecorator"), TEXT("BP_BTDec_"));
	ParentClassPrefixMappings.Add(TEXT("BTService"), TEXT("BP_BTServ_"));

	// ========== Animation & UI Classes (动画与UI类) ==========
	ParentClassPrefixMappings.Add(TEXT("AnimLayerInterface"), TEXT("ABPI_"));  // 更具体的接口，优先匹配
	ParentClassPrefixMappings.Add(TEXT("/Script/Engine.AnimLayerInterface"), TEXT("ABPI_"));  // 完整路径匹配
	ParentClassPrefixMappings.Add(TEXT("AnimInstance"), TEXT("ABP_"));
	ParentClassPrefixMappings.Add(TEXT("AnimNotify"), TEXT("AN_"));
	ParentClassPrefixMappings.Add(TEXT("AnimNotify_PlayMontageNotify"), TEXT("AMN_"));  // 蒙太奇通知专用前缀
	ParentClassPrefixMappings.Add(TEXT("AnimGraphRuntime.AnimNotify_PlayMontageNotify"), TEXT("AMN_"));
	ParentClassPrefixMappings.Add(TEXT("AnimNotifyState"), TEXT("ANS_"));
	ParentClassPrefixMappings.Add(TEXT("UserWidget"), TEXT("WBP_"));

	// ========== Save Game (存档类) ==========
	ParentClassPrefixMappings.Add(TEXT("SaveGame"), TEXT("BP_SG_"));

	// ========== Subsystem Classes (子系统类) ==========
	ParentClassPrefixMappings.Add(TEXT("GameInstanceSubsystem"), TEXT("BP_GIS_"));
	ParentClassPrefixMappings.Add(TEXT("WorldSubsystem"), TEXT("BP_WS_"));
	ParentClassPrefixMappings.Add(TEXT("LocalPlayerSubsystem"), TEXT("BP_LPS_"));

	// ========== Actor Classes (Actor类) ==========
	ParentClassPrefixMappings.Add(TEXT("TriggerVolume"), TEXT("BP_TV_"));
	ParentClassPrefixMappings.Add(TEXT("TriggerBox"), TEXT("BP_TB_"));
	ParentClassPrefixMappings.Add(TEXT("TriggerSphere"), TEXT("BP_TS_"));
}

void UX_AssetEditorSettings::ApplyPluginLogVerbosity()
{
	// XTools 插件所有日志分类列表
	TArray<FName> XToolsLogCategories = {
		TEXT("LogXTools"),
		TEXT("LogX_AssetEditor"),
		TEXT("LogX_AssetNaming"),
		TEXT("LogX_AssetNamingDelegates"),
		TEXT("LogSort"),
		TEXT("LogRandomShuffles"),
		TEXT("LogEnhancedCodeFlow"),
		TEXT("LogPointSampling"),
		TEXT("LogFormationSystem"),
		TEXT("LogComponentTimeline"),
		TEXT("LogBlueprintExtensions"),
		TEXT("LogObjectPool"),
	};

	// 转换枚举值为 ELogVerbosity::Type
	ELogVerbosity::Type VerbosityLevel = static_cast<ELogVerbosity::Type>(PluginLogVerbosity);

	// 获取日志级别字符串
	const TCHAR* VerbosityString = ToString(VerbosityLevel);

	// 应用日志级别到所有分类
	for (const FName& LogCategory : XToolsLogCategories)
	{
		FString Command = FString::Printf(TEXT("Log %s %s"), *LogCategory.ToString(), VerbosityString);

		// 使用 GEngine 执行控制台命令
		if (GEngine)
		{
			GEngine->Exec(nullptr, *Command);
		}
	}

	UE_LOG(LogX_AssetEditor, Log, TEXT("已应用 XTools 插件日志级别: %s"), VerbosityString);
}

void UX_AssetEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// 检查是否修改了日志级别设置
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UX_AssetEditorSettings, PluginLogVerbosity))
	{
		ApplyPluginLogVerbosity();
	}

	// 检查是否修改了自动重命名相关设置
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UX_AssetEditorSettings, bAutoRenameOnImport) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UX_AssetEditorSettings, bAutoRenameOnCreate))
	{
		// 通知 Manager 重新初始化委托绑定
		if (FModuleManager::Get().IsModuleLoaded("X_AssetEditor"))
		{
			FX_AssetNamingManager& Manager = FX_AssetNamingManager::Get();
			Manager.RefreshDelegateBindings();
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
