// Copyright 1998-2023 Epic Games, Inc. All Rights Reserved.

#include "X_AssetEditor.h"
#include "MaterialTools/X_MaterialFunctionParamDialog.h"
#include "MaterialTools/X_MaterialFunctionManager.h"
#include "MaterialTools/X_MaterialFunctionParams.h"
#include "MaterialTools/X_MaterialFunctionOperation.h"

// 核心头文件
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "ISettingsModule.h"
#include "Misc/MessageDialog.h"

// 材质工具相关
#include "MaterialTools/X_MaterialFunctionOperation.h"
#include "MaterialTools/X_MaterialToolsSettings.h"

// 资产相关
#include "EditorUtilityLibrary.h"
#include "ISettingsContainer.h"
#include "ISettingsSection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

// 移除所有游戏框架和资产类型相关的头文件
    // 移除 GameFramework, Components, Animation, Blueprint, Materials 等相关头文件

DEFINE_LOG_CATEGORY(LogX_AssetEditor);

IMPLEMENT_MODULE(FX_AssetEditorModule, X_AssetEditor);

#define LOCTEXT_NAMESPACE "X_AssetEditor"

// 定义资产命名规范相关的前缀
TMap<FString, FString> CreateAssetPrefixMap()
{
	TMap<FString, FString> PrefixMap;
	
	// --- 核心与通用 ---
	PrefixMap.Add(TEXT("Blueprint"), TEXT("BP_"));             // 通用蓝图默认前缀
	PrefixMap.Add(TEXT("World"), TEXT("Map_"));               // 关卡/地图
	
	// --- 网格体与几何体 ---
	PrefixMap.Add(TEXT("StaticMesh"), TEXT("SM_"));           // 静态网格体
	PrefixMap.Add(TEXT("SkeletalMesh"), TEXT("SK_"));         // 骨骼网格体
	PrefixMap.Add(TEXT("GeometryCollection"), TEXT("GC_"));   // 几何集合
	PrefixMap.Add(TEXT("GeometryCollectionCache"), TEXT("GCC_"));   // 几何集合缓存
	PrefixMap.Add(TEXT("ChaosCacheCollection"), TEXT("CCC_"));   // Chaos缓存集合
	PrefixMap.Add(TEXT("PhysicsAsset"), TEXT("PHYS_"));       // 物理资产
	PrefixMap.Add(TEXT("PhysicalMaterial"), TEXT("PM_"));     // 物理材质
	PrefixMap.Add(TEXT("Skeleton"), TEXT("SKEL_"));           // 骨架
	
	// --- 材质与纹理 ---
	PrefixMap.Add(TEXT("Material"), TEXT("M_"));              // 材质
	PrefixMap.Add(TEXT("MaterialInstanceConstant"), TEXT("MI_")); // 材质实例
	PrefixMap.Add(TEXT("MaterialFunction"), TEXT("MF_"));     // 材质函数
	PrefixMap.Add(TEXT("MaterialFunctionInstance"), TEXT("MFI_")); // 材质函数实例
	PrefixMap.Add(TEXT("MaterialParameterCollection"), TEXT("MPC_")); // 材质参数集
	PrefixMap.Add(TEXT("SubsurfaceProfile"), TEXT("SSP_"));   // 次表面配置文件
	PrefixMap.Add(TEXT("TextureRenderTarget2D"), TEXT("RT_")); // 渲染目标纹理2D
	PrefixMap.Add(TEXT("TextureRenderTarget2DArray"), TEXT("RTA_")); // 渲染目标纹理数组
	PrefixMap.Add(TEXT("RenderTarget2DArray"), TEXT("RTA_")); // 渲染目标纹理数组（简化名）
	PrefixMap.Add(TEXT("TextureRenderTargetCube"), TEXT("RTC_")); // 渲染目标立方体纹理
	PrefixMap.Add(TEXT("TextureRenderTargetVolume"), TEXT("RTV_")); // 渲染目标体积纹理
	PrefixMap.Add(TEXT("Texture2D"), TEXT("T_"));              // 2D 纹理
	PrefixMap.Add(TEXT("TextureCube"), TEXT("TC_"));           // 立方体纹理
	PrefixMap.Add(TEXT("Texture2DArray"), TEXT("TA_"));        // 2D 纹理数组
	PrefixMap.Add(TEXT("TextureCubeArray"), TEXT("TCA_"));     // Cube 纹理数组
	PrefixMap.Add(TEXT("VolumeTexture"), TEXT("VT_"));         // 体积纹理
	PrefixMap.Add(TEXT("MediaTexture"), TEXT("MT_"));          // 媒体纹理
	PrefixMap.Add(TEXT("RenderTarget"), TEXT("RT_"));          // 渲染目标 (通用)
	PrefixMap.Add(TEXT("CanvasRenderTarget2D"), TEXT("CRT_")); // 画布渲染目标
	PrefixMap.Add(TEXT("RuntimeVirtualTexture"), TEXT("RVT_")); // 运行时虚拟纹理
	
	// --- UI ---
	PrefixMap.Add(TEXT("WidgetBlueprint"), TEXT("WBP_"));     // 控件蓝图
	PrefixMap.Add(TEXT("SlateWidgetStyleAsset"), TEXT("Style_")); // UI 样式资产
	PrefixMap.Add(TEXT("SlateBrushAsset"), TEXT("SB_"));      // Slate 画刷资产
	PrefixMap.Add(TEXT("Font"), TEXT("Font_"));               // 字体资产
	PrefixMap.Add(TEXT("FontFace"), TEXT("Font_"));
	PrefixMap.Add(TEXT("FontAsset"), TEXT("Font_"));          // 字体资产
	
	// --- 数据与配置 ---
	PrefixMap.Add(TEXT("DataTable"), TEXT("DT_"));             // 数据表
	PrefixMap.Add(TEXT("CurveFloat"), TEXT("Curve_"));         // 浮点曲线
	PrefixMap.Add(TEXT("CurveVector"), TEXT("CurveV_"));       // 向量曲线
	PrefixMap.Add(TEXT("CurveLinearColor"), TEXT("CurveC_"));  // 颜色曲线
	PrefixMap.Add(TEXT("UserDefinedStruct"), TEXT("S_"));      // 用户定义结构体
	PrefixMap.Add(TEXT("UserDefinedEnum"), TEXT("E_"));        // 用户定义枚举
	PrefixMap.Add(TEXT("PrimaryDataAsset"), TEXT("PDA_"));     // 主要数据资产
	PrefixMap.Add(TEXT("DataAsset"), TEXT("DA_"));             // 基础数据资产
	
	// --- 音频 ---
	PrefixMap.Add(TEXT("SoundCue"), TEXT("SC_"));              // 声音提示
	PrefixMap.Add(TEXT("SoundWave"), TEXT("S_"));              // 声音波形
	PrefixMap.Add(TEXT("SoundMix"), TEXT("Mix_"));             // 声音混合
	PrefixMap.Add(TEXT("SoundClass"), TEXT("SClass_"));        // 声音类
	PrefixMap.Add(TEXT("ReverbEffect"), TEXT("Reverb_"));      // 混响效果
	
	// --- 效果 ---
	PrefixMap.Add(TEXT("ParticleSystem"), TEXT("PS_"));        // 粒子系统 (Cascade)
	PrefixMap.Add(TEXT("NiagaraSystem"), TEXT("NS_"));         // Niagara 系统
	PrefixMap.Add(TEXT("NiagaraEmitter"), TEXT("NE_"));        // Niagara 发射器
	
	// --- AI ---
	PrefixMap.Add(TEXT("BehaviorTree"), TEXT("BT_"));          // 行为树
	PrefixMap.Add(TEXT("BlackboardData"), TEXT("BB_"));        // 黑板数据
	PrefixMap.Add(TEXT("EnvQuery"), TEXT("EQS_"));            // 环境查询
	
	// --- 序列与过场 ---
	PrefixMap.Add(TEXT("LevelSequence"), TEXT("LS_"));         // 关卡序列
	PrefixMap.Add(TEXT("TemplateSequence"), TEXT("TS_"));      // 模板序列
	
	// --- 动画 ---
	PrefixMap.Add(TEXT("AnimBlueprint"), TEXT("ABP_"));        // 动画蓝图
	PrefixMap.Add(TEXT("Animation"), TEXT("A_"));              // 动画序列
	PrefixMap.Add(TEXT("AnimSequence"), TEXT("A_"));
	PrefixMap.Add(TEXT("AnimMontage"), TEXT("AM_"));           // 动画蒙太奇
	PrefixMap.Add(TEXT("Montage"), TEXT("AM_"));              // 动画蒙太奇（简化名）
	PrefixMap.Add(TEXT("BlendSpace"), TEXT("BS_"));           // 混合空间
	PrefixMap.Add(TEXT("BlendSpace1D"), TEXT("BS1D_"));       // 1D混合空间
	PrefixMap.Add(TEXT("AimOffsetBlendSpace"), TEXT("AO_"));  // 瞄准偏移混合空间
	PrefixMap.Add(TEXT("AimOffsetBlendSpace1D"), TEXT("AO1D_")); // 1D瞄准偏移混合空间
	PrefixMap.Add(TEXT("CameraAnimationSequence"), TEXT("CA_")); // 相机动画序列
	PrefixMap.Add(TEXT("PoseAsset"), TEXT("PA_"));             // 姿势资产
	
	// --- 控制绑定 ---
	PrefixMap.Add(TEXT("ControlRigBlueprint"), TEXT("CR_"));   // 控制绑定蓝图
	PrefixMap.Add(TEXT("IKRetargeter"), TEXT("IKR_"));         // IK重定向器
	PrefixMap.Add(TEXT("IKRigDefinition"), TEXT("IK_"));     // IK绑定定义
	
	// --- 材质特殊类型 ---
	PrefixMap.Add(TEXT("MaterialFunctionMaterialLayer"), TEXT("ML_")); // 材质层函数
	
	// --- 蓝图特殊类型（脚本内部判断）---
	PrefixMap.Add(TEXT("ActorBlueprint"), TEXT("BP_"));        // Actor 蓝图默认
	PrefixMap.Add(TEXT("SceneComponentBlueprint"), TEXT("BPSC_"));// SceneComponent 蓝图
	PrefixMap.Add(TEXT("ActorComponentBlueprint"), TEXT("BPC_")); // ActorComponent 蓝图 (非 Scene)
	PrefixMap.Add(TEXT("ObjectBlueprint"), TEXT("BPO_"));       // Object 蓝图 (非特定类型)
	PrefixMap.Add(TEXT("GameModeBaseBlueprint"), TEXT("GM_"));   // GameMode 蓝图
	PrefixMap.Add(TEXT("AnimNotifyBlueprint"), TEXT("AN_"));   // AnimNotify 蓝图
	PrefixMap.Add(TEXT("AnimNotifyStateBlueprint"), TEXT("ANS_"));// AnimNotifyState 蓝图
	
	// --- 蓝图特殊类型（基于 Tag 或类名）---
	PrefixMap.Add(TEXT("BlueprintFunctionLibrary"), TEXT("BPFL_"));// 蓝图函数库
	PrefixMap.Add(TEXT("BlueprintMacroLibrary"), TEXT("BPM_"));  // 蓝图宏库
	PrefixMap.Add(TEXT("BlueprintInterface"), TEXT("BPI_"));     // 蓝图接口
	
	// --- 编辑器工具 ---
	PrefixMap.Add(TEXT("EditorUtilityBlueprint"), TEXT("EUBP_"));// 编辑器工具蓝图
	PrefixMap.Add(TEXT("EditorUtilityWidgetBlueprint"), TEXT("EUWBP_")); // 编辑器工具控件蓝图
	
	// --- Landscape ---
	PrefixMap.Add(TEXT("LandscapeLayerInfoObject"), TEXT("Lyr_"));// 地形图层信息
	
	return PrefixMap;
}

