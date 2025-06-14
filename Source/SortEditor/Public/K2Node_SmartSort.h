#pragma once

#include "CoreMinimal.h"

// ----------------------------------------------------------------------------
// 公共枚举：运行时与编辑器均可见，便于在蓝图/代码侧直接引用
// ----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ESmartSort_BaseSortMode : uint8
{
	Integer     UMETA(DisplayName = "整数"),
	Float       UMETA(DisplayName = "浮点"),
	String      UMETA(DisplayName = "字符串"),
	Name        UMETA(DisplayName = "名称")
};

UENUM(BlueprintType)
enum class ESmartSort_ActorSortMode : uint8
{
	ByDistance  UMETA(DisplayName = "按距离"),
	ByHeight    UMETA(DisplayName = "按高度"),
	ByAxis      UMETA(DisplayName = "按坐标轴"),
	ByAngle     UMETA(DisplayName = "按夹角"),
	ByAzimuth   UMETA(DisplayName = "按方位角")
};

UENUM(BlueprintType)
enum class ESmartSort_VectorSortMode : uint8
{
	ByLength     UMETA(DisplayName = "按长度"),
	ByProjection UMETA(DisplayName = "按投影"),
	ByAxis       UMETA(DisplayName = "按坐标轴")
};

#if WITH_EDITOR
#include "K2Node.h"
#include "SortLibrary.h" // 包含 ECoordinateAxis
#include "K2Node_SmartSort.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraphPin;
class UK2Node_CallFunction;
class FKismetCompilerContext;

/**
 * @brief 智能排序节点的引脚名称常量
 * 使用constexpr提高编译时性能
 */
struct FSmartSort_Helper
{
	static const FName PN_TargetArray;
	static const FName PN_SortedArray;
	static const FName PN_OriginalIndices;
	static const FName PN_Ascending;
	static const FName PN_SortMode;
	static const FName PN_Location;
	static const FName PN_Direction;
	static const FName PN_Axis;
	static const FName PN_PropertyName;
	// 移除PN_Fail - 不再需要失败执行引脚

	// 编译时常量，提高性能
	static constexpr int32 MaxDynamicPins = 3;
};

/**
 * @brief 一个智能排序节点，可根据连接的数组类型动态改变其排序选项。
 */
UCLASS()
class SORTEDITOR_API UK2Node_SmartSort : public UK2Node
{
	GENERATED_BODY()

public:
	UK2Node_SmartSort(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	//~ UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetMenuCategory() const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PostReconstructNode() override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UEdGraphNode interface

	//~ UK2Node interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	//~ End UK2Node interface

	/** 获取输入数组引脚 */
	FORCEINLINE UEdGraphPin* GetArrayInputPin() const;

	/** 获取输出的已排序数组引脚 */
	FORCEINLINE UEdGraphPin* GetSortedArrayOutputPin() const;

	/** 获取排序模式引脚 */
	FORCEINLINE UEdGraphPin* GetSortModePin() const;

	// 移除GetFailPin() - 不再需要失败引脚

private:
	/**
	 * @brief 核心函数：根据输入和输出引脚的连接情况，解析出数组引脚应该是什么类型。
	 * @return 返回解析出的引脚类型，如果没有任何连接，则返回通配符类型。
	 */
	FEdGraphPinType GetResolvedArrayType() const;

	/**
	 * @brief 根据传入的类型，获取对应的排序方式枚举。
	 * @param PinType 已解析的数组元素类型。
	 * @return 返回一个UEnum指针，如果该类型没有特定的排序模式，则返回nullptr。
	 */
	UEnum* GetSortModeEnumForType(const FEdGraphPinType& PinType) const;
	
	/**
	 * @brief 根据当前的数组类型，重新构建节点的动态输入引脚 (如 Location, Axis 等)。
	 */
	void RebuildDynamicPins();

	/**
	 * @brief 将确定的类型应用到本节点的通配符引脚上。
	 * @param TypeToPropagate 要应用的类型。
	 */
	void PropagatePinType(const FEdGraphPinType& TypeToPropagate);
	void PropagateTypeToFunctionNode(UK2Node_CallFunction* FunctionNode, const FEdGraphPinType& ArrayType);

	// ExpandNode辅助函数
	bool DetermineSortFunction(const FEdGraphPinType& ConnectedType, FName& OutFunctionName);
	bool DetermineActorSortFunction(FName& OutFunctionName);
	bool DetermineVectorSortFunction(FName& OutFunctionName);
	bool DetermineBasicTypeSortFunction(const FEdGraphPinType& ConnectedType, FName& OutFunctionName);
	bool ConnectArrayInputPin(FKismetCompilerContext& CompilerContext, UEdGraphPin* ArrayInputPin, UK2Node_CallFunction* CallFunctionNode);
	void ConnectOutputPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode);
	void ConnectDynamicInputPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode);

	// Pure函数特殊处理
	void CreatePureFunctionExecutionFlow(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode, UEdGraph* SourceGraph);

	// 枚举引脚辅助函数
	void SetEnumPinDefaultValue(UEdGraphPin* EnumPin, UEnum* EnumClass);

	// 类型名称映射辅助函数
	FString GetTypeDisplayName(const FEdGraphPinType& PinType) const;

public:
	// 通用属性排序支持
	TArray<FString> GetAvailableProperties(const FEdGraphPinType& ArrayType) const;
	bool IsGenericPropertySortSupported(const FEdGraphPinType& ArrayType) const;
	bool IsPropertySortable(const FProperty* Property) const;

	// 获取属性选择选项（用于下拉框）
	TArray<TSharedPtr<FString>> GetPropertyOptions() const;

private:

	// 结构体排序支持
	FProperty* FindPropertyByName(UScriptStruct* StructType, FName PropertyName);
	bool IsVectorType(const FEdGraphPinType& PinType) const;

#if WITH_EDITORONLY_DATA
	/** 存储当前节点使用的排序方式枚举 - 仅用于UI显示和引脚重建 */
	UPROPERTY()
	TObjectPtr<UEnum> CurrentSortEnum;

	/** 防止递归调用的标志 */
	bool bIsReconstructingPins;
#endif

};

#endif
