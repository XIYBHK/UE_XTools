#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "SortLibrary.generated.h"

UENUM(BlueprintType)
enum class ECoordinateAxis : uint8
{
    X UMETA(DisplayName = "X轴"),
    Y UMETA(DisplayName = "Y轴"),
    Z UMETA(DisplayName = "Z轴")
};

/**
 * 提供一个可从蓝图调用的、用于排序和数组操作的静态函数库。
 */
UCLASS(meta=(
    DisplayName="排序工具",
    autoCollapseCategories = "XTools|排序|Actor,XTools|排序|基础类型,XTools|排序|向量,XTools|数组操作|反转,XTools|数组操作|截取,XTools|数组操作|去重"
))
class SORT_API USortLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    //~ Actor排序函数
    // =================================================================================================

    /** 根据与指定位置的距离对Actor数组进行排序，并返回原始索引 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|Actor", 
        meta = (
            DisplayName = "根据距离排序Actor数组",
            Keywords = "排序,距离,Actor,索引",
            AutoCreateRefTerm = "Location",
            bAscending = "true",
            ToolTip = "将Actor数组按照与指定位置的距离进行排序，并返回排序后元素对应的原始索引。\n参数:\nActors - 要排序的Actor数组\nLocation - 参考位置\nbAscending - true为升序（从近到远），false为降序（从远到近）\nb2DDistance - true则忽略Z轴计算距离\n返回值:\nSortedActors - 排序后的Actor数组\nOriginalIndices - 排序后每个元素在原数组中的索引\nSortedDistances - 排序后每个Actor到参考位置的距离"
        ))
    static void SortActorsByDistance(UPARAM(DisplayName="Actor数组") const TArray<AActor*>& Actors, 
        UPARAM(DisplayName="参考位置") const FVector& Location, 
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="2D距离") bool b2DDistance,
        UPARAM(DisplayName="排序后数组") TArray<AActor*>& SortedActors, 
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序距离") TArray<float>& SortedDistances);

    /** 根据Actor的Z坐标（高度）进行排序，并返回原始索引 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|Actor", 
        meta = (
            DisplayName = "根据高度排序Actor数组",
            Keywords = "排序,高度,Z轴,Actor,索引",
            bAscending = "true",
            ToolTip = "将Actor数组按照Z轴坐标（高度）进行排序，并返回排序后元素对应的原始索引。\n参数:\nActors - 要排序的Actor数组\nbAscending - true为升序（从低到高），false为降序（从高到低）\n返回值:\nSortedActors - 排序后的Actor数组\nOriginalIndices - 排序后每个元素在原数组中的索引"
        ))
    static void SortActorsByHeight(UPARAM(DisplayName="Actor数组") const TArray<AActor*>& Actors, 
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<AActor*>& SortedActors, 
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices);

    /** 根据Actor在指定坐标轴上的值进行排序 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|Actor", 
        meta = (
            DisplayName = "根据坐标排序Actor数组",
            Keywords = "排序,坐标,轴,Actor,索引,XYZ",
            bAscending = "true",
            ToolTip = "将Actor数组按照指定坐标轴上的值进行排序。\n参数:\nActors - 要排序的Actor数组\nAxis - 要排序的坐标轴\nbAscending - true为升序（从小到大），false为降序（从大到小）\n返回值:\nSortedActors - 排序后的Actor数组\nOriginalIndices - 排序后每个元素在原数组中的索引\nSortedAxisValues - 排序后每个Actor在指定轴上的坐标值"
        ))
    static void SortActorsByAxis(
        UPARAM(DisplayName="Actor数组") const TArray<AActor*>& Actors,
        UPARAM(DisplayName="坐标轴") ECoordinateAxis Axis,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<AActor*>& SortedActors,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序坐标") TArray<float>& SortedAxisValues);

    /** 根据Actor相对于指定方向的夹角进行排序 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|Actor", 
        meta = (
            DisplayName = "根据夹角排序Actor数组",
            Keywords = "排序,夹角,方向,Actor,索引,角度",
            AutoCreateRefTerm = "Center,Direction",
            bAscending = "true",
            ToolTip = "将Actor数组按照与指定方向的夹角进行排序。\n参数:\nActors - 要排序的Actor数组\nCenter - 中心点位置\nDirection - 参考方向\nbAscending - true为升序（从小到大），false为降序（从大到小）\nb2DAngle - true则在XY平面上计算夹角\n返回值:\nSortedActors - 排序后的Actor数组\nOriginalIndices - 排序后每个元素在原数组中的索引\nSortedAngles - 排序后每个Actor与参考方向的夹角（度数）"
        ))
    static void SortActorsByAngle(
        UPARAM(DisplayName="Actor数组") const TArray<AActor*>& Actors,
        UPARAM(DisplayName="中心点") const FVector& Center,
        UPARAM(DisplayName="参考方向") const FVector& Direction,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="2D夹角") bool b2DAngle,
        UPARAM(DisplayName="排序后数组") TArray<AActor*>& SortedActors,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序夹角") TArray<float>& SortedAngles);

    /** 根据Actor相对于中心点的方位角进行排序 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|Actor", 
        meta = (
            DisplayName = "根据方位角排序Actor数组",
            Keywords = "排序,方位角,方向,Actor,索引,角度,指南针",
            AutoCreateRefTerm = "Center",
            bAscending = "true",
            ToolTip = "将Actor数组按照相对于中心点的方位角进行排序（以正北为0度，顺时针计算）。\n参数:\nActors - 要排序的Actor数组\nCenter - 中心点位置\nbAscending - true为升序（从小到大），false为降序（从大到小）\n返回值:\nSortedActors - 排序后的Actor数组\nOriginalIndices - 排序后每个元素在原数组中的索引\nSortedAzimuths - 排序后每个Actor的方位角（0-360度，0为正北，90为正东）"
        ))
    static void SortActorsByAzimuth(
        UPARAM(DisplayName="Actor数组") const TArray<AActor*>& Actors,
        UPARAM(DisplayName="中心点") const FVector& Center,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<AActor*>& SortedActors,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序方位角") TArray<float>& SortedAzimuths);

    /** 根据Actor相对于中心点的夹角和距离进行加权排序 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|Actor", 
        meta = (
            DisplayName = "根据夹角和距离排序Actor数组",
            Keywords = "排序,夹角,距离,权重,方向,Actor,索引,角度",
            AutoCreateRefTerm = "Center,Direction",
            bAscending = "true",
            ToolTip = "将Actor数组按照与参考方向的夹角和到中心点的距离进行加权排序。\n最大夹角和最大距离用于过滤，设为0表示不限制。\n当权重都为0时默认只按夹角排序，否则按权重计算综合评分进行排序。\n2D夹角选项开启时会忽略Z轴，在XY平面上计算。"
        ))
    static void SortActorsByAngleAndDistance(
        UPARAM(DisplayName="Actor数组") const TArray<AActor*>& Actors,
        UPARAM(DisplayName="中心点") const FVector& Center,
        UPARAM(DisplayName="参考方向") const FVector& Direction,
        UPARAM(DisplayName="最大夹角") float MaxAngle,
        UPARAM(DisplayName="最大距离") float MaxDistance,
        UPARAM(DisplayName="夹角权重") float AngleWeight,
        UPARAM(DisplayName="距离权重") float DistanceWeight,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="2D夹角") bool b2DAngle,
        UPARAM(DisplayName="排序后数组") TArray<AActor*>& SortedActors,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序夹角") TArray<float>& SortedAngles,
        UPARAM(DisplayName="已排序距离") TArray<float>& SortedDistances);
    
    //~ 基础类型排序函数
    // =================================================================================================

    /** 对整数数组进行排序 */
    UFUNCTION(BlueprintCallable,
        Category = "XTools|排序|基础类型", 
        meta = (
            DisplayName = "排序整数数组",
            Keywords = "排序,整数,数字,索引",
            bAscending = "true",
            ToolTip = "对整数数组进行排序，并返回排序后元素对应的原始索引。\n参数:\nInArray - 要排序的整数数组\nbAscending - true为升序（从小到大），false为降序（从大到小）\n返回值:\nSortedArray - 排序后的数组\nOriginalIndices - 排序后每个元素在原数组中的索引"
        ))
    static void SortIntegerArray(UPARAM(DisplayName="输入数组") const TArray<int32>& InArray, 
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<int32>& SortedArray, 
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices);

    /** 对浮点数数组进行排序 */
    UFUNCTION(BlueprintCallable,
        Category = "XTools|排序|基础类型", 
        meta = (
            DisplayName = "排序浮点数数组",
            Keywords = "排序,浮点数,小数,索引",
            bAscending = "true",
            ToolTip = "对浮点数数组进行排序，并返回排序后元素对应的原始索引。\n参数:\nInArray - 要排序的浮点数数组\nbAscending - true为升序（从小到大），false为降序（从大到小）\n返回值:\nSortedArray - 排序后的数组\nOriginalIndices - 排序后每个元素在原数组中的索引"
        ))
    static void SortFloatArray(UPARAM(DisplayName="输入数组") const TArray<float>& InArray, 
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<float>& SortedArray, 
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices);

    /** 对字符串数组进行排序 */
    UFUNCTION(BlueprintCallable,
        Category = "XTools|排序|基础类型", 
        meta = (
            DisplayName = "排序字符串数组",
            Keywords = "排序,字符串,文本,索引",
            bAscending = "true",
            ToolTip = "对字符串数组进行排序，按照字典序（lexicographical order）进行比较，并返回排序后元素对应的原始索引。\n参数:\nInArray - 要排序的字符串数组\nbAscending - true为升序（字典序），false为降序（字典序）\n返回值:\nSortedArray - 排序后的数组\nOriginalIndices - 排序后每个元素在原数组中的索引"
        ))
    static void SortStringArray(UPARAM(DisplayName="输入数组") const TArray<FString>& InArray, 
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<FString>& SortedArray, 
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices);

    /** 根据字典序对命名数组进行排序 */
    UFUNCTION(BlueprintCallable,
        Category = "XTools|排序|基础类型", 
        meta = (
            DisplayName = "排序命名数组",
            Keywords = "排序,命名,名称,FName,索引",
            bAscending = "true",
            ToolTip = "将命名数组按照本地化规则进行排序，支持中文拼音排序。\n当输入为中文时，将按照拼音顺序排序。"
        ))
    static void SortNameArray(
        UPARAM(DisplayName="命名数组") const TArray<FName>& InArray,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<FName>& SortedArray,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices);
    
    //~ 向量排序函数
    // =================================================================================================

    /** 根据向量在指定轴上的投影进行排序 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|向量", 
        meta = (
            DisplayName = "根据投影排序向量数组",
            Keywords = "排序,向量,轴向,投影,索引",
            AutoCreateRefTerm = "Direction",
            bAscending = "true",
            ToolTip = "将向量数组按照在指定方向上的投影值进行排序。\n参数:\nVectors - 要排序的向量数组\nDirection - 投影方向\nbAscending - true为升序（从小到大），false为降序（从大到小）\n返回值:\nSortedVectors - 排序后的向量数组\nOriginalIndices - 排序后每个元素在原数组中的索引\nSortedProjections - 排序后每个向量在指定方向上的投影值"
        ))
    static void SortVectorsByProjection(
        UPARAM(DisplayName="向量数组") const TArray<FVector>& Vectors,
        UPARAM(DisplayName="投影方向") const FVector& Direction,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<FVector>& SortedVectors,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序投影") TArray<float>& SortedProjections);

    /** 根据向量的长度进行排序 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|向量", 
        meta = (
            DisplayName = "根据长度排序向量数组",
            Keywords = "排序,向量,长度,大小,索引",
            bAscending = "true",
            ToolTip = "将向量数组按照长度进行排序。\n参数:\nVectors - 要排序的向量数组\nbAscending - true为升序（从短到长），false为降序（从长到短）\n返回值:\nSortedVectors - 排序后的向量数组\nOriginalIndices - 排序后每个元素在原数组中的索引\nSortedLengths - 排序后每个向量的长度"
        ))
    static void SortVectorsByLength(
        UPARAM(DisplayName="向量数组") const TArray<FVector>& Vectors,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<FVector>& SortedVectors,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序长度") TArray<float>& SortedLengths);

    /** 根据向量在指定坐标轴上的值进行排序 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|排序|向量", 
        meta = (
            DisplayName = "根据坐标排序向量数组",
            Keywords = "排序,向量,坐标,XYZ,索引",
            bAscending = "true",
            ToolTip = "将向量数组按照指定坐标轴上的值进行排序。\n参数:\nVectors - 要排序的向量数组\nAxis - 要排序的坐标轴\nbAscending - true为升序（从小到大），false为降序（从大到小）\n返回值:\nSortedVectors - 排序后的向量数组\nOriginalIndices - 排序后每个元素在原数组中的索引\nSortedAxisValues - 排序后每个向量在指定轴上的值"
        ))
    static void SortVectorsByAxis(
        UPARAM(DisplayName="向量数组") const TArray<FVector>& Vectors,
        UPARAM(DisplayName="坐标轴") ECoordinateAxis Axis,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="排序后数组") TArray<FVector>& SortedVectors,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices,
        UPARAM(DisplayName="已排序坐标") TArray<float>& SortedAxisValues);

    //~ 数组截取函数
    // =================================================================================================

    /** 根据索引范围截取Actor数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "由索引截取Actor数组",
            Keywords = "截取,索引,Actor,数组",
            ToolTip = "根据索引范围截取Actor数组。\n参数:\nInArray - 要截取的Actor数组\nStartIndex - 起始索引（包含）\nEndIndex - 结束索引（包含）\n返回值:\nOutArray - 截取后的数组"
        ))
    static void SliceActorArrayByIndices(
        UPARAM(DisplayName="输入数组") const TArray<AActor*>& InArray,
        UPARAM(DisplayName="起始索引") int32 StartIndex,
        UPARAM(DisplayName="结束索引") int32 EndIndex,
        UPARAM(DisplayName="输出数组") TArray<AActor*>& OutArray);

    /** 根据索引范围截取浮点数数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "由索引截取浮点数数组",
            Keywords = "截取,索引,浮点数,数组",
            ToolTip = "根据索引范围截取浮点数数组。\n参数:\nInArray - 要截取的浮点数数组\nStartIndex - 起始索引（包含）\nEndIndex - 结束索引（包含）\n返回值:\nOutArray - 截取后的数组"
        ))
    static void SliceFloatArrayByIndices(
        UPARAM(DisplayName="输入数组") const TArray<float>& InArray,
        UPARAM(DisplayName="起始索引") int32 StartIndex,
        UPARAM(DisplayName="结束索引") int32 EndIndex,
        UPARAM(DisplayName="输出数组") TArray<float>& OutArray);

    /** 根据索引范围截取整数数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "由索引截取整数数组",
            Keywords = "截取,索引,整数,数组",
            ToolTip = "根据索引范围截取整数数组。\n参数:\nInArray - 要截取的整数数组\nStartIndex - 起始索引（包含）\nEndIndex - 结束索引（包含）\n返回值:\nOutArray - 截取后的数组"
        ))
    static void SliceIntegerArrayByIndices(
        UPARAM(DisplayName="输入数组") const TArray<int32>& InArray,
        UPARAM(DisplayName="起始索引") int32 StartIndex,
        UPARAM(DisplayName="结束索引") int32 EndIndex,
        UPARAM(DisplayName="输出数组") TArray<int32>& OutArray);

    /** 根据索引范围截取向量数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "由索引截取向量数组",
            Keywords = "截取,索引,向量,数组",
            ToolTip = "根据索引范围截取向量数组。\n参数:\nInArray - 要截取的向量数组\nStartIndex - 起始索引（包含）\nEndIndex - 结束索引（包含）\n返回值:\nOutArray - 截取后的数组"
        ))
    static void SliceVectorArrayByIndices(
        UPARAM(DisplayName="输入数组") const TArray<FVector>& InArray,
        UPARAM(DisplayName="起始索引") int32 StartIndex,
        UPARAM(DisplayName="结束索引") int32 EndIndex,
        UPARAM(DisplayName="输出数组") TArray<FVector>& OutArray);

    /** 根据值范围截取浮点数数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "由值截取浮点数数组",
            Keywords = "截取,值,浮点数,数组",
            ToolTip = "根据值范围截取浮点数数组。\n参数:\nInArray - 要截取的浮点数数组\nMinValue - 最小值（包含）\nMaxValue - 最大值（包含）\n返回值:\nOutArray - 截取后的数组\nIndices - 保留元素的原始索引"
        ))
    static void SliceFloatArrayByValue(
        UPARAM(DisplayName="输入数组") const TArray<float>& InArray,
        UPARAM(DisplayName="最小值") float MinValue,
        UPARAM(DisplayName="最大值") float MaxValue,
        UPARAM(DisplayName="输出数组") TArray<float>& OutArray,
        UPARAM(DisplayName="原始索引") TArray<int32>& Indices);

    /** 根据值范围截取整数数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "由值截取整数数组",
            Keywords = "截取,值,整数,数组",
            ToolTip = "根据值范围截取整数数组。\n参数:\nInArray - 要截取的整数数组\nMinValue - 最小值（包含）\nMaxValue - 最大值（包含）\n返回值:\nOutArray - 截取后的数组\nIndices - 保留元素的原始索引"
        ))
    static void SliceIntegerArrayByValue(
        UPARAM(DisplayName="输入数组") const TArray<int32>& InArray,
        UPARAM(DisplayName="最小值") int32 MinValue,
        UPARAM(DisplayName="最大值") int32 MaxValue,
        UPARAM(DisplayName="输出数组") TArray<int32>& OutArray,
        UPARAM(DisplayName="原始索引") TArray<int32>& Indices);

    /** 根据长度范围截取向量数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "由长度截取向量数组",
            Keywords = "截取,长度,向量,数组",
            ToolTip = "根据长度范围截取向量数组。\n参数:\nInArray - 要截取的向量数组\nMinLength - 最小长度（包含）\nMaxLength - 最大长度（包含）\n返回值:\nOutArray - 截取后的数组\nIndices - 保留元素的原始索引\nLengths - 保留向量的长度"
        ))
    static void SliceVectorArrayByLength(
        UPARAM(DisplayName="输入数组") const TArray<FVector>& InArray,
        UPARAM(DisplayName="最小长度") float MinLength,
        UPARAM(DisplayName="最大长度") float MaxLength,
        UPARAM(DisplayName="输出数组") TArray<FVector>& OutArray,
        UPARAM(DisplayName="原始索引") TArray<int32>& Indices,
        UPARAM(DisplayName="向量长度") TArray<float>& Lengths);

    /** 根据向量在指定坐标轴上的值截取数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|截取", 
        meta = (
            DisplayName = "根据坐标值截取向量数组",
            Keywords = "截取,向量,坐标,XYZ,范围,筛选",
            ToolTip = "根据向量在指定坐标轴上的值范围截取数组。\n参数:\nInArray - 要截取的向量数组\nAxis - 要检查的坐标轴\nMinValue - 最小坐标值（包含）\nMaxValue - 最大坐标值（包含）\n返回值:\nOutArray - 截取后的向量数组\nIndices - 保留元素的原始索引\nAxisValues - 保留元素在指定轴上的值"
        ))
    static void SliceVectorArrayByComponent(
        UPARAM(DisplayName="向量数组") const TArray<FVector>& InArray,
        UPARAM(DisplayName="坐标轴") ECoordinateAxis Axis,
        UPARAM(DisplayName="最小值") float MinValue,
        UPARAM(DisplayName="最大值") float MaxValue,
        UPARAM(DisplayName="截取后数组") TArray<FVector>& OutArray,
        UPARAM(DisplayName="原始索引") TArray<int32>& Indices,
        UPARAM(DisplayName="坐标值") TArray<float>& AxisValues);

    //~ 数组反转函数
    // =================================================================================================

    /** 反转浮点数数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|反转", 
        meta = (
            DisplayName = "反转浮点数数组",
            Keywords = "反转,浮点数,数组",
            ToolTip = "反转浮点数数组的元素顺序。\n参数:\nInArray - 要反转的浮点数数组\n返回值:\nOutArray - 反转后的数组"
        ))
    static void ReverseFloatArray(
        UPARAM(DisplayName="输入数组") const TArray<float>& InArray,
        UPARAM(DisplayName="输出数组") TArray<float>& OutArray);

    /** 反转整数数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|反转", 
        meta = (
            DisplayName = "反转整数数组",
            Keywords = "反转,整数,数组",
            ToolTip = "反转整数数组的元素顺序。\n参数:\nInArray - 要反转的整数数组\n返回值:\nOutArray - 反转后的数组"
        ))
    static void ReverseIntegerArray(
        UPARAM(DisplayName="输入数组") const TArray<int32>& InArray,
        UPARAM(DisplayName="输出数组") TArray<int32>& OutArray);

    /** 反转向量数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|反转", 
        meta = (
            DisplayName = "反转向量数组",
            Keywords = "反转,向量,数组",
            ToolTip = "反转向量数组的元素顺序。\n参数:\nInArray - 要反转的向量数组\n返回值:\nOutArray - 反转后的数组"
        ))
    static void ReverseVectorArray(
        UPARAM(DisplayName="输入数组") const TArray<FVector>& InArray,
        UPARAM(DisplayName="输出数组") TArray<FVector>& OutArray);

    /** 反转Actor数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|反转", 
        meta = (
            DisplayName = "反转Actor数组",
            Keywords = "反转,Actor,数组",
            ToolTip = "反转Actor数组的元素顺序。\n参数:\nInArray - 要反转的Actor数组\n返回值:\nOutArray - 反转后的数组"
        ))
    static void ReverseActorArray(
        UPARAM(DisplayName="输入数组") const TArray<AActor*>& InArray,
        UPARAM(DisplayName="输出数组") TArray<AActor*>& OutArray);

    /** 反转字符串数组 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|反转", 
        meta = (
            DisplayName = "反转字符串数组",
            Keywords = "反转,字符串,数组",
            ToolTip = "反转字符串数组的元素顺序。\n参数:\nInArray - 要反转的字符串数组\n返回值:\nOutArray - 反转后的数组"
        ))
    static void ReverseStringArray(
        UPARAM(DisplayName="输入数组") const TArray<FString>& InArray,
        UPARAM(DisplayName="输出数组") TArray<FString>& OutArray);

    //~ 数组去重函数
    // =================================================================================================

    /** 清除Actor数组中的重复项 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|去重", 
        meta = (
            DisplayName = "清除Actor数组重复项",
            Keywords = "去重,重复,Actor,数组",
            ToolTip = "清除Actor数组中的重复项（无效和空指针也会被移除）。\n参数:\nInArray - 要处理的Actor数组\n返回值:\nOutArray - 去重后的数组"
        ))
    static void RemoveDuplicateActors(
        UPARAM(DisplayName="输入数组") const TArray<AActor*>& InArray,
        UPARAM(DisplayName="输出数组") TArray<AActor*>& OutArray);

    /** 清除浮点数数组中的重复项 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|去重", 
        meta = (
            DisplayName = "清除浮点数数组重复项",
            Keywords = "去重,重复,浮点数,数组",
            ToolTip = "清除浮点数数组中的重复项。\n参数:\nInArray - 要处理的浮点数数组\nTolerance - 判断相等的容差值（默认为KindaSmallNumber）\n返回值:\nOutArray - 去重后的数组"
        ))
    static void RemoveDuplicateFloats(
        UPARAM(DisplayName="输入数组") const TArray<float>& InArray,
        UPARAM(DisplayName="容差值") float Tolerance,
        UPARAM(DisplayName="输出数组") TArray<float>& OutArray);

    /** 清除整数数组中的重复项 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|去重", 
        meta = (
            DisplayName = "清除整数数组重复项",
            Keywords = "去重,重复,整数,数组",
            ToolTip = "清除整数数组中的重复项。\n参数:\nInArray - 要处理的整数数组\n返回值:\nOutArray - 去重后的数组"
        ))
    static void RemoveDuplicateIntegers(
        UPARAM(DisplayName="输入数组") const TArray<int32>& InArray,
        UPARAM(DisplayName="输出数组") TArray<int32>& OutArray);

    /** 清除字符串数组中的重复项 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|去重", 
        meta = (
            DisplayName = "清除字符串数组重复项",
            Keywords = "去重,重复,字符串,数组",
            ToolTip = "清除字符串数组中的重复项。\n参数:\nInArray - 要处理的字符串数组\nbCaseSensitive - 是否区分大小写\n返回值:\nOutArray - 去重后的数组"
        ))
    static void RemoveDuplicateStrings(
        UPARAM(DisplayName="输入数组") const TArray<FString>& InArray,
        UPARAM(DisplayName="区分大小写") bool bCaseSensitive,
        UPARAM(DisplayName="输出数组") TArray<FString>& OutArray);

    /** 清除向量数组中的重复项 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|去重", 
        meta = (
            DisplayName = "清除向量数组重复项",
            Keywords = "去重,重复,向量,数组",
            ToolTip = "清除向量数组中的重复项。\n参数:\nInArray - 要处理的向量数组\nTolerance - 判断相等的容差值（默认为KindaSmallNumber）\n返回值:\nOutArray - 去重后的数组"
        ))
    static void RemoveDuplicateVectors(
        UPARAM(DisplayName="输入数组") const TArray<FVector>& InArray,
        UPARAM(DisplayName="容差值") float Tolerance,
        UPARAM(DisplayName="输出数组") TArray<FVector>& OutArray);

    /** 查找向量数组中的重复向量 */
    UFUNCTION(BlueprintPure,
        Category = "XTools|数组操作|去重", 
        meta = (
            DisplayName = "查找重复向量",
            Keywords = "查找,重复,向量,数组,索引",
            ToolTip = "查找向量数组中完全相同的向量。\n参数:\nInArray - 要处理的向量数组\nTolerance - 判断相等的容差值（默认为KindaSmallNumber）\n返回值:\nDuplicateIndices - 重复向量的索引\nDuplicateValues - 对应的向量值"
        ))
    static void FindDuplicateVectors(
        UPARAM(DisplayName="输入数组") const TArray<FVector>& InArray,
        UPARAM(DisplayName="容差值") float Tolerance,
        UPARAM(DisplayName="重复项索引") TArray<int32>& DuplicateIndices,
        UPARAM(DisplayName="重复项值") TArray<FVector>& DuplicateValues);

    //~ 通用属性排序函数 (基于GitHub项目的改进)
    // =================================================================================================

    /**
     * 通用数组按属性排序 - 支持任意结构体和对象类型
     * 原地排序：直接修改输入数组，不创建新数组
     */
    UFUNCTION(BlueprintCallable, CustomThunk,
        Category = "XTools|排序|通用",
        meta = (
            ArrayParm = "TargetArray",
            DisplayName = "通用属性排序（原地）",
            Keywords = "排序,属性,结构体,对象,通用,原地",
            ToolTip = "对任意类型的数组按指定属性进行原地排序。\n⚠️ 注意：这会直接修改输入数组的顺序！\n\n支持类型：\n• 结构体数组 - 按结构体的任意属性排序\n• 对象数组 - 按对象的任意属性排序\n\n参数:\n• TargetArray - 要排序的数组（会被直接修改）\n• PropertyName - 要排序的属性名称\n• bAscending - 是否升序排序\n\n输出:\n• OriginalIndices - 排序后每个元素在原数组中的原始索引"
        ))
    static void SortArrayByPropertyInPlace(
        UPARAM(ref, DisplayName="目标数组") TArray<int32>& TargetArray,
        UPARAM(DisplayName="属性名称") FName PropertyName,
        UPARAM(DisplayName="升序排序") bool bAscending,
        UPARAM(DisplayName="原始索引") TArray<int32>& OriginalIndices);

    // 通用属性排序的实现函数
    static void GenericSortArrayByProperty(void* TargetArray, FArrayProperty* ArrayProp, FName PropertyName, bool bAscending, TArray<int32>& OriginalIndices);

    // CustomThunk函数声明
    DECLARE_FUNCTION(execSortArrayByPropertyInPlace);