const TMap<FString, FString>& GetAssetPrefixes()
{
	static const TMap<FString, FString> AssetPrefixes = CreateAssetPrefixMap();
	return AssetPrefixes;
}

void FX_AssetEditorModule::StartupModule()
{
	// 注册编辑器工具扩展
	RegisterAssetTools();
	RegisterFolderActions();
	RegisterMeshActions();
	RegisterMeshComponentActions();
	
	// 注册资源浏览器菜单扩展
	RegisterMenuExtensions();
	
	// 注册资源编辑器Actions
	RegisterAssetEditorActions();
	
	// 注册缩略图渲染扩展
	RegisterThumbnailRenderer();

	// 确保在编辑器中运行
	if (!IsRunningCommandlet())
	{
		// 注册自定义设置
		RegisterSettings();

		// 注册内容浏览器菜单扩展
		RegisterContentBrowserContextMenuExtender();

		// 注册关卡编辑器菜单扩展
		RegisterLevelEditorContextMenuExtender();

		// 等待工具菜单系统初始化
		if (UToolMenus::IsToolMenuUIEnabled())
		{
			RegisterMenus();
		}
		else
		{
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FX_AssetEditorModule::RegisterMenus));
		}
	}

	UE_LOG(LogX_AssetEditor, Log, TEXT("X_AssetEditor 模块已启动"));
}

