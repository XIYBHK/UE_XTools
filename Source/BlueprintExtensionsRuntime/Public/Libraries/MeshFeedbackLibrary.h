#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MeshFeedbackLibrary.generated.h"

class AActor;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EXToolsMeshFeedbackComponentType : uint8
{
	None = 0 UMETA(Hidden),
	StaticMesh = 1 << 0 UMETA(DisplayName = "静态网格体"),
	SkeletalMesh = 1 << 1 UMETA(DisplayName = "骨骼网格体"),
	GeometryCollection = 1 << 2 UMETA(DisplayName = "几何体集"),
	OtherMesh = 1 << 3 UMETA(DisplayName = "其他网格体")
};
ENUM_CLASS_FLAGS(EXToolsMeshFeedbackComponentType)

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsMeshFeedbackCollectOptions
{
	GENERATED_BODY()

	static constexpr int32 AllMeshTypes =
		static_cast<int32>(EXToolsMeshFeedbackComponentType::StaticMesh) |
		static_cast<int32>(EXToolsMeshFeedbackComponentType::SkeletalMesh) |
		static_cast<int32>(EXToolsMeshFeedbackComponentType::GeometryCollection) |
		static_cast<int32>(EXToolsMeshFeedbackComponentType::OtherMesh);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (Bitmask, BitmaskEnum = "/Script/BlueprintExtensionsRuntime.EXToolsMeshFeedbackComponentType", DisplayName = "网格体类型"))
	int32 MeshTypes = AllMeshTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "组件Tag", ToolTip = "留空时不按Tag过滤；有值时组件必须同时满足该Tag和网格体类型筛选。"))
	FName RequiredComponentTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "包含ChildActor组件"))
	bool bIncludeFromChildActors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "包含隐藏组件"))
	bool bIncludeHiddenComponents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "包含未注册组件"))
	bool bIncludeUnregisteredComponents = false;
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsMeshFeedbackMIDOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "清空输出数组"))
	bool bClearOutputs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "跳过空材质槽"))
	bool bSkipNullMaterials = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "复用已有MID", ToolTip = "材质槽已经是动态材质实例时直接复用，避免重复创建。"))
	bool bReuseExistingMID = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "MID名称"))
	FName OptionalMIDName = NAME_None;
};

USTRUCT(BlueprintType)
struct BLUEPRINTEXTENSIONSRUNTIME_API FXToolsMeshFeedbackMIDEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "XTools|蓝图扩展|反馈", meta = (DisplayName = "网格体组件"))
	UMeshComponent* MeshComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|蓝图扩展|反馈", meta = (DisplayName = "材质槽索引"))
	int32 MaterialIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|蓝图扩展|反馈", meta = (DisplayName = "原始材质"))
	UMaterialInterface* OriginalMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|蓝图扩展|反馈", meta = (DisplayName = "动态材质实例"))
	UMaterialInstanceDynamic* MID = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|蓝图扩展|反馈", meta = (DisplayName = "原本就是MID"))
	bool bWasAlreadyMID = false;

	UPROPERTY(BlueprintReadOnly, Category = "XTools|蓝图扩展|反馈", meta = (DisplayName = "本次创建"))
	bool bCreatedMID = false;
};

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UMeshFeedbackLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "收集Actor网格体组件",
			Keywords = "Mesh Feedback Collect Static Skeletal Geometry Tag 网格体 反馈 收集 静态 骨骼 几何体集",
			ToolTip = "从目标Actor收集网格体组件。默认收集静态网格体、骨骼网格体、几何体集和其他Mesh；组件Tag留空时不启用Tag过滤，有值时必须同时满足Tag和类型筛选。"))
	static void CollectActorMeshComponents(
		UPARAM(DisplayName = "目标Actor") AActor* TargetActor,
		UPARAM(DisplayName = "收集选项") const FXToolsMeshFeedbackCollectOptions& Options,
		UPARAM(DisplayName = "网格体组件") TArray<UMeshComponent*>& OutMeshComponents);

	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "为网格体创建动态材质实例",
			Keywords = "Mesh Feedback MID Dynamic Material Instance 创建 动态材质 反馈",
			ToolTip = "为传入的网格体组件逐材质槽创建或复用动态材质实例，并返回MID数组和详细条目。"))
	static void CreateDynamicMaterialInstancesForMeshes(
		UPARAM(DisplayName = "网格体组件") const TArray<UMeshComponent*>& MeshComponents,
		UPARAM(DisplayName = "MID选项") const FXToolsMeshFeedbackMIDOptions& Options,
		UPARAM(DisplayName = "动态材质实例") TArray<UMaterialInstanceDynamic*>& OutMIDs,
		UPARAM(DisplayName = "MID条目") TArray<FXToolsMeshFeedbackMIDEntry>& OutEntries);

	UFUNCTION(BlueprintCallable, Category = "XTools|蓝图扩展|反馈",
		meta = (DisplayName = "为Actor网格体创建动态材质实例",
			Keywords = "Mesh Feedback Actor MID Dynamic Material Instance 收集 创建 动态材质",
			ToolTip = "先按收集选项从Actor获取网格体组件，再为这些组件创建或复用动态材质实例。适合默认全量初始化反馈材质。"))
	static void CreateDynamicMaterialInstancesForActorMeshes(
		UPARAM(DisplayName = "目标Actor") AActor* TargetActor,
		UPARAM(DisplayName = "收集选项") const FXToolsMeshFeedbackCollectOptions& CollectOptions,
		UPARAM(DisplayName = "MID选项") const FXToolsMeshFeedbackMIDOptions& MIDOptions,
		UPARAM(DisplayName = "网格体组件") TArray<UMeshComponent*>& OutMeshComponents,
		UPARAM(DisplayName = "动态材质实例") TArray<UMaterialInstanceDynamic*>& OutMIDs,
		UPARAM(DisplayName = "MID条目") TArray<FXToolsMeshFeedbackMIDEntry>& OutEntries);
};