private:
    // 属性查找辅助函数
    template<typename T>
    static T* FindPropertyByName(const UStruct* Owner, FName FieldName);

    // 属性值比较函数
    static bool ComparePropertyValues(const FProperty* Property, const void* LeftValuePtr, const void* RightValuePtr, bool bAscending);

    // 快速排序实现
    static void QuickSortByProperty(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, TArray<int32>& Indices, int32 Low, int32 High, bool bAscending);
    static int32 PartitionByProperty(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, TArray<int32>& Indices, int32 Low, int32 High, bool bAscending);

    // 属性值访问辅助函数
    static void* GetPropertyValuePtr(FScriptArrayHelper& ArrayHelper, FProperty* InnerProp, FProperty* SortProp, int32 ElementIndex);

private:
    // 属性类型枚举
    enum class EPropertyType : uint8
    {
        Float,
        Integer,
        String,
        Boolean
    };

    // 结构体排序核心实现
    static void SortStructByPropertyImpl(void* TargetArray, FArrayProperty* ArrayProp, FName PropertyName, bool bAscending, TArray<int32>& OriginalIndices, EPropertyType PropertyType);

    // 结构体属性值比较
    static bool CompareStructPropertyValues(const FProperty* Property, const void* LeftValuePtr, const void* RightValuePtr, bool bAscending, EPropertyType PropertyType);

    // 结构体快速排序
    static void QuickSortStructByProperty(FScriptArrayHelper& ArrayHelper, FProperty* StructProp, FProperty* SortProp, TArray<int32>& Indices, int32 Low, int32 High, bool bAscending, EPropertyType PropertyType);
    static int32 PartitionStructByProperty(FScriptArrayHelper& ArrayHelper, FProperty* StructProp, FProperty* SortProp, TArray<int32>& Indices, int32 Low, int32 High, bool bAscending, EPropertyType PropertyType);
};