void FX_AssetEditorModule::ShutdownModule()
{
	// 注销内容浏览器菜单扩展
	UnregisterContentBrowserContextMenuExtender();

	// 注销关卡编辑器菜单扩展
	UnregisterLevelEditorContextMenuExtender();

	// 注销菜单扩展
	UnregisterMenuExtensions();

	// 注销自定义设置
	UnregisterSettings();

	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (ToolMenus)
		{
			ToolMenus->UnregisterOwner(this);
		}
	}

	UE_LOG(LogX_AssetEditor, Log, TEXT("X_AssetEditor 模块已关闭"));
}

void FX_AssetEditorModule::RegisterMenus()
{
	// 确保工具菜单系统已初始化
	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		return;
	}

	// 注册内容浏览器的资产上下文菜单
	UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu");
	if (Menu)
	{
		// 添加新的菜单区域
		FToolMenuSection& Section = Menu->FindOrAddSection("AssetContextExtensions");
		
	}
}

FString FX_AssetEditorModule::GetSimpleClassName(const FAssetData& AssetData)
{
	// 获取完整的类路径
	FString FullClassPath = AssetData.AssetClassPath.ToString();
	
	// 获取类名
	FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
	
	// 移除 _C 后缀（如果有的话）
	if (ClassName.EndsWith(TEXT("_C")))
	{
		ClassName.LeftChopInline(2, false);
	}
	
	// 尝试从完整路径中提取类名
	if (ClassName.IsEmpty())
	{
		FString Left, Right;
		if (FullClassPath.Split(TEXT("."), &Left, &Right))
		{
			ClassName = Right;
		}
	}
	
	// 如果类名为空，使用资产名称作为备选
	if (ClassName.IsEmpty())
	{
		ClassName = AssetData.AssetName.ToString();
	}

	return ClassName;
}

FString FX_AssetEditorModule::GetAssetClassDisplayName(const FAssetData& AssetData)
{
	return GetSimpleClassName(AssetData);
}

FString FX_AssetEditorModule::GetCorrectPrefix(const FAssetData& AssetData, const FString& SimpleClassName)
{
	// 获取前缀映射
	const TMap<FString, FString>& AssetPrefixes = GetAssetPrefixes();
	
	// 尝试直接从类名获取前缀
	const FString* PrefixPtr = AssetPrefixes.Find(SimpleClassName);
	if (PrefixPtr)
	{
		return *PrefixPtr;
	}

	// 尝试从完整类路径中获取类名
	FString FullClassPath = AssetData.AssetClassPath.ToString();
	FString Left, Right;
	if (FullClassPath.Split(TEXT("."), &Left, &Right))
	{
		PrefixPtr = AssetPrefixes.Find(Right);
		if (PrefixPtr)
		{
			return *PrefixPtr;
		}
	}

	// 精简日志 - 只保留关键警告
	UE_LOG(LogX_AssetEditor, Warning, TEXT("无法确定资产 '%s' 的前缀 (类型: %s)"), 
		*AssetData.AssetName.ToString(), *SimpleClassName);
	
	return TEXT("");
}

