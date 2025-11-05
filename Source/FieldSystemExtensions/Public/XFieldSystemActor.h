// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Field/FieldSystemActor.h"
#include "Field/FieldSystemObjects.h"
#include "Field/FieldSystemTypes.h"
#include "XFieldSystemActor.generated.h"

/**
 * Field响应禁用方法
 * 
 * 通过将物理组件设置为Kinematic状态来阻止Field System影响
 * Kinematic物体不响应外力，但保留完整的碰撞检测功能
 */
UENUM(BlueprintType)
enum class EFieldResponseDisableMethod : uint8
{
	/** 
	 * 设置为运动学
	 * 
	 * 原理：Kinematic状态的物体在Chaos物理系统中不响应任何外力
	 * 
	 * ✅ 优点：
	 *   - 完全阻止Field System影响
	 *   - 保留碰撞检测和物理查询
	 *   - 可通过蓝图调用SetSimulatePhysics恢复
	 *   - 性能开销极小
	 * 
	 * ⚠️ 注意：
	 *   - 不受重力影响（布娃娃会停止下落）
	 *   - 不响应其他外力和冲量
	 */
	SetKinematic UMETA(DisplayName = "设为运动学")
};

/**
 * AXFieldSystemActor
 * 
 * 增强版本的Field System Actor，提供丰富的筛选功能
 * 完全兼容AFieldSystemActor，可直接替换FS_MasterField的父类
 * 
 * 筛选功能：
 * - 包含/排除特定Actor类
 * - 包含/排除带有特定Tag的Actor
 * - 精确控制影响的对象类型（刚体、布料、破碎、角色等）
 * - 精确控制影响的状态类型（动态、静态、运动学等）
 */
