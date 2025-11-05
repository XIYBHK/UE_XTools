// Copyright Epic Games, Inc. All Rights Reserved.

#include "XFieldSystemActor.h"
#include "Field/FieldSystemComponent.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"  // For TActorIterator
#include "Components/PrimitiveComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "ChaosSolversModule.h"

AXFieldSystemActor::AXFieldSystemActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 设置默认值
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;  // 默认不Tick，只在需要时启用
}

void AXFieldSystemActor::BeginPlay()
{
	Super::BeginPlay();

	// 如果启用了筛选，在开始时应用
	if (bEnableFiltering)
	{
		ApplyFilter();
		
		// 收集GeometryCollectionComponent（用于Tag筛选）
		CollectGeometryCollections();
		
		// 自动注册到筛选后的GC（推荐方式）
		if (bAutoRegisterToGCs && CachedGeometryCollections.Num() > 0)
		{
			RegisterToFilteredGCs();
		}
		
		// 如果启用了Actor类/Tag筛选
		if (bEnableActorClassFilter || bEnableActorTagFilter)
		{
			if (bListenToActorSpawn)
			{
				// 优雅方案：监听Spawn事件
				RegisterSpawnListener();
				
				// 处理已存在的Actor（只在第一次需要）
				ApplyRuntimeFiltering();
			}
			else
			{
				// 传统方案：遍历场景
				ApplyRuntimeFiltering();
			}
		}
	}
}

void AXFieldSystemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 注销Spawn监听
	if (bListenToActorSpawn)
	{
		UnregisterSpawnListener();
	}

	Super::EndPlay(EndPlayReason);
}

UFieldSystemMetaDataFilter* AXFieldSystemActor::GetCachedFilter() const
{
	return CachedFilter;
}

void AXFieldSystemActor::SetCachedFilter(UFieldSystemMetaDataFilter* Filter)
{
	CachedFilter = Filter;
}

void AXFieldSystemActor::ApplyFilter()
{
	if (!bEnableFiltering)
	{
		return;
	}

	// 创建筛选器
	CachedFilter = CreateMetaDataFilter();
	
	if (CachedFilter)
	{
		bFilterApplied = true;
		
		// 注意：实际的筛选器应用需要在具体的Field命令中传入MetaData参数
		// 这里只是创建和缓存筛选器对象
		// 用户需要在蓝图中调用FieldSystemComponent的方法时传入这个Filter
		
		UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Filter applied - ObjectType=%d, FilterType=%d, PositionType=%d"),
			ObjectType.GetValue(), FilterType.GetValue(), PositionType.GetValue());
	}
}

UFieldSystemMetaDataFilter* AXFieldSystemActor::CreateMetaDataFilter()
{
	if (!bEnableFiltering)
	{
		return nullptr;
	}

	// 创建MetaDataFilter对象
	UFieldSystemMetaDataFilter* Filter = NewObject<UFieldSystemMetaDataFilter>(this);
	
	if (Filter)
	{
		// 设置筛选参数
		Filter->SetMetaDataFilterType(
			FilterType,
			ObjectType,
			PositionType
		);
	}

	return Filter;
}