void FX_AssetEditorModule::RenameSelectedAssets()
{
	// 获取选中的资产数据
	TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
	if (SelectedAssets.Num() == 0)
	{
		UE_LOG(LogX_AssetEditor, Warning, TEXT("未选中任何资产，无法执行重命名操作"));
		return;
	}

	// 获取AssetTools模块
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// 开始资产重命名操作
	FScopedTransaction Transaction(NSLOCTEXT("X_AssetEditor", "RenameAssets", "重命名资产"));

	int32 SuccessCount = 0;
	int32 SkippedCount = 0;
	int32 FailedCount = 0;

	// 优化静态变量存储方式，减小初始容量
	static FString LastOperationDetails;
	LastOperationDetails.Empty();
	LastOperationDetails.Append(FString::Printf(TEXT("资产命名规范化操作详情 (%s)\n\n"), *FDateTime::Now().ToString()));
	
	// 获取前缀映射
	const TMap<FString, FString>& AssetPrefixes = GetAssetPrefixes();
	
	UE_LOG(LogX_AssetEditor, Log, TEXT("开始处理%d个资产的命名规范化"), SelectedAssets.Num());
	
	// 添加进度条，改善大量资产处理时的用户体验
	FScopedSlowTask SlowTask(
		SelectedAssets.Num(),
		FText::Format(LOCTEXT("NormalizingAssetNames", "正在规范化 {0} 个资产的命名..."), FText::AsNumber(SelectedAssets.Num()))
	);
	SlowTask.MakeDialog(true); // true表示允许取消

	for (const FAssetData& AssetData : SelectedAssets)
	{
		// 更新进度条
		SlowTask.EnterProgressFrame(1.0f);
		
		// 检查用户是否取消了操作
		if (SlowTask.ShouldCancel())
		{
			UE_LOG(LogX_AssetEditor, Warning, TEXT("用户取消了命名规范化操作"));
			break;
		}

		// 添加异常处理
		if (!AssetData.IsValid())
		{
			UE_LOG(LogX_AssetEditor, Warning, TEXT("发现无效的资产数据，已跳过"));
			FailedCount++;
			continue;
		}

		FString CurrentName = AssetData.AssetName.ToString();
		FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
		
		// 添加路径验证
		if (PackagePath.IsEmpty())
		{
			UE_LOG(LogX_AssetEditor, Warning, TEXT("资产'%s'的包路径无效"), *CurrentName);
			FailedCount++;
			continue;
		}
		
		FString SimpleClassName = GetSimpleClassName(AssetData);
		
		// 记录到操作详情 - 保留关键信息
		LastOperationDetails.Append(FString::Printf(TEXT("处理: %s (类型: %s)\n"), *CurrentName, *SimpleClassName));

		// 获取正确的前缀
		FString CorrectPrefix = GetCorrectPrefix(AssetData, SimpleClassName);
		if (CorrectPrefix.IsEmpty())
		{
			LastOperationDetails.Append(TEXT("  无法确定前缀\n"));
			FailedCount++;
			continue;
		}

		// 检查当前名称是否已经符合规范
		if (CurrentName.StartsWith(CorrectPrefix))
		{
			LastOperationDetails.Append(TEXT("  已符合规范\n"));
			SkippedCount++;
			continue;
		}

		// 构建新名称
		FString BaseName = CurrentName;
		
		// 移除已有的不正确前缀
		for (const auto& Pair : AssetPrefixes)
		{
			const FString& ExistingPrefix = Pair.Value;
			if (!ExistingPrefix.IsEmpty() && CurrentName.StartsWith(ExistingPrefix))
			{
				BaseName = CurrentName.RightChop(ExistingPrefix.Len());
				break;
			}
		}

		// 优化字符串操作，减少内存分配
		FString NewName;
		NewName.Reserve(CorrectPrefix.Len() + BaseName.Len());
		NewName.Append(CorrectPrefix);
		NewName.Append(BaseName);
		
		FString FinalNewName = NewName;
		int32 SuffixCounter = 1;

		// 检查是否存在同名资产
		while (FPackageName::DoesPackageExist(FString::Printf(TEXT("%s/%s"), *PackagePath, *FinalNewName)))
		{
			FinalNewName = FString::Printf(TEXT("%s_%d"), *NewName, SuffixCounter++);
		}

		if (FinalNewName != CurrentName)
		{
			// 执行重命名操作
			UObject* AssetObject = AssetData.GetAsset();
			if (AssetObject)
			{
				TArray<FAssetRenameData> AssetsToRename;
				AssetsToRename.Add(FAssetRenameData(AssetObject, PackagePath, FinalNewName));
				
				if (AssetTools.RenameAssets(AssetsToRename))
				{
					LastOperationDetails.Append(FString::Printf(TEXT("  重命名: %s -> %s\n"), *CurrentName, *FinalNewName));
					SuccessCount++;
				}
				else
				{
					LastOperationDetails.Append(FString::Printf(TEXT("  重命名失败: %s\n"), *CurrentName));
					FailedCount++;
				}
			}
			else
			{
				LastOperationDetails.Append(FString::Printf(TEXT("  无法加载资产: %s\n"), *CurrentName));
				FailedCount++;
			}
		}
		else
		{
			// 名称未改变，跳过
			LastOperationDetails.Append(TEXT("  名称未改变，跳过\n"));
			SkippedCount++;
		}
	}

	// 添加统计信息到详细信息 - 保留关键信息
	LastOperationDetails.Append(TEXT("\n==================== 命名规范化完成 ====================\n"));
	LastOperationDetails.Append(TEXT("处理结果统计:\n"));
	LastOperationDetails.Append(FString::Printf(TEXT("- 总计资产: %d\n"), SelectedAssets.Num()));
	LastOperationDetails.Append(FString::Printf(TEXT("- 成功重命名: %d\n"), SuccessCount));
	LastOperationDetails.Append(FString::Printf(TEXT("- 已符合规范: %d\n"), SkippedCount));
	LastOperationDetails.Append(FString::Printf(TEXT("- 处理失败: %d\n"), FailedCount));
	LastOperationDetails.Append(TEXT("====================================================\n"));

	// 构建详细信息文本
	FString DetailedMessage;
	if (FailedCount > 0)
	{
		DetailedMessage.Append(TEXT("失败的资产:\n"));
		int32 FailureCount = 0;
		
		for (const FAssetData& AssetData : SelectedAssets)
		{
			FString SimpleClassName = GetSimpleClassName(AssetData);
			FString CorrectPrefix = GetCorrectPrefix(AssetData, SimpleClassName);
			
			if (CorrectPrefix.IsEmpty())
			{
				// 无法确定前缀的情况
				DetailedMessage.Append(FString::Printf(TEXT("- %s (类型: %s): 无法确定前缀\n"), 
					*AssetData.AssetName.ToString(), *SimpleClassName));
				FailureCount++;
				
				// 优化：限制失败资产显示数量，减少内存使用
				if (FailureCount >= 10)
				{
					break;
				}
			}
		}
		
		// 如果失败数量过多，只显示前几个
		if (FailureCount > 10 || FailureCount < FailedCount)
		{
			DetailedMessage.Append(FString::Printf(TEXT("... 以及其他 %d 个资产\n"), FailedCount - FailureCount));
		}
		
		DetailedMessage.Append(TEXT("\n提示: 可能需要为这些资产类型添加前缀规则。"));
	}
	else if (SuccessCount > 0)
	{
		DetailedMessage = TEXT("所有资产处理成功！\n\n重命名操作无法通过编辑器撤销功能回滚。");
	}
	else if (SkippedCount == SelectedAssets.Num())
	{
		DetailedMessage = TEXT("所有选中的资产已符合命名规范，无需重命名。");
	}
	else
	{
		DetailedMessage = TEXT("未处理任何资产。请确保选中了有效的资产。");
	}
	
	// 更新静态详细信息变量
	LastOperationDetails.Append(DetailedMessage);

	// 方案一：显示一个可点击的通知消息
	FNotificationInfo Info(FText::Format(
		LOCTEXT("AssetRenameNotification", "资产命名规范化完成\n总计: {0} | 成功: {1} | 已符合: {2} | 失败: {3}\n点击查看详情"),
		FText::AsNumber(SelectedAssets.Num()),
		FText::AsNumber(SuccessCount),
		FText::AsNumber(SkippedCount),
		FText::AsNumber(FailedCount)
	));
	
	// 基本设置 - 纯文字显示
	Info.bUseLargeFont = false;
	Info.bUseSuccessFailIcons = false; // 禁用图标
	Info.bUseThrobber = false;
	Info.FadeOutDuration = 1.0f; // 淡出时间短一些
	Info.ExpireDuration = FailedCount > 0 ? 8.0f : 5.0f; // 失败时显示时间长一些
	Info.bFireAndForget = true; // 使用自动消失模式
	Info.bAllowThrottleWhenFrameRateIsLow = true; // 低帧率时允许降频
	
	// 禁用图标
	Info.Image = nullptr;
	
	// 添加点击通知的处理函数
	Info.Hyperlink = FSimpleDelegate::CreateLambda([]()
	{
		// 创建消息对话框
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(LastOperationDetails), 
			LOCTEXT("ViewDetailsHyperlink", "查看详情"));
	});
	Info.HyperlinkText = LOCTEXT("ViewDetailsHyperlink", "查看详情");
	
	// 显示通知并获取通知项引用
	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	
	// 设置通知的完成状态 - 只使用颜色区分，不使用图标
	if (NotificationItem.IsValid())
	{
		// 通知会在ExpireDuration时间后自动消失
		
		if (FailedCount == 0)
		{
			// 成功 - 绿色
			NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		}
		else
		{
			// 失败 - 红色
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
	
	// 优化处理失败资产的逻辑 - 仅在有失败且用户点击查看详情时才显示对话框
	// 方案二：直接显示详细信息对话框，但只在失败资产数量多时才自动显示
	if (FailedCount > 0 && FailedCount > SelectedAssets.Num() / 3)  // 失败数量超过总数的1/3时自动显示
	{
		// 如果有较多失败的资产，直接显示详细信息对话框
		FMessageDialog::Open(EAppMsgType::Ok, 
			FText::FromString(LastOperationDetails), 
			LOCTEXT("AssetRenameDetails", "资产命名规范化详情"));
	}
}

void FX_AssetEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Project");
		
		if (SettingsContainer.IsValid())
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "X_AssetEditor",
				LOCTEXT("RuntimeSettingsName", "X Asset Editor"),
				LOCTEXT("RuntimeSettingsDescription", "资产编辑器工具集配置"),
				GetMutableDefault<UX_MaterialToolsSettings>()
			);

			if (SettingsSection.IsValid())
			{
				SettingsSection->OnModified().BindRaw(this, &FX_AssetEditorModule::HandleSettingsSaved);
			}
		}
	}
}

