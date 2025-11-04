#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"

#include "TraceExtensionsLibrary.generated.h"


UENUM(BlueprintType)
enum class EDebugTraceType : uint8
{
	None UMETA(DisplayName = "无"),
	ForOneFrame UMETA(DisplayName = "单帧"),
	ForDuration UMETA(DisplayName = "持续时长"),
	Persistent UMETA(DisplayName = "永久显示")
};

UCLASS()
class BLUEPRINTEXTENSIONSRUNTIME_API UTraceExtensionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region QueryNames

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetTraceTypeQueryNames();

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetObjectTypeQueryNames();
	
	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "追踪通道类型", CompactNodeTitle = "TraceChannelType", ToolTip = "将追踪通道的显示名称转换为枚举字符串"))
	static void TraceChannelType(
		UPARAM(DisplayName = "通道名称", meta = (GetOptions = GetTraceTypeQueryNames, ToolTip = "从下拉列表选择的通道显示名称")) FName InputName,
		UPARAM(DisplayName = "枚举字符串") FString& OutString
	);

	UFUNCTION(BlueprintPure, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "追踪对象类型", CompactNodeTitle = "TraceObjectType", ToolTip = "将追踪对象的显示名称转换为枚举字符串"))
	static void TraceObjectType(
		UPARAM(DisplayName = "对象名称", meta = (GetOptions = GetObjectTypeQueryNames, ToolTip = "从下拉列表选择的对象显示名称")) FName InputName,
		UPARAM(DisplayName = "枚举字符串") FString& OutString
	);

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————
	
#pragma region LineTrace
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "线性追踪(通道)", CompactNodeTitle = "TraceLineChannel", ToolTip = "从起点到终点发射一条射线，按通道类型检测碰撞", AdvancedDisplay = "TraceColor, TraceHitColor, DrawTime", WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static void TraceLineChannel(
		UObject* WorldContextObject,
		UPARAM(DisplayName = "起点") FVector Start,
		UPARAM(DisplayName = "终点") FVector End,
		UPARAM(DisplayName = "通道类型") FString TraceChannelType,
		UPARAM(DisplayName = "复杂碰撞") bool bTraceComplex,
		UPARAM(DisplayName = "忽略的Actor") const TArray<AActor*>& ActorsToIgnore,
		UPARAM(DisplayName = "调试绘制") EDebugTraceType DrawDebugType,
		UPARAM(DisplayName = "是否命中") bool& Block, 
		UPARAM(DisplayName = "命中点") FVector& ImpactPoint, 
		UPARAM(DisplayName = "命中结果") FHitResult& OutHit,
		UPARAM(DisplayName = "射线颜色") FLinearColor TraceColor = FLinearColor::Green,
		UPARAM(DisplayName = "命中颜色") FLinearColor TraceHitColor = FLinearColor::Red,
		UPARAM(DisplayName = "绘制时长") float DrawTime = 0.0f
	);
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "Z轴线性追踪(通道)", CompactNodeTitle = "TraceLineZ", ToolTip = "以指定位置为中心，沿Z轴向上向下发射射线检测碰撞(常用于地面检测)", AdvancedDisplay = "TraceColor, TraceHitColor, DrawTime", WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static void TraceLineChannelOnAxisZ(
		UObject* WorldContextObject,
		UPARAM(DisplayName = "中心位置") FVector PivotLocation,
		UPARAM(DisplayName = "检测范围") float TraceRange,
		UPARAM(DisplayName = "通道类型") FString TraceChannelType,
		UPARAM(DisplayName = "复杂碰撞") bool bTraceComplex,
		UPARAM(DisplayName = "忽略的Actor") const TArray<AActor*>& ActorsToIgnore,
		UPARAM(DisplayName = "调试绘制") EDebugTraceType DrawDebugType,
		UPARAM(DisplayName = "是否命中") bool& Block,
		UPARAM(DisplayName = "命中点") FVector& ImpactPoint,
		UPARAM(DisplayName = "命中结果") FHitResult& OutHit,
		UPARAM(DisplayName = "射线颜色") FLinearColor TraceColor = FLinearColor::Green,
		UPARAM(DisplayName = "命中颜色") FLinearColor TraceHitColor = FLinearColor::Red,
		UPARAM(DisplayName = "绘制时长") float DrawTime = 0.0f
	);
	
	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "延伸线性追踪(通道)", CompactNodeTitle = "TraceLineExt", ToolTip = "根据起点和方向点，按指定距离延伸射线检测碰撞(常用于武器射击)", AdvancedDisplay = "TraceColor, TraceHitColor, DrawTime", WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static void TraceLineChannelByExtension(
		UObject* WorldContextObject,
		UPARAM(DisplayName = "起点") FVector Start,
		UPARAM(DisplayName = "方向点") FVector End,
		UPARAM(DisplayName = "延伸距离") float TraceRange,
		UPARAM(DisplayName = "通道类型") FString TraceChannelType,
		UPARAM(DisplayName = "复杂碰撞") bool bTraceComplex,
		UPARAM(DisplayName = "忽略的Actor") const TArray<AActor*>& ActorsToIgnore,
		UPARAM(DisplayName = "调试绘制") EDebugTraceType DrawDebugType,
		UPARAM(DisplayName = "是否命中") bool& Block,
		UPARAM(DisplayName = "命中点") FVector& ImpactPoint,
		UPARAM(DisplayName = "命中结果") FHitResult& OutHit,
		UPARAM(DisplayName = "射线颜色") FLinearColor TraceColor = FLinearColor::Green,
		UPARAM(DisplayName = "命中颜色") FLinearColor TraceHitColor = FLinearColor::Red,
		UPARAM(DisplayName = "绘制时长") float DrawTime = 0.0f
	);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "线性追踪(对象)", CompactNodeTitle = "TraceLineObject", ToolTip = "从起点到终点发射一条射线，按对象类型检测碰撞(可同时检测多种对象类型)", AdvancedDisplay = "TraceColor, TraceHitColor, DrawTime", WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static void TraceLineObject(
		UObject* WorldContextObject,
		UPARAM(DisplayName = "起点") FVector Start,
		UPARAM(DisplayName = "终点") FVector End,
		UPARAM(DisplayName = "对象类型") const TArray<FString>& TraceObjectType,
		UPARAM(DisplayName = "复杂碰撞") bool bTraceComplex,
		UPARAM(DisplayName = "忽略的Actor") const TArray<AActor*>& ActorsToIgnore,
		UPARAM(DisplayName = "调试绘制") EDebugTraceType DrawDebugType,
		UPARAM(DisplayName = "是否命中") bool& Block,
		UPARAM(DisplayName = "命中点") FVector& ImpactPoint,
		UPARAM(DisplayName = "命中结果") FHitResult& OutHit,
		UPARAM(DisplayName = "射线颜色") FLinearColor TraceColor = FLinearColor::Green,
		UPARAM(DisplayName = "命中颜色") FLinearColor TraceHitColor = FLinearColor::Red,
		UPARAM(DisplayName = "绘制时长") float DrawTime = 0.0f
	);