bool AXFieldSystemActor::ShouldAffectActor(AActor* Actor) const
{
	if (!Actor || !bEnableFiltering)
	{
		return true;  // 未启用筛选时，影响所有Actor
	}

	// 检查Actor类筛选
	if (bEnableActorClassFilter)
	{
		UClass* ActorClass = Actor->GetClass();

		// 检查排除列表
		for (TSubclassOf<AActor> ExcludedClass : ExcludeActorClasses)
		{
			if (ExcludedClass && ActorClass->IsChildOf(ExcludedClass))
			{
				return false;  // 在排除列表中
			}
		}

		// 检查包含列表（如果有指定）
		if (IncludeActorClasses.Num() > 0)
		{
			bool bFoundInIncludeList = false;
			for (TSubclassOf<AActor> IncludedClass : IncludeActorClasses)
			{
				if (IncludedClass && ActorClass->IsChildOf(IncludedClass))
				{
					bFoundInIncludeList = true;
					break;
				}
			}
			if (!bFoundInIncludeList)
			{
				return false;  // 不在包含列表中
			}
		}
	}

	// 检查Actor Tag筛选
	if (bEnableActorTagFilter)
	{
		// 检查排除Tag
		for (const FName& ExcludedTag : ExcludeActorTags)
		{
			if (Actor->ActorHasTag(ExcludedTag))
			{
				return false;  // 带有排除Tag
			}
		}

		// 检查包含Tag（如果有指定）
		if (IncludeActorTags.Num() > 0)
		{
			bool bHasIncludedTag = false;
			for (const FName& IncludedTag : IncludeActorTags)
			{
				if (Actor->ActorHasTag(IncludedTag))
				{
					bHasIncludedTag = true;
					break;
				}
			}
			if (!bHasIncludedTag)
			{
				return false;  // 没有包含Tag
			}
		}
	}

	return true;  // 通过所有筛选
}

void AXFieldSystemActor::ExcludeCharacters()
{
	bEnableFiltering = true;
	bEnableActorClassFilter = true;
	
	// 添加ACharacter到排除列表
	ExcludeActorClasses.AddUnique(ACharacter::StaticClass());
	
	// 同时设置只影响Destruction对象
	ObjectType = EFieldObjectType::Field_Object_Destruction;
	
	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Configured to exclude Characters"));
}

void AXFieldSystemActor::OnlyAffectDestruction()
{
	bEnableFiltering = true;
	ObjectType = EFieldObjectType::Field_Object_Destruction;
	FilterType = EFieldFilterType::Field_Filter_Dynamic;
	
	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Configured to only affect Destruction objects"));
}

void AXFieldSystemActor::OnlyAffectDynamic()
{
	bEnableFiltering = true;
	FilterType = EFieldFilterType::Field_Filter_Dynamic;
	
	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Configured to only affect Dynamic objects"));
}

void AXFieldSystemActor::CollectGeometryCollections()
{
	CachedGeometryCollections.Empty();

	if (!bEnableFiltering || (!bEnableActorClassFilter && !bEnableActorTagFilter))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 遍历场景中所有GeometryCollectionComponent
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this)
		{
			continue;
		}

		// 检查是否应该影响此Actor
		if (!ShouldAffectActor(Actor))
		{
			continue;  // 不符合筛选条件，跳过
		}

		// 获取GeometryCollectionComponent
		UGeometryCollectionComponent* GC = Actor->FindComponentByClass<UGeometryCollectionComponent>();
		if (GC)
		{
			CachedGeometryCollections.Add(GC);
			UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Cached GeometryCollection from '%s'"), 
				*Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Collected %d GeometryCollections"), 
		CachedGeometryCollections.Num());
}

void AXFieldSystemActor::RefreshGeometryCollectionCache()
{
	CollectGeometryCollections();
}