void FX_AssetEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "X_AssetEditor");
	}
}

bool FX_AssetEditorModule::HandleSettingsSaved()
{
	UX_MaterialToolsSettings* Settings = GetMutableDefault<UX_MaterialToolsSettings>();
	bool bSaved = false;
	
	if (Settings)
	{
		Settings->SaveConfig();
		bSaved = true;
	}

	return bSaved;
}

void FX_AssetEditorModule::RegisterMenuExtensions()
{
	// 创建命令列表
	PluginCommands = MakeShareable(new FUICommandList);

	// 注册菜单扩展
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, 
			FMenuExtensionDelegate::CreateRaw(this, &FX_AssetEditorModule::AddMenuExtension));
			
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
		MenuExtenders.Add(MenuExtender);
	}
}

void FX_AssetEditorModule::UnregisterMenuExtensions()
{
	MenuExtenders.Empty();
	PluginCommands.Reset();
}

void FX_AssetEditorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.BeginSection("X_AssetEditor", LOCTEXT("X_AssetEditor", "X Asset Editor"));
	{
		// 原有的资产命名工具
		Builder.AddMenuEntry(
			LOCTEXT("AssetRenaming", "资产命名规范化"),
			LOCTEXT("AssetRenamingTooltip", "根据命名规范重命名选中资产"),			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&FX_AssetEditorModule::RenameSelectedAssets)
			)
		);

		// 添加材质函数工具
		Builder.AddMenuEntry(
			LOCTEXT("MaterialFunctionTool", "材质函数工具"),
			LOCTEXT("MaterialFunctionToolTooltip", "打开材质函数工具"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					// 打开材质函数工具窗口
					TArray<FAssetData> EmptyAssetArray;
					TArray<AActor*> EmptyActorArray;
					ShowMaterialFunctionPicker(EmptyAssetArray, EmptyActorArray, false);
				})
			)
		);
	}
	Builder.EndSection();
}

void FX_AssetEditorModule::RegisterContentBrowserContextMenuExtender()
{
	// 优化模块加载
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
	{
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule->GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(this, &FX_AssetEditorModule::OnExtendContentBrowserAssetSelectionMenu));
		ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
	}
}

void FX_AssetEditorModule::UnregisterContentBrowserContextMenuExtender()
{
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
	{
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule->GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
	}
}

TSharedRef<FExtender> FX_AssetEditorModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());

	// 只有当选中了资产时才添加菜单项
	if (SelectedAssets.Num() > 0)
	{
		// 为所有资产添加"资产规范命名"菜单
		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FX_AssetEditorModule::AddAssetNamingMenuEntry, SelectedAssets)
		);

		// 检查是否有可能包含材质的资产
		bool bHasMaterialAssets = false;
		for (const FAssetData& Asset : SelectedAssets)
		{
			const FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
			if (AssetClassName.Contains(TEXT("Material")) || 
				AssetClassName == TEXT("StaticMesh") || 
				AssetClassName == TEXT("SkeletalMesh") ||
				AssetClassName.Contains(TEXT("Blueprint")))
			{
				bHasMaterialAssets = true;
				break;
			}
		}

		// 如果有可能包含材质的资产，添加材质工具菜单
		if (bHasMaterialAssets)
		{
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateRaw(this, &FX_AssetEditorModule::AddMaterialFunctionMenuEntry, SelectedAssets)
			);
		}
	}

	return Extender;
}

// 添加资产命名菜单项
void FX_AssetEditorModule::AddAssetNamingMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.BeginSection("AssetNaming", LOCTEXT("AssetNaming", "资产命名"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameAssetMenuAction", "资产规范命名"),
			LOCTEXT("RenameAssetMenuAction_Tooltip", "根据类型自动规范选中资产的命名"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericEditor.Rename"),
			FUIAction(FExecuteAction::CreateStatic(&FX_AssetEditorModule::RenameSelectedAssets))
		);
	}
	MenuBuilder.EndSection();
}

// 添加材质函数菜单项
void FX_AssetEditorModule::AddMaterialFunctionMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.BeginSection("MaterialTools", LOCTEXT("MaterialTools", "材质工具"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddFresnelToAssets", "添加菲涅尔函数"),
			LOCTEXT("AddFresnelToAssetsTooltip", "向所选资产的材质添加菲涅尔函数"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.NewMaterialFunction"),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FX_AssetEditorModule::AddFresnelToAssets, SelectedAssets)
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddMaterialFunction", "添加材质函数"),
			LOCTEXT("AddMaterialFunctionTooltip", "向所选资产的材质添加自定义材质函数"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.NewMaterialFunctionCall"),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FX_AssetEditorModule::OnAddMaterialFunctionToAsset, SelectedAssets)
			)
		);
	}
	MenuBuilder.EndSection();
}

void FX_AssetEditorModule::RegisterLevelEditorContextMenuExtender()
{
	// 注册关卡编辑器Actor上下文菜单扩展
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& LevelEditorMenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
	
	LevelEditorMenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FX_AssetEditorModule::OnExtendLevelEditorActorContextMenu));
	LevelEditorExtenderDelegateHandle = LevelEditorMenuExtenders.Last().GetHandle();
}

void FX_AssetEditorModule::UnregisterLevelEditorContextMenuExtender()
{
	// 注销关卡编辑器Actor上下文菜单扩展
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		auto& LevelEditorMenuExtenders = LevelEditorModule->GetAllLevelViewportContextMenuExtenders();
		LevelEditorMenuExtenders.RemoveAll([this](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) { return Delegate.GetHandle() == LevelEditorExtenderDelegateHandle; });
	}
}

TSharedRef<FExtender> FX_AssetEditorModule::OnExtendLevelEditorActorContextMenu(
	TSharedRef<FUICommandList> CommandList, 
	TArray<AActor*> SelectedActors)
{
	TSharedRef<FExtender> Extender(new FExtender());

	// 只有当选中了Actor时才添加菜单项
	if (SelectedActors.Num() > 0)
	{
		Extender->AddMenuExtension(
			"ActorControl",
			EExtensionHook::After,
			CommandList,
			FMenuExtensionDelegate::CreateRaw(this, &FX_AssetEditorModule::AddActorMaterialMenuEntry, SelectedActors)
		);
	}

	return Extender;
}