#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————

#pragma region SphereTrace

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "球形追踪(通道)", CompactNodeTitle = "TraceSphereChannel", ToolTip = "从起点到终点扫描球形区域，按通道类型检测碰撞(常用于范围攻击检测)", AdvancedDisplay = "TraceColor, TraceHitColor, DrawTime", WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static void TraceSphereChannel(
		UObject* WorldContextObject,
		UPARAM(DisplayName = "起点") FVector Start,
		UPARAM(DisplayName = "终点") FVector End,
		UPARAM(DisplayName = "半径") float Radius,
		UPARAM(DisplayName = "通道类型") FString TraceChannelType,
		UPARAM(DisplayName = "复杂碰撞") bool bTraceComplex,
		UPARAM(DisplayName = "忽略的Actor") const TArray<AActor*>& ActorsToIgnore,
		UPARAM(DisplayName = "调试绘制") EDebugTraceType DrawDebugType,
		UPARAM(DisplayName = "是否命中") bool& Block,
		UPARAM(DisplayName = "命中点") FVector& ImpactPoint,
		UPARAM(DisplayName = "命中结果") FHitResult& OutHit,
		UPARAM(DisplayName = "球体颜色") FLinearColor TraceColor = FLinearColor::Green,
		UPARAM(DisplayName = "命中颜色") FLinearColor TraceHitColor = FLinearColor::Red,
		UPARAM(DisplayName = "绘制时长") float DrawTime = 0.0f
	);

	UFUNCTION(BlueprintCallable, Category = "XTools|Blueprint Extensions|Trace", meta = (DisplayName = "球形追踪(对象)", CompactNodeTitle = "TraceSphereObject", ToolTip = "从起点到终点扫描球形区域，按对象类型检测碰撞(可同时检测多种对象类型)", AdvancedDisplay = "TraceColor, TraceHitColor, DrawTime", WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore"))
	static void TraceSphereObject(
		UObject* WorldContextObject,
		UPARAM(DisplayName = "起点") FVector Start,
		UPARAM(DisplayName = "终点") FVector End,
		UPARAM(DisplayName = "半径") float Radius,
		UPARAM(DisplayName = "对象类型") const TArray<FString>& TraceObjectType,
		UPARAM(DisplayName = "复杂碰撞") bool bTraceComplex,
		UPARAM(DisplayName = "忽略的Actor") const TArray<AActor*>& ActorsToIgnore,
		UPARAM(DisplayName = "调试绘制") EDebugTraceType DrawDebugType,
		UPARAM(DisplayName = "是否命中") bool& Block,
		UPARAM(DisplayName = "命中点") FVector& ImpactPoint,
		UPARAM(DisplayName = "命中结果") FHitResult& OutHit,
		UPARAM(DisplayName = "球体颜色") FLinearColor TraceColor = FLinearColor::Green,
		UPARAM(DisplayName = "命中颜色") FLinearColor TraceHitColor = FLinearColor::Red,
		UPARAM(DisplayName = "绘制时长") float DrawTime = 0.0f
	);
	
#pragma endregion

//————————————————————————————————————————————————————————————————————————————————————————————————————
	
};