void AXFieldSystemActor::ApplyFieldToFilteredGeometryCollections(
	bool Enabled,
	EFieldPhysicsType Target,
	UFieldNodeBase* Field)
{
	if (!Enabled || !Field)
	{
		return;
	}

	if (CachedGeometryCollections.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("XFieldSystemActor: No cached GeometryCollections! Call RefreshGeometryCollectionCache or enable filtering in BeginPlay."));
		return;
	}

	// 创建Field命令（注意：MetaData筛选器对GC不起作用，因为GC有自己的筛选机制）
	// 这里我们已经通过Tag/Class预先筛选了GC列表，所以不需要再传MetaData
	UFieldSystemMetaData* MetaData = nullptr;  // GC不需要MetaData筛选
	FFieldSystemCommand Command = FFieldObjectCommands::CreateFieldCommand(Target, Field, MetaData);

	int32 AppliedCount = 0;
	FChaosSolversModule* ChaosModule = FChaosSolversModule::GetModule();
	
	for (UGeometryCollectionComponent* GC : CachedGeometryCollections)
	{
		if (!GC || !GC->IsValidLowLevel())
		{
			continue;
		}

		// 获取PhysicsProxy（公开方法）
		FGeometryCollectionPhysicsProxy* PhysicsProxy = GC->GetPhysicsProxy();
		if (!PhysicsProxy)
		{
			continue;
		}

		// 获取Solver
		auto Solver = PhysicsProxy->GetSolver<Chaos::FPBDRigidsSolver>();
		if (!Solver)
		{
			continue;
		}

		// 初始化Field命令
		const FName OwnerName = GC->GetOwner() ? FName(*GC->GetOwner()->GetName()) : NAME_None;
		FFieldSystemCommand LocalCommand = Command;
		LocalCommand.InitFieldNodes(Solver->GetSolverTime(), OwnerName);

		// 提交命令到Solver（复制UGeometryCollectionComponent::DispatchFieldCommand的实现）
		Solver->EnqueueCommandImmediate([Solver, PhysicsProxy, NewCommand = LocalCommand]()
		{
			PhysicsProxy->BufferCommand(Solver, NewCommand);
		});

		AppliedCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Applied field to %d/%d GeometryCollections"), 
		AppliedCount, CachedGeometryCollections.Num());
}

void AXFieldSystemActor::RegisterToFilteredGCs()
{
	if (CachedGeometryCollections.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("XFieldSystemActor: No cached GeometryCollections to register!"));
		return;
	}

	int32 RegisteredCount = 0;

	for (UGeometryCollectionComponent* GC : CachedGeometryCollections)
	{
		if (!GC || !GC->IsValidLowLevel())
		{
			continue;
		}

		// 检查是否已经在InitializationFields中
		if (GC->InitializationFields.Contains(this))
		{
			continue;  // 已注册，跳过
		}

		// 添加到InitializationFields
		GC->InitializationFields.Add(this);
		RegisteredCount++;

		UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Registered to GeometryCollection '%s'"), 
			*GC->GetOwner()->GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Registered to %d/%d GeometryCollections"), 
		RegisteredCount, CachedGeometryCollections.Num());
}

void AXFieldSystemActor::ApplyCurrentFieldToFilteredGCs()
{
	if (CachedGeometryCollections.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("XFieldSystemActor: No cached GeometryCollections! Call RefreshGeometryCollectionCache or enable filtering in BeginPlay."));
		return;
	}

	UFieldSystemComponent* FieldComp = GetFieldSystemComponent();
	if (!FieldComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("XFieldSystemActor: No FieldSystemComponent found!"));
		return;
	}

	// 获取FieldSystemComponent上的所有命令（使用更简洁的GetConstructionFields）
	const TArray<FFieldSystemCommand>& ConstructionFields = FieldComp->GetConstructionFields();
	
	if (ConstructionFields.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("XFieldSystemActor: FieldSystemComponent has no construction fields configured!"));
		return;
	}

	int32 TotalApplied = 0;

	// 遍历每个GC
	for (UGeometryCollectionComponent* GC : CachedGeometryCollections)
	{
		if (!GC || !GC->IsValidLowLevel())
		{
			continue;
		}

		FGeometryCollectionPhysicsProxy* PhysicsProxy = GC->GetPhysicsProxy();
		if (!PhysicsProxy)
		{
			continue;
		}

		auto Solver = PhysicsProxy->GetSolver<Chaos::FPBDRigidsSolver>();
		if (!Solver)
		{
			continue;
		}

		// 应用所有命令到此GC
		for (const FFieldSystemCommand& Command : ConstructionFields)
		{
			if (!Command.RootNode)
			{
				continue;
			}

			// 初始化Field命令（需要拷贝，因为InitFieldNodes会修改）
			const FName OwnerName = GC->GetOwner() ? FName(*GC->GetOwner()->GetName()) : NAME_None;
			FFieldSystemCommand LocalCommand = Command;
			LocalCommand.InitFieldNodes(Solver->GetSolverTime(), OwnerName);

			// 提交命令到Solver
			Solver->EnqueueCommandImmediate([Solver, PhysicsProxy, NewCommand = LocalCommand]()
			{
				PhysicsProxy->BufferCommand(Solver, NewCommand);
			});

			TotalApplied++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Applied %d construction fields to %d GeometryCollections"), 
		ConstructionFields.Num(), CachedGeometryCollections.Num());
}