void FX_AssetEditorModule::AddActorMaterialMenuEntry(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors)
{
	MenuBuilder.BeginSection("MaterialTools", LOCTEXT("MaterialTools", "资产工具"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddFresnelToActors", "添加菲涅尔函数"),
			LOCTEXT("AddFresnelToActorsTooltip", "向所选Actor的材质添加菲涅尔函数"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FX_AssetEditorModule::AddFresnelToActors, SelectedActors)
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddMaterialFunctionToActor", "添加材质函数"),
			LOCTEXT("AddMaterialFunctionToActorTooltip", "向所选Actor的材质添加自定义材质函数"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FX_AssetEditorModule::OnAddMaterialFunctionToActor, SelectedActors)
			)
		);
	}
	MenuBuilder.EndSection();
}

void FX_AssetEditorModule::ShowMaterialFunctionPicker(
	const TArray<FAssetData>& SelectedAssets,
	const TArray<AActor*>& SelectedActors,
	bool bIsActor)
{
	// 打印日志，标明使用了哪个模块的选择器
	UE_LOG(LogX_AssetEditor, Warning, TEXT("### 调用了 FX_AssetEditorModule::ShowMaterialFunctionPicker - 使用X_AssetEditor模块的选择器"));
	
	// 检查是否有选中的资产或Actor
	if ((bIsActor && SelectedActors.Num() == 0) || (!bIsActor && SelectedAssets.Num() == 0))
	{
		FNotificationInfo Info(LOCTEXT("NoItemsSelected", "请先选择资产或场景中的Actor"));
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = true;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}
    FAssetPickerConfig AssetPickerConfig;
    AssetPickerConfig.Filter.ClassPaths.Add(UMaterialFunction::StaticClass()->GetClassPathName());
    AssetPickerConfig.Filter.bRecursiveClasses = true;
    AssetPickerConfig.bAllowNullSelection = false;
    AssetPickerConfig.bCanShowFolders = true;
    AssetPickerConfig.bCanShowClasses = true;
    AssetPickerConfig.bShowTypeInColumnView = true;
    AssetPickerConfig.bShowPathInColumnView = true;
    AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

    AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(
        [this, SelectedAssets, SelectedActors, bIsActor](const FAssetData& AssetData)
        {
            UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(AssetData.GetAsset());
            if (!MaterialFunction)
            {
                FNotificationInfo Info(LOCTEXT("NoMaterialFunctionSelected", "未选择材质函数或选择的不是材质函数"));
                Info.ExpireDuration = 3.0f;
                Info.bUseLargeFont = true;
                FSlateNotificationManager::Get().AddNotification(Info);
                return;
            }

            // 关闭资产选择器窗口
            if (PickerWindow.IsValid())
            {
                PickerWindow->RequestDestroyWindow();
            }
            
            // 创建参数结构体
            FX_MaterialFunctionParams Params;
            
            // 根据材质函数名称自动设置连接选项
            Params.SetupConnectionsByFunctionName(MaterialFunction->GetName());
            
            // 根据材质函数的输入输出引脚情况设置智能连接选项
            int32 InputCount = 0;
            int32 OutputCount = 0;
            FX_MaterialFunctionOperation::GetFunctionInputOutputCount(MaterialFunction, InputCount, OutputCount);
            
            // 只有同时具有输入和输出引脚的材质函数才默认启用智能连接
            // 只有输出引脚的材质函数默认禁用智能连接
            Params.bEnableSmartConnect = (InputCount > 0 && OutputCount > 0);
            
            UE_LOG(LogX_AssetEditor, Log, TEXT("材质函数 %s: 输入引脚=%d, 输出引脚=%d, 智能连接=%s"), 
                *MaterialFunction->GetName(), InputCount, OutputCount, 
                Params.bEnableSmartConnect ? TEXT("启用") : TEXT("禁用"));
            
            // 创建结构体包装器
            TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FX_MaterialFunctionParams::StaticStruct(), (uint8*)&Params);
            
            // 显示参数对话框
            FText DialogTitle = FText::Format(
                LOCTEXT("ConfigureMaterialFunction", "配置材质函数: {0}"),
                FText::FromString(MaterialFunction->GetName())
            );
            
            bool bOKPressed = SX_MaterialFunctionParamDialog::ShowDialog(DialogTitle, StructOnScope);
            
            if (bOKPressed)
            {
                // 用户点击了确定按钮，处理材质函数
                if (bIsActor)
                {
                    // 处理Actor材质函数
                    FX_MaterialFunctionOperation::ProcessActorMaterialFunction(
                        SelectedActors, MaterialFunction, FName(*Params.NodeName), MakeShared<FX_MaterialFunctionParams>(Params));
                }
                else
                {
                    // 处理资产材质函数
                    FX_MaterialFunctionOperation::ProcessAssetMaterialFunction(
                        SelectedAssets, MaterialFunction, FName(*Params.NodeName), MakeShared<FX_MaterialFunctionParams>(Params));
                }
            }
        });

    AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateLambda(
        [this, SelectedAssets, SelectedActors, bIsActor](const FAssetData& AssetData)
        {
            UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(AssetData.GetAsset());
            if (!MaterialFunction)
            {
                FNotificationInfo Info(LOCTEXT("NoMaterialFunctionSelected", "未选择材质函数或选择的不是材质函数"));
                Info.ExpireDuration = 3.0f;
                Info.bUseLargeFont = true;
                FSlateNotificationManager::Get().AddNotification(Info);
                return;
            }

            // 关闭资产选择器窗口
            if (PickerWindow.IsValid())
            {
                PickerWindow->RequestDestroyWindow();
            }
            
            // 创建参数结构体
            FX_MaterialFunctionParams Params;
            
            // 根据材质函数名称自动设置连接选项
            Params.SetupConnectionsByFunctionName(MaterialFunction->GetName());
            
            // 根据材质函数的输入输出引脚情况设置智能连接选项
            int32 InputCount = 0;
            int32 OutputCount = 0;
            FX_MaterialFunctionOperation::GetFunctionInputOutputCount(MaterialFunction, InputCount, OutputCount);
            
            // 只有同时具有输入和输出引脚的材质函数才默认启用智能连接
            // 只有输出引脚的材质函数默认禁用智能连接
            Params.bEnableSmartConnect = (InputCount > 0 && OutputCount > 0);
            
            UE_LOG(LogX_AssetEditor, Log, TEXT("材质函数 %s: 输入引脚=%d, 输出引脚=%d, 智能连接=%s"), 
                *MaterialFunction->GetName(), InputCount, OutputCount, 
                Params.bEnableSmartConnect ? TEXT("启用") : TEXT("禁用"));
            
            // 创建结构体包装器
            TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FX_MaterialFunctionParams::StaticStruct(), (uint8*)&Params);
            
            // 显示参数对话框
            FText DialogTitle = FText::Format(
                LOCTEXT("ConfigureMaterialFunction", "配置材质函数: {0}"),
                FText::FromString(MaterialFunction->GetName())
            );
            
            bool bOKPressed = SX_MaterialFunctionParamDialog::ShowDialog(DialogTitle, StructOnScope);
            
            if (bOKPressed)
            {
                // 用户点击了确定按钮，处理材质函数
                if (bIsActor)
                {
                    // 处理Actor材质函数
                    FX_MaterialFunctionOperation::ProcessActorMaterialFunction(
                        SelectedActors, MaterialFunction, FName(*Params.NodeName), MakeShared<FX_MaterialFunctionParams>(Params));
                }
                else
                {
                    // 处理资产材质函数
                    FX_MaterialFunctionOperation::ProcessAssetMaterialFunction(
                        SelectedAssets, MaterialFunction, FName(*Params.NodeName), MakeShared<FX_MaterialFunctionParams>(Params));
                }
            }
        });

    // 创建资产选择器小部件
    TSharedRef<SWidget> AssetPickerWidget = SNew(SBox)
        .WidthOverride(400.0f)
        .HeightOverride(600.0f)
        [
            FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().CreateAssetPicker(AssetPickerConfig)
        ];

    // 创建并显示模态窗口
    PickerWindow = SNew(SWindow)
        .Title(LOCTEXT("SelectMaterialFunction", "选择材质函数"))
        .SizingRule(ESizingRule::UserSized)
        .AutoCenter(EAutoCenter::PreferredWorkArea)
        .ClientSize(FVector2D(400, 600))
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        [
            AssetPickerWidget
        ];

    FSlateApplication::Get().AddModalWindow(PickerWindow.ToSharedRef(), nullptr, false);

	// 记录到日志
	UE_LOG(LogX_AssetEditor, Log, TEXT("打开材质函数选择器，%s模式，选中了 %d 个项目"), bIsActor ? TEXT("Actor") : TEXT("Asset"), (bIsActor ? SelectedActors.Num() : SelectedAssets.Num()));
}