UCLASS(Blueprintable, ClassGroup = (Field), meta = (ChildCanTick))
class FIELDSYSTEMEXTENSIONS_API AXFieldSystemActor : public AFieldSystemActor
{
	GENERATED_BODY()

public:
	AXFieldSystemActor(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	// ============================================================================
	// 筛选配置属性
	// ============================================================================

	/** 是否启用高级筛选功能 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter", meta = (DisplayName = "启用筛选"))
	bool bEnableFiltering = false;

	// ----------------------------------------------------------------------------
	// 对象类型筛选
	// ----------------------------------------------------------------------------

	/** 影响的对象类型（刚体、布料、破碎、角色等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Object Type", 
		meta = (EditCondition = "bEnableFiltering", DisplayName = "对象类型"))
	TEnumAsByte<EFieldObjectType> ObjectType = EFieldObjectType::Field_Object_All;

	// ----------------------------------------------------------------------------
	// 状态类型筛选
	// ----------------------------------------------------------------------------

	/** 影响的状态类型（动态、静态、运动学等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|State Type", 
		meta = (EditCondition = "bEnableFiltering", DisplayName = "状态类型"))
	TEnumAsByte<EFieldFilterType> FilterType = EFieldFilterType::Field_Filter_All;

	// ----------------------------------------------------------------------------
	// 位置类型
	// ----------------------------------------------------------------------------

	/** 位置类型（质心、轴心点） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Position", 
		meta = (EditCondition = "bEnableFiltering", DisplayName = "位置类型"))
	TEnumAsByte<EFieldPositionType> PositionType = EFieldPositionType::Field_Position_CenterOfMass;

	// ----------------------------------------------------------------------------
	// Actor类筛选（曲线实现）
	// ----------------------------------------------------------------------------

	/** 
	 * 是否启用Actor类筛选
	 * 注意：这是通过修改Actor物理响应实现的，不是Field System原生功能
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Advanced", 
		meta = (EditCondition = "bEnableFiltering", DisplayName = "启用类筛选（运行时）"))
	bool bEnableActorClassFilter = false;

	/** 排除这些Actor类（通过禁用物理响应实现） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Advanced", 
		meta = (EditCondition = "bEnableFiltering && bEnableActorClassFilter", DisplayName = "排除的类"))
	TArray<TSubclassOf<AActor>> ExcludeActorClasses;

	/** 只影响这些Actor类（为空则不限制，通过禁用其他Actor物理响应实现） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Advanced", 
		meta = (EditCondition = "bEnableFiltering && bEnableActorClassFilter", DisplayName = "包含的类（可选）"))
	TArray<TSubclassOf<AActor>> IncludeActorClasses;

	// ----------------------------------------------------------------------------
	// Actor Tag筛选（曲线实现）
	// ----------------------------------------------------------------------------

	/** 
	 * 是否启用Actor Tag筛选
	 * 注意：这是通过修改Actor物理响应实现的，不是Field System原生功能
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Advanced", 
		meta = (EditCondition = "bEnableFiltering", DisplayName = "启用Tag筛选（运行时）"))
	bool bEnableActorTagFilter = false;

	/** 排除带有这些Tag的Actor（通过禁用物理响应实现） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Advanced", 
		meta = (EditCondition = "bEnableFiltering && bEnableActorTagFilter", DisplayName = "排除的Tag"))
	TArray<FName> ExcludeActorTags;

	/** 只影响带有这些Tag的Actor（为空则不限制） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Advanced", 
		meta = (EditCondition = "bEnableFiltering && bEnableActorTagFilter", DisplayName = "包含的Tag（可选）"))
	TArray<FName> IncludeActorTags;


	/** 
	 * 是否监听Actor Spawn事件（推荐）
	 * 启用后无需遍历场景，只处理新生成的Actor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|Filter|Advanced", 
		meta = (EditCondition = "bEnableFiltering && (bEnableActorClassFilter || bEnableActorTagFilter)", 
			DisplayName = "监听Spawn事件"))
	bool bListenToActorSpawn = true;

	// ============================================================================
	// 蓝图可调用方法
	// ============================================================================

	/**
	 * 创建并应用筛选器到现有的Field
	 * 注意：这会影响后续添加的所有Field命令
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter", 
		meta = (DisplayName = "应用筛选器"))
	void ApplyFilter();

	/**
	 * 创建MetaDataFilter对象
	 * 根据当前的筛选配置创建UFieldSystemMetaDataFilter
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter", 
		meta = (DisplayName = "创建元数据筛选器"))
	UFieldSystemMetaDataFilter* CreateMetaDataFilter();

	/**
	 * 应用Actor类/Tag筛选（运行时实现）
	 * 扫描场景中的Actor，修改符合筛选条件的Actor的物理响应
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter", 
		meta = (DisplayName = "应用运行时筛选"))
	void ApplyRuntimeFiltering();

	/**
	 * 检查指定Actor是否应该被影响
	 * 根据类/Tag筛选条件判断
	 */
	UFUNCTION(BlueprintPure, Category = "Field|Filter", 
		meta = (DisplayName = "检查Actor是否受影响"))
	bool ShouldAffectActor(AActor* Actor) const;

	/**
	 * 禁用指定Actor对Field的响应
	 * 通过修改物理组件的属性实现
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter", 
		meta = (DisplayName = "禁用Actor的Field响应"))
	void DisableFieldResponseForActor(AActor* Actor);

	// ============================================================================
	// 便捷的蓝图方法
	// ============================================================================

	/**
	 * 快速排除角色类
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter|Presets", 
		meta = (DisplayName = "排除角色"))
	void ExcludeCharacters();

	/**
	 * 快速设置只影响破碎对象
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter|Presets", 
		meta = (DisplayName = "只影响破碎对象"))
	void OnlyAffectDestruction();

	/**
	 * 快速设置只影响动态对象
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter|Presets", 
		meta = (DisplayName = "只影响动态对象"))
	void OnlyAffectDynamic();

	/** 
	 * 获取筛选器（自动创建）
	 * 
	 * ⚠️ 重要：Field System的筛选器必须在每次调用Field函数时传入！
	 * 
	 * 使用示例：
	 * 1. 调用此函数获取筛选器
	 * 2. 将返回值传给 ApplyPhysicsField 的 MetaData 参数
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|Filter", 
		meta = (DisplayName = "获取筛选器"))
	UFieldSystemMetaDataFilter* GetCachedFilter() const;

	/**
	 * 应用Field到筛选后的GeometryCollection（手动模式）
	 * 
	 * 此方法专门为GeometryCollection设计，支持Actor类/Tag筛选
	 * 直接调用每个GC的内部Field系统，绕过全局Field System
	 * 
	 * @param Enabled 是否启用Field
	 * @param Target Field类型（力、速度、应变等）
	 * @param Field Field节点（手动创建，如Radial Vector）
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|GeometryCollection", 
		meta = (DisplayName = "应用Field到筛选的GC（手动）"))
	void ApplyFieldToFilteredGeometryCollections(
		UPARAM(DisplayName = "启用") bool Enabled,
		UPARAM(DisplayName = "物理类型") EFieldPhysicsType Target,
		UPARAM(DisplayName = "Field节点") UFieldNodeBase* Field);

	/**
	 * 应用当前FieldSystemComponent的Field到筛选后的GC（自动模式）
	 * 
	 * 自动使用FieldSystemComponent上配置的所有Field命令
	 * 适用于希望像FS_MasterField一样工作，但只影响特定Tag的GC
	 * 
	 * 使用场景：
	 * 1. 在XFieldSystemActor上配置好Field（如FS_MasterField）
	 * 2. 配置Tag筛选规则
	 * 3. 调用此方法，自动应用到筛选后的GC
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|GeometryCollection", 
		meta = (DisplayName = "应用当前Field到筛选的GC"))
	void ApplyCurrentFieldToFilteredGCs();

	/**
	 * 注册自己到筛选后的GC的InitializationFields（更优雅的方案）
	 * 
	 * ✨ 推荐方案！将此XFieldSystemActor添加到每个GC的InitializationFields数组
	 * GC会自动使用这些Field，无需手动调用ApplyCurrentFieldToFilteredGCs
	 * 
	 * 优势：
	 * - 自动化：GC自己会处理Field应用
	 * - 高性能：使用UE原生机制
	 * - 无需手动触发：一次设置，永久生效
	 * 
	 * 使用场景：
	 * 1. BeginPlay时自动调用（如果bAutoRegisterToGCs=true）
	 * 2. 或手动调用此方法
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|GeometryCollection", 
		meta = (DisplayName = "注册到筛选的GC"))
	void RegisterToFilteredGCs();

	/**
	 * 是否在BeginPlay时自动注册到筛选后的GC
	 * 
	 * ⚠️ 注意：自动注册适用于"持久性Field"场景
	 * 如果需要触发器控制（如触发器A影响GroupA，触发器B影响GroupB），
	 * 请禁用此选项，改用ApplyCurrentFieldToFilteredGCs()手动触发
	 * 
	 * 推荐配置：
	 * - 持久性影响（如重力场）：true
	 * - 触发器控制：false（改用ApplyCurrentFieldToFilteredGCs）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field|GeometryCollection|Auto",
		meta = (DisplayName = "自动注册到GC"))
	bool bAutoRegisterToGCs = false;

	/**
	 * 刷新GeometryCollection缓存
	 * 重新扫描场景，收集所有符合筛选条件的GeometryCollectionComponent
	 */
	UFUNCTION(BlueprintCallable, Category = "Field|GeometryCollection", 
		meta = (DisplayName = "刷新GC缓存"))
	void RefreshGeometryCollectionCache();

	/** 
	 * 设置缓存的筛选器（仅供C++和Library使用）
	 * 注意：这会覆盖BeginPlay时自动创建的筛选器
	 */
	void SetCachedFilter(UFieldSystemMetaDataFilter* Filter);

protected:
	/** 筛选器是否已应用 */
	bool bFilterApplied = false;

	/** 缓存的MetaDataFilter */
	UPROPERTY(Transient)
	TObjectPtr<UFieldSystemMetaDataFilter> CachedFilter;

	/** Spawn监听器句柄 */
	FDelegateHandle SpawnListenerHandle;

	/** 缓存的GeometryCollectionComponent列表（用于Tag筛选） */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UGeometryCollectionComponent>> CachedGeometryCollections;

	/** Actor Spawn事件回调 */
	UFUNCTION()
	void OnActorSpawned(AActor* SpawnedActor);

	/** 收集场景中符合筛选条件的GeometryCollectionComponent */
	void CollectGeometryCollections();

	/** 注册Spawn监听 */
	void RegisterSpawnListener();

	/** 注销Spawn监听 */
	void UnregisterSpawnListener();
};