void AXFieldSystemActor::ApplyRuntimeFiltering()
{
	if (!bEnableActorClassFilter && !bEnableActorTagFilter)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 ProcessedCount = 0;
	int32 ExcludedCount = 0;

	// 遍历场景中所有Actor
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this)
		{
			continue;
		}

		ProcessedCount++;

		// 检查是否应该排除此Actor
		if (!ShouldAffectActor(Actor))
		{
			DisableFieldResponseForActor(Actor);
			ExcludedCount++;
			
			UE_LOG(LogTemp, Verbose, TEXT("XFieldSystemActor: Excluded Actor '%s' from Field effects"), 
				*Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Runtime filtering applied - Processed %d actors, Excluded %d"), 
		ProcessedCount, ExcludedCount);
}

void AXFieldSystemActor::RegisterSpawnListener()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 注册Actor Spawn回调，保存句柄
	SpawnListenerHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(
		this, &AXFieldSystemActor::OnActorSpawned));

	UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Registered spawn listener"));
}

void AXFieldSystemActor::UnregisterSpawnListener()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 注销回调（使用保存的句柄）
	if (SpawnListenerHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(SpawnListenerHandle);
		SpawnListenerHandle.Reset();
		
		UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Unregistered spawn listener"));
	}
}

void AXFieldSystemActor::OnActorSpawned(AActor* SpawnedActor)
{
	if (!SpawnedActor || SpawnedActor == this)
	{
		return;
	}

	// 检查是否应该排除此Actor
	if (!ShouldAffectActor(SpawnedActor))
	{
		DisableFieldResponseForActor(SpawnedActor);
		
		UE_LOG(LogTemp, Verbose, TEXT("XFieldSystemActor: Spawn listener excluded '%s'"), 
			*SpawnedActor->GetName());
	}
	else
	{
		// 如果应该影响此Actor，检查是否有GeometryCollectionComponent
		UGeometryCollectionComponent* GC = SpawnedActor->FindComponentByClass<UGeometryCollectionComponent>();
		if (GC)
		{
			CachedGeometryCollections.Add(GC);
			UE_LOG(LogTemp, Log, TEXT("XFieldSystemActor: Added spawned GeometryCollection from '%s'"), 
				*SpawnedActor->GetName());
		}
	}
}

void AXFieldSystemActor::DisableFieldResponseForActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// 获取Actor的所有物理组件
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive || !Primitive->IsSimulatingPhysics())
		{
			continue;
		}

		// 设置为Kinematic：不响应外力，但保留碰撞
		if (FBodyInstance* BodyInstance = Primitive->GetBodyInstance())
		{
			// SetInstanceSimulatePhysics(false) 会将物理状态设为Kinematic
			// 参数：bSimulate=false, bMaintainPhysicsBlending=false, bUpdatePhysicsProperties=true
			BodyInstance->SetInstanceSimulatePhysics(false, false, true);
			
			UE_LOG(LogTemp, Log, TEXT("  ✓ Set kinematic for '%s' (Field response blocked, collision retained)"), 
				*Primitive->GetName());
		}
		else
		{
			// Fallback: 如果无法获取BodyInstance，使用组件级方法
			Primitive->SetSimulatePhysics(false);
			Primitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			
			UE_LOG(LogTemp, Log, TEXT("  ✓ Disabled physics for '%s' (fallback method)"), 
				*Primitive->GetName());
		}
	}
}