void FX_AssetEditorModule::ProcessAssetMaterialFunction(
	const TArray<FAssetData>& SelectedAssets,
	UMaterialFunctionInterface* MaterialFunction)
{
	// 委托给FX_MaterialFunctionOperation处理
	FX_MaterialFunctionOperation::ProcessAssetMaterialFunction(SelectedAssets, MaterialFunction, NAME_None);
}

void FX_AssetEditorModule::ProcessActorMaterialFunction(
	const TArray<AActor*>& SelectedActors,
	UMaterialFunctionInterface* MaterialFunction)
{
	// 委托给FX_MaterialFunctionOperation处理
	FX_MaterialFunctionOperation::ProcessActorMaterialFunction(SelectedActors, MaterialFunction, NAME_None);
}

void FX_AssetEditorModule::AddFresnelToAssets(TArray<FAssetData> SelectedAssets)
{
    // 检查是否有选中的资产
    if (SelectedAssets.Num() == 0)
    {
        FNotificationInfo Info(LOCTEXT("NoAssetsSelected", "请先选择资产"));
        Info.ExpireDuration = 3.0f;
        Info.bUseLargeFont = true;
        FSlateNotificationManager::Get().AddNotification(Info);
        return;
    }

    // 显示处理中的通知
    FNotificationInfo InfoProcessing(LOCTEXT("AddFresnelProcessing", "正在添加菲涅尔函数..."));
    InfoProcessing.ExpireDuration = 2.0f;
    InfoProcessing.bFireAndForget = false;
    InfoProcessing.bUseSuccessFailIcons = false;
    InfoProcessing.bUseThrobber = true;
    auto NotificationItem = FSlateNotificationManager::Get().AddNotification(InfoProcessing);
    
    // 收集所有源对象
    TArray<UObject*> SourceObjects;
    for (const FAssetData& AssetData : SelectedAssets)
    {
        if (UObject* Asset = AssetData.GetAsset())
        {
            SourceObjects.Add(Asset);
        }
    }
    
    // 使用统一处理逻辑添加菲涅尔函数
    FMaterialProcessResult Result = FX_MaterialFunctionManager::AddFresnelToAssets(SourceObjects);
    
    // 关闭处理中的通知
    if (NotificationItem.IsValid())
    {
        NotificationItem->SetCompletionState(Result.SuccessCount > 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
        NotificationItem->ExpireAndFadeout();
    }
    
    // 显示结果通知
    FText ResultMessage;
    if (Result.TotalMaterials == 0)
    {
        ResultMessage = LOCTEXT("NoMaterialsFound", "选中的资产中未找到任何材质。\n请选择包含材质的资产，如材质、网格体或蓝图。");
    }
    else if (Result.SuccessCount > 0)
    {
        ResultMessage = FText::Format(
            LOCTEXT("AddFresnelSuccess", "添加菲涅尔函数结果\n找到材质: {0}\n成功: {1}\n已有函数: {2}\n失败: {3}"),
            FText::AsNumber(Result.TotalMaterials),
            FText::AsNumber(Result.SuccessCount),
            FText::AsNumber(Result.AlreadyHasFunctionCount),
            FText::AsNumber(Result.FailedCount));
    }
    else
    {
        ResultMessage = FText::Format(
            LOCTEXT("AddFresnelFailed", "添加菲涅尔函数失败\n源对象: {0}\n找到材质: {1}\n失败: {2}"),
            FText::AsNumber(Result.TotalSourceObjects),
            FText::AsNumber(Result.TotalMaterials),
            FText::AsNumber(Result.FailedCount));
    }
    
    FNotificationInfo InfoResult(ResultMessage);
    InfoResult.ExpireDuration = 5.0f;
    InfoResult.bUseLargeFont = true;
    InfoResult.bUseSuccessFailIcons = true;
    FSlateNotificationManager::Get().AddNotification(InfoResult);
}

bool FX_AssetEditorModule::ValidateAssetPath(const FString& AssetPath)
{
	// 验证资产路径是否有效
	return !AssetPath.IsEmpty();
}

void FX_AssetEditorModule::OnAddMaterialFunctionToAsset(TArray<FAssetData> SelectedAssets)
{
	// 检查是否有选中的资产
	if (SelectedAssets.Num() == 0)
	{
		FNotificationInfo Info(LOCTEXT("NoAssetsSelected", "请先选择资产"));
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = true;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// 显示材质函数选择器
	ShowMaterialFunctionPicker(SelectedAssets, TArray<AActor*>(), false);
	
	// 记录到日志
	UE_LOG(LogX_AssetEditor, Log, TEXT("用户尝试添加材质函数，选中了 %d 个资产"), SelectedAssets.Num());

	// 添加友好提示
	FNotificationInfo Info(LOCTEXT("MaterialFunctionInfo", "如果选中的资产不包含材质，将不会有任何效果。\n请确保选择了包含材质的资产，如材质、网格体或蓝图。"));
	Info.ExpireDuration = 5.0f;
	Info.bUseLargeFont = false;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FX_AssetEditorModule::OnAddMaterialFunctionToActor(TArray<AActor*> SelectedActors)
{
	// 显示材质函数选择器
	ShowMaterialFunctionPicker(TArray<FAssetData>(), SelectedActors, true);
}

void FX_AssetEditorModule::AddFresnelToActors(TArray<AActor*> SelectedActors)
{
    // 检查是否有选中的Actor
    if (SelectedActors.Num() == 0)
    {
        FNotificationInfo Info(LOCTEXT("NoActorsSelected", "请先选择场景中的Actor"));
        Info.ExpireDuration = 3.0f;
        Info.bUseLargeFont = true;
        FSlateNotificationManager::Get().AddNotification(Info);
        return;
    }
    
    // 显示处理中的通知
    FNotificationInfo InfoProcessing(LOCTEXT("ProcessingActorMaterials", "正在处理Actor材质..."));
    InfoProcessing.ExpireDuration = 2.0f;
    InfoProcessing.bFireAndForget = false;
    InfoProcessing.bUseSuccessFailIcons = false;
    InfoProcessing.bUseThrobber = true;
    auto NotificationItem = FSlateNotificationManager::Get().AddNotification(InfoProcessing);
    
    // 收集所有源对象
    TArray<UObject*> SourceObjects;
    for (AActor* Actor : SelectedActors)
    {
        if (Actor)
        {
            SourceObjects.Add(Actor);
        }
    }
    
    // 使用统一处理逻辑添加菲涅尔函数
    FMaterialProcessResult Result = FX_MaterialFunctionManager::AddFresnelToAssets(SourceObjects);
    
    // 关闭处理中的通知
    if (NotificationItem.IsValid())
    {
        NotificationItem->SetCompletionState(Result.SuccessCount > 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
        NotificationItem->ExpireAndFadeout();
    }
    
    // 显示结果通知
    FText ResultMessage;
    if (Result.SuccessCount > 0)
    {
        ResultMessage = FText::Format(
            LOCTEXT("ProcessActorMaterialsSuccess", "在Actor上添加菲涅尔函数结果\n找到材质: {0}\n成功: {1}\n已有函数: {2}\n失败: {3}"), 
            FText::AsNumber(Result.TotalMaterials),
            FText::AsNumber(Result.SuccessCount),
            FText::AsNumber(Result.AlreadyHasFunctionCount),
            FText::AsNumber(Result.FailedCount));
    }
    else
    {
        ResultMessage = FText::Format(
            LOCTEXT("ProcessActorMaterialsFailed", "处理Actor材质失败\n源对象: {0}\n找到材质: {1}\n失败: {2}"),
            FText::AsNumber(Result.TotalSourceObjects),
            FText::AsNumber(Result.TotalMaterials),
            FText::AsNumber(Result.FailedCount));
    }
    
    FNotificationInfo InfoResult(ResultMessage);
    InfoResult.ExpireDuration = 5.0f;
    InfoResult.bUseLargeFont = true;
    InfoResult.bUseSuccessFailIcons = true;
    FSlateNotificationManager::Get().AddNotification(InfoResult);
}

// 实现空的注册函数
void FX_AssetEditorModule::RegisterAssetTools()
{
	// 获取AssetTools模块实例
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();
	
	// 在这里注册自定义资产类型和操作
	// 例如: AssetTools.RegisterAssetTypeActions(MakeShareable(new FMyAssetTypeActions));
	
	UE_LOG(LogX_AssetEditor, Log, TEXT("已注册资产工具"));
}

void FX_AssetEditorModule::RegisterFolderActions()
{
	// 注册文件夹相关操作
	// 例如: 为Content Browser中的文件夹注册右键菜单项
	
	UE_LOG(LogX_AssetEditor, Log, TEXT("已注册文件夹操作"));
}

void FX_AssetEditorModule::RegisterMeshActions()
{
	// 注册网格体相关操作
	// 例如: 为StaticMesh和SkeletalMesh资产注册自定义操作
	
	UE_LOG(LogX_AssetEditor, Log, TEXT("已注册网格体操作"));
}

void FX_AssetEditorModule::RegisterMeshComponentActions()
{
	// 注册网格体组件相关操作
	// 例如: 为StaticMeshComponent和SkeletalMeshComponent注册自定义操作
	
	UE_LOG(LogX_AssetEditor, Log, TEXT("已注册网格体组件操作"));
}

void FX_AssetEditorModule::RegisterAssetEditorActions()
{
	// 注册资产编辑器相关操作
	// 例如: 为自定义资产编辑器注册工具栏按钮和菜单项
	
	UE_LOG(LogX_AssetEditor, Log, TEXT("已注册资产编辑器操作"));
}

void FX_AssetEditorModule::RegisterThumbnailRenderer()
{
	// 在编辑器中检查
	if (GIsEditor)
	{
		// 使用符合UE5.3规范的方式查找ThumbnailManager
		UObject* ThumbnailManagerSingleton = nullptr;
		
		// 使用StaticLoadClass安全地加载ThumbnailManager类
		UClass* ThumbnailManagerClass = nullptr;
		
		// 首先尝试从UnrealEd模块加载类
		FString ThumbnailManagerClassName = TEXT("/Script/UnrealEd.ThumbnailManager");
		ThumbnailManagerClass = LoadClass<UObject>(nullptr, *ThumbnailManagerClassName);
		
		if (ThumbnailManagerClass)
		{
			// 获取单例实例
			FString ThumbnailManagerObjectPath = TEXT("/Engine/UnrealEd.Default__ThumbnailManager");
			ThumbnailManagerSingleton = LoadObject<UObject>(nullptr, *ThumbnailManagerObjectPath);
			
			if (ThumbnailManagerSingleton)
			{
				UE_LOG(LogX_AssetEditor, Log, TEXT("找到ThumbnailManager实例，可以注册缩略图渲染器"));
				
				// 这里可以添加缩略图渲染器注册逻辑
				// 例如通过反射调用RegisterCustomRenderer方法
			}
		}
	}
	
	UE_LOG(LogX_AssetEditor, Log, TEXT("已注册缩略图渲染器"));
}

#undef LOCTEXT_NAMESPACE
