#include "K2Node_SmartSort.h"

#if WITH_EDITOR

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_AssignmentStatement.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SortLibrary.h"
#include "XToolsErrorReporter.h"

#define LOCTEXT_NAMESPACE "K2Node_SmartSort"

// 定义引脚名称常量
const FName FSmartSort_Helper::PN_TargetArray(TEXT("TargetArray"));
const FName FSmartSort_Helper::PN_SortedArray(TEXT("SortedArray"));
const FName FSmartSort_Helper::PN_OriginalIndices(TEXT("OriginalIndices"));
const FName FSmartSort_Helper::PN_Ascending(TEXT("bAscending"));
const FName FSmartSort_Helper::PN_SortMode(TEXT("SortMode"));
const FName FSmartSort_Helper::PN_Location(TEXT("Location"));
const FName FSmartSort_Helper::PN_Direction(TEXT("Direction"));
const FName FSmartSort_Helper::PN_Axis(TEXT("Axis"));
const FName FSmartSort_Helper::PN_PropertyName(TEXT("PropertyName"));

UK2Node_SmartSort::UK2Node_SmartSort(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, bIsReconstructingPins(false)
#endif
{
	// 编译时验证枚举完整性
	static_assert(static_cast<int32>(EActorSortMode::ByAzimuth) == 4, "Actor排序模式枚举不完整");
	static_assert(static_cast<int32>(EVectorSortMode::ByAxis) == 2, "Vector排序模式枚举不完整");
}

void UK2Node_SmartSort::AllocateDefaultPins()
{
	// 创建执行引脚
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	// 移除Fail引脚 - 所有排序函数都是Pure函数，不会运行时失败

	// 创建数组输入引脚（通配符）
	UEdGraphPin* ArrayPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, FSmartSort_Helper::PN_TargetArray);
	ArrayPin->PinType.ContainerType = EPinContainerType::Array;
	ArrayPin->PinToolTip = LOCTEXT("ArrayPin_Tooltip", "要排序的数组").ToString();

	// 创建数组输出引脚（通配符）
	UEdGraphPin* SortedArrayPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, FSmartSort_Helper::PN_SortedArray);
	SortedArrayPin->PinType.ContainerType = EPinContainerType::Array;
	SortedArrayPin->PinToolTip = LOCTEXT("SortedArrayPin_Tooltip", "排序后的数组").ToString();

	// 创建原始索引输出引脚
	UEdGraphPin* IndicesPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, FSmartSort_Helper::PN_OriginalIndices);
	IndicesPin->PinType.ContainerType = EPinContainerType::Array;
	IndicesPin->PinToolTip = LOCTEXT("IndicesPin_Tooltip", "原始索引数组").ToString();

	// 创建升序/降序引脚
	UEdGraphPin* AscendingPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Boolean, FSmartSort_Helper::PN_Ascending);
	AscendingPin->DefaultValue = TEXT("true");
	AscendingPin->PinToolTip = LOCTEXT("AscendingPin_Tooltip", "是否升序排列").ToString();

	// 创建排序模式引脚（隐藏，动态显示）
	UEdGraphPin* ModePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, nullptr, FSmartSort_Helper::PN_SortMode);
	ModePin->bHidden = true;
	ModePin->PinToolTip = LOCTEXT("ModePin_Tooltip", "排序模式").ToString();
	// 注意：排序模式的默认值将在RebuildDynamicPins中根据数组类型设置

	Super::AllocateDefaultPins();
}

void UK2Node_SmartSort::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();

	// 先恢复连接，然后再重建动态引脚
	RestoreSplitPins(OldPins);

	// 重建动态引脚（在连接恢复后）
	RebuildDynamicPins();
}

void UK2Node_SmartSort::PostReconstructNode()
{
	Super::PostReconstructNode();

	// 确保引脚类型正确传播
	UEdGraphPin* ArrayInputPin = GetArrayInputPin();
	if (ArrayInputPin && ArrayInputPin->LinkedTo.Num() > 0)
	{
		PropagatePinType(ArrayInputPin->LinkedTo[0]->PinType);
	}

	// 确保动态引脚正确显示
	RebuildDynamicPins();
}

void UK2Node_SmartSort::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

#if WITH_EDITORONLY_DATA
	// 防止递归调用
	if (bIsReconstructingPins)
	{
		return;
	}
#endif

	// 处理数组引脚连接变化
	if (Pin && Pin->PinName == FSmartSort_Helper::PN_TargetArray)
	{
#if WITH_EDITORONLY_DATA
		bIsReconstructingPins = true;
#endif

		// 传播类型
		if (Pin->LinkedTo.Num() > 0)
		{
			PropagatePinType(Pin->LinkedTo[0]->PinType);
		}
		else
		{
			// 如果断开连接，重置为通配符
			FEdGraphPinType WildcardType;
			WildcardType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			WildcardType.ContainerType = EPinContainerType::Array;
			PropagatePinType(WildcardType);
		}

		// 当数组引脚连接改变时，重建动态引脚
		RebuildDynamicPins();

		// 重建图形
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForNode(this));

		// 触发节点视觉更新（包括标题）
		if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}

#if WITH_EDITORONLY_DATA
		bIsReconstructingPins = false;
#endif
	}
	// 处理排序模式引脚连接变化（提升为变量或断开连接）
	else if (Pin && Pin->PinName == FSmartSort_Helper::PN_SortMode)
	{
#if WITH_EDITORONLY_DATA
		bIsReconstructingPins = true;
#endif

		// 当排序模式引脚连接状态改变时，重建动态引脚
		// 如果连接了（提升为变量），显示所有可能的引脚
		// 如果断开连接，根据默认值显示需要的引脚
		RebuildDynamicPins();

		// 重建图形
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForNode(this));

		UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 排序模式引脚连接状态改变，已重建动态引脚"));

#if WITH_EDITORONLY_DATA
		bIsReconstructingPins = false;
#endif
	}
}

void UK2Node_SmartSort::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	// 注意：不要在这里调用PinConnectionListChanged，因为Super::NotifyPinConnectionListChanged已经会调用它
	// 这样可以避免重复调用导致的递归问题
}

void UK2Node_SmartSort::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin && Pin->PinName == FSmartSort_Helper::PN_SortMode)
	{
		// 当排序模式改变时，重建动态引脚
		RebuildDynamicPins();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForNode(this));
	}
}

FEdGraphPinType UK2Node_SmartSort::GetResolvedArrayType() const
{
	if (const UEdGraphPin* ArrayPin = GetArrayInputPin())
	{
		if (ArrayPin->LinkedTo.Num() > 0 && ArrayPin->LinkedTo[0])
		{
			return ArrayPin->LinkedTo[0]->PinType;
		}
	}

	// 返回通配符类型
	FEdGraphPinType WildcardType;
	WildcardType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	WildcardType.ContainerType = EPinContainerType::Array;
	return WildcardType;
}

UEnum* UK2Node_SmartSort::GetSortModeEnumForType(const FEdGraphPinType& ArrayType) const
{
	if (ArrayType.PinCategory == UEdGraphSchema_K2::PC_Object &&
		ArrayType.PinSubCategoryObject.IsValid())
	{
		if (UClass* Class = Cast<UClass>(ArrayType.PinSubCategoryObject.Get()))
		{
			if (Class->IsChildOf(AActor::StaticClass()))
			{
				return StaticEnum<EActorSortMode>();
			}
		}
	}
	else if (ArrayType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
			 ArrayType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
	{
		return StaticEnum<EVectorSortMode>();
	}

	return nullptr; // 基础类型不需要枚举
}

void UK2Node_SmartSort::RebuildDynamicPins()
{
	// 移除所有动态引脚（保留基础引脚）
	// 优化：使用反向迭代避免索引问题，并使用常量引用
	static const TArray<FName> DynamicPinNames = {
		FSmartSort_Helper::PN_Location,
		FSmartSort_Helper::PN_Direction,
		FSmartSort_Helper::PN_Axis,
		FSmartSort_Helper::PN_PropertyName
	};

	for (int32 i = Pins.Num() - 1; i >= 0; --i)
	{
		if (UEdGraphPin* Pin = Pins[i])
		{
			if (DynamicPinNames.Contains(Pin->PinName))
			{
				RemovePin(Pin);
			}
		}
	}

	// 获取当前连接的数组类型
	FEdGraphPinType ConnectedType = this->GetResolvedArrayType();
	if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		// 如果是通配符，隐藏模式引脚
		UEdGraphPin* ModePin = this->GetSortModePin();
		if (ModePin)
		{
			ModePin->bHidden = true;
		}
		return;
	}

	// 根据类型确定是否需要显示排序模式选择
	UEnum* SortModeEnum = this->GetSortModeEnumForType(ConnectedType);
	UEdGraphPin* ModePin = this->GetSortModePin();

	if (SortModeEnum && ModePin)
	{
		// 显示模式选择引脚
		ModePin->PinType.PinSubCategoryObject = SortModeEnum;
		ModePin->bHidden = false;

		// 检查排序模式引脚是否被连接（提升为变量）
		const bool bModePinIsConnected = (ModePin->LinkedTo.Num() > 0);

		if (bModePinIsConnected)
		{
			// 排序模式引脚连接到变量：显示所有可能的引脚，运行时根据变量值选择
			// 使用 Advanced 视图折叠不常用的引脚，保持 UI 简洁
			UE_LOG(LogBlueprint, Log, TEXT("[智能排序] 排序模式连接到变量，显示所有可能的引脚（部分折叠到高级选项）"));

			if (SortModeEnum == StaticEnum<EActorSortMode>())
			{
				// Actor 排序：创建所有可能需要的引脚
				UEdGraphPin* LocationPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), FSmartSort_Helper::PN_Location);
				LocationPin->PinToolTip = LOCTEXT("LocationPin_Tooltip", "参考位置或中心点（用于距离/角度/方位角排序）").ToString();
				LocationPin->bAdvancedView = false; // 常用，始终显示

				UEdGraphPin* DirectionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), FSmartSort_Helper::PN_Direction);
				DirectionPin->PinToolTip = LOCTEXT("DirectionPin_Tooltip", "参考方向（用于角度排序）").ToString();
				DirectionPin->bAdvancedView = true; // 不常用，折叠到高级选项

				UEdGraphPin* AxisPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<ECoordinateAxis>(), FSmartSort_Helper::PN_Axis);
				AxisPin->PinToolTip = LOCTEXT("AxisPin_Tooltip", "排序使用的坐标轴（用于坐标轴排序）").ToString();
				SetEnumPinDefaultValue(AxisPin, StaticEnum<ECoordinateAxis>());
				AxisPin->bAdvancedView = true; // 不常用，折叠到高级选项
			}
			else if (SortModeEnum == StaticEnum<EVectorSortMode>())
			{
				// Vector 排序：创建所有可能需要的引脚
				UEdGraphPin* DirectionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), FSmartSort_Helper::PN_Direction);
				DirectionPin->PinToolTip = LOCTEXT("DirectionPin_Tooltip", "投影方向（用于投影排序）").ToString();
				DirectionPin->bAdvancedView = true; // 不常用，折叠到高级选项

				UEdGraphPin* AxisPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<ECoordinateAxis>(), FSmartSort_Helper::PN_Axis);
				AxisPin->PinToolTip = LOCTEXT("AxisPin_Tooltip", "排序使用的坐标轴（用于坐标轴排序）").ToString();
				SetEnumPinDefaultValue(AxisPin, StaticEnum<ECoordinateAxis>());
				AxisPin->bAdvancedView = true; // 不常用，折叠到高级选项
			}
		}
		else
		{
			// 排序模式引脚未连接：根据默认值只显示需要的引脚（原有逻辑）
			UE_LOG(LogBlueprint, Log, TEXT("[智能排序] 排序模式使用直接输入，仅显示当前模式需要的引脚"));

			// 检查当前默认值是否属于新的枚举类型
			bool bNeedResetDefault = true;
			const FString CurrentDefault = ModePin->GetDefaultAsString();
			if (!CurrentDefault.IsEmpty())
			{
				int64 IntValue = SortModeEnum->GetValueByNameString(CurrentDefault);
				if (IntValue != INDEX_NONE)
				{
					bNeedResetDefault = false; // 当前默认值有效，不需要重置
				}
			}

			// 获取当前选择的排序模式
			uint8 EnumValue = 0;

			if (bNeedResetDefault)
			{
				// 需要重置默认值，设置为枚举的第一项
				if (SortModeEnum->NumEnums() > 1)
				{
					FString FirstEnumName = SortModeEnum->GetNameStringByIndex(0);
					ModePin->DefaultValue = FirstEnumName;
					EnumValue = 0;
					UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 重置排序模式默认值: %s"), *FirstEnumName);
				}
			}
			else
			{
				// 使用现有的有效默认值
				const FString DefaultStr = ModePin->GetDefaultAsString();
				int64 IntValue = SortModeEnum->GetValueByNameString(DefaultStr);
				EnumValue = static_cast<uint8>(IntValue);
				UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 保持现有排序模式: %s"), *DefaultStr);
			}

			// 根据选择的排序模式，创建额外的输入引脚
			if (SortModeEnum == StaticEnum<EActorSortMode>())
			{
				switch (static_cast<EActorSortMode>(EnumValue))
				{
				case EActorSortMode::ByDistance:
				case EActorSortMode::ByAngle:
				case EActorSortMode::ByAzimuth:
					{
						UEdGraphPin* LocationPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), FSmartSort_Helper::PN_Location);
						LocationPin->PinToolTip = LOCTEXT("LocationPin_Tooltip", "参考位置或中心点").ToString();

						if (static_cast<EActorSortMode>(EnumValue) == EActorSortMode::ByAngle)
						{
							UEdGraphPin* DirectionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), FSmartSort_Helper::PN_Direction);
							DirectionPin->PinToolTip = LOCTEXT("DirectionPin_Tooltip", "参考方向").ToString();
						}
					}
					break;
				case EActorSortMode::ByAxis:
					{
						UEdGraphPin* AxisPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<ECoordinateAxis>(), FSmartSort_Helper::PN_Axis);
						AxisPin->PinToolTip = LOCTEXT("AxisPin_Tooltip", "排序使用的坐标轴").ToString();
						// 设置默认值为第一个枚举项
						SetEnumPinDefaultValue(AxisPin, StaticEnum<ECoordinateAxis>());
					}
					break;
				default: break;
				}
			}
			else if (SortModeEnum == StaticEnum<EVectorSortMode>())
			{
				switch (static_cast<EVectorSortMode>(EnumValue))
				{
				case EVectorSortMode::ByProjection:
					{
						UEdGraphPin* DirectionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get(), FSmartSort_Helper::PN_Direction);
						DirectionPin->PinToolTip = LOCTEXT("DirectionPin_Tooltip", "投影方向").ToString();
					}
					break;
				case EVectorSortMode::ByAxis:
					{
						UEdGraphPin* AxisPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<ECoordinateAxis>(), FSmartSort_Helper::PN_Axis);
						AxisPin->PinToolTip = LOCTEXT("AxisPin_Tooltip", "排序使用的坐标轴").ToString();
						// 设置默认值为第一个枚举项
						SetEnumPinDefaultValue(AxisPin, StaticEnum<ECoordinateAxis>());
					}
					break;
				default: break;
				}
			}
		}
	}
	else if (ModePin)
	{
		// 基础类型不需要模式选择，隐藏引脚
		ModePin->bHidden = true;
	}

	// 检查是否为结构体类型，如果是则创建属性选择引脚
	if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
		ConnectedType.PinSubCategoryObject.IsValid())
	{
		UScriptStruct* StructType = Cast<UScriptStruct>(ConnectedType.PinSubCategoryObject.Get());
		if (StructType && !IsVectorType(ConnectedType)) // 排除已处理的FVector
		{
			// 创建属性名称选择引脚
			UEdGraphPin* PropertyNamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, FSmartSort_Helper::PN_PropertyName);
			PropertyNamePin->PinToolTip = LOCTEXT("PropertyNamePin_Tooltip", "选择要排序的结构体属性").ToString();

			// 获取可排序的属性列表
			TArray<FString> AvailableProperties = GetAvailableProperties(ConnectedType);
			if (AvailableProperties.Num() > 0)
			{
				// 如果当前没有默认值或默认值无效，设置第一个可用属性为默认值
				FString CurrentDefault = PropertyNamePin->GetDefaultAsString();
				if (CurrentDefault.IsEmpty() || !AvailableProperties.Contains(CurrentDefault))
				{
					PropertyNamePin->DefaultValue = AvailableProperties[0];
					UE_LOG(LogBlueprint, Warning, TEXT("[智能排序] 设置结构体属性默认值: %s"), *AvailableProperties[0]);
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Warning, TEXT("[智能排序] 结构体 %s 没有可排序的属性"), *StructType->GetName());
			}
		}
	}
}

void UK2Node_SmartSort::PropagatePinType(const FEdGraphPinType& TypeToPropagate)
{
	UEdGraphPin* ArrayInputPin = GetArrayInputPin();
	UEdGraphPin* SortedArrayOutputPin = GetSortedArrayOutputPin();

	if (ArrayInputPin && SortedArrayOutputPin)
	{
		ArrayInputPin->PinType = TypeToPropagate;
		SortedArrayOutputPin->PinType = TypeToPropagate;

		// 确保引脚不是隐藏状态
		ArrayInputPin->bHidden = false;
		SortedArrayOutputPin->bHidden = false;

		// 标记引脚需要重新绘制
		ArrayInputPin->bWasTrashed = false;
		SortedArrayOutputPin->bWasTrashed = false;
	}
}

void UK2Node_SmartSort::PropagateTypeToFunctionNode(UK2Node_CallFunction* FunctionNode, const FEdGraphPinType& ArrayType)
{
	if (!FunctionNode)
	{
		FXToolsErrorReporter::Error(
			LogBlueprint,
			TEXT("[智能排序调试] PropagateTypeToFunctionNode: FunctionNode为空"),
			TEXT("K2Node_SmartSort::PropagateTypeToFunctionNode"));
		return;
	}

	// 查找函数节点的数组引脚并设置类型
	UEdGraphPin* FuncArrayPin = FunctionNode->FindPin(TEXT("TargetArray"), EGPD_Input);
	if (FuncArrayPin)
	{
		FuncArrayPin->PinType = ArrayType;
	}

	// 查找并设置输出引脚类型（如果存在）
	UEdGraphPin* FuncOutputPin = FunctionNode->FindPin(TEXT("ReturnValue"), EGPD_Output);
	if (FuncOutputPin)
	{
		FuncOutputPin->PinType = ArrayType;
	}

	// 重新构建函数节点以应用类型更改
	FunctionNode->ReconstructNode();
}

void UK2Node_SmartSort::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	// 1. 获取输入数组引脚并检查连接
	UEdGraphPin* ArrayInputPin = GetArrayInputPin();
	if (!ArrayInputPin)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_NoArrayPin", "[智能排序] 节点 %% 缺少数组输入引脚。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	if (ArrayInputPin->LinkedTo.Num() == 0)
	{
		CompilerContext.MessageLog.Warning(*LOCTEXT("SmartSort_NoArrayConnected", "[智能排序] 节点 %% 未连接任何数组。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 获取连接的数组类型
	FEdGraphPinType ConnectedType = ArrayInputPin->LinkedTo[0]->PinType;

	if (ConnectedType.ContainerType != EPinContainerType::Array)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_NotAnArray", "[智能排序] 连接的引脚不是数组类型。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 检查是否需要使用统一入口函数（Vector 或 Actor 类型）
	UEnum* SortEnum = GetSortModeEnumForType(ConnectedType);

	if (SortEnum == StaticEnum<EVectorSortMode>())
	{
		// Vector 类型：使用统一入口函数
		ExpandNodeWithUnifiedFunction(CompilerContext, SourceGraph, ConnectedType, 
			GET_FUNCTION_NAME_CHECKED(USortLibrary, SortVectorsUnified));
	}
	else if (SortEnum == StaticEnum<EActorSortMode>())
	{
		// Actor 类型：使用统一入口函数
		ExpandNodeWithUnifiedFunction(CompilerContext, SourceGraph, ConnectedType,
			GET_FUNCTION_NAME_CHECKED(USortLibrary, SortActorsUnified));
	}
	else
	{
		// 基础类型或结构体：使用静态函数路径
		ExpandNodeWithStaticFunction(CompilerContext, SourceGraph, ConnectedType);
	}
}

void UK2Node_SmartSort::ExpandNodeWithStaticFunction(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const FEdGraphPinType& ConnectedType)
{
	UEdGraphPin* ArrayInputPin = GetArrayInputPin();

	// 根据类型确定要调用的库函数名
	FName FunctionName;
	if (!DetermineSortFunction(ConnectedType, FunctionName))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_NoMatchingFunction", "找不到与选项匹配的排序函数 for node %%.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 创建函数调用节点
	UK2Node_CallFunction* CallFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFunctionNode->FunctionReference.SetExternalMember(FunctionName, USortLibrary::StaticClass());
	CallFunctionNode->AllocateDefaultPins();

	// 传播类型信息到函数调用节点
	PropagateTypeToFunctionNode(CallFunctionNode, ConnectedType);

	// 检查函数是否为Pure函数并进行相应处理
	UFunction* Function = CallFunctionNode->GetTargetFunction();
	bool bIsPureFunction = Function && Function->HasAnyFunctionFlags(FUNC_BlueprintPure);

	if (bIsPureFunction)
	{
		CreatePureFunctionExecutionFlow(CompilerContext, CallFunctionNode, SourceGraph);
	}
	else
	{
		// 连接执行引脚（非Pure函数）
		UEdGraphPin* MyExecPin = GetExecPin();
		UEdGraphPin* FuncExecPin = CallFunctionNode->GetExecPin();

		if (MyExecPin && FuncExecPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*MyExecPin, *FuncExecPin);
		}
	}

	// 连接数组输入引脚
	if (!ConnectArrayInputPin(CompilerContext, ArrayInputPin, CallFunctionNode))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_ArrayConnectionFailed", "无法连接数组输入引脚。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 连接后再次确保类型传播正确
	UEdGraphPin* FuncArrayPin = CallFunctionNode->FindPin(TEXT("TargetArray"), EGPD_Input);
	if (FuncArrayPin)
	{
		FuncArrayPin->PinType = ConnectedType;
	}

	// 连接输出引脚
	ConnectOutputPins(CompilerContext, CallFunctionNode);

	// 连接动态输入引脚
	ConnectDynamicInputPins(CompilerContext, CallFunctionNode);

	// 断开所有原始连接
	BreakAllNodeLinks();
}

void UK2Node_SmartSort::ExpandNodeWithUnifiedFunction(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const FEdGraphPinType& ConnectedType, FName UnifiedFunctionName)
{
	UEdGraphPin* ArrayInputPin = GetArrayInputPin();

	// 创建统一入口函数调用节点
	UK2Node_CallFunction* CallFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFunctionNode->FunctionReference.SetExternalMember(UnifiedFunctionName, USortLibrary::StaticClass());
	CallFunctionNode->AllocateDefaultPins();

	// 传播类型信息
	PropagateTypeToFunctionNode(CallFunctionNode, ConnectedType);

	// 连接执行引脚（统一入口函数是 Pure 函数）
	CreatePureFunctionExecutionFlow(CompilerContext, CallFunctionNode, SourceGraph);

	// 连接数组输入引脚
	if (!ConnectArrayInputPin(CompilerContext, ArrayInputPin, CallFunctionNode))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("SmartSort_ArrayConnectionFailed", "无法连接数组输入引脚。").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 连接排序模式引脚
	UEdGraphPin* ModePin = GetSortModePin();
	UEdGraphPin* FuncModePin = CallFunctionNode->FindPin(TEXT("SortMode"), EGPD_Input);
	
	if (ModePin && FuncModePin)
	{
		if (ModePin->LinkedTo.Num() > 0)
		{
			// 提升为变量：移动连接
			CompilerContext.MovePinLinksToIntermediate(*ModePin, *FuncModePin);
		}
		else
		{
			// 使用默认值
			FString DefaultStr = ModePin->GetDefaultAsString();
			if (DefaultStr.IsEmpty())
			{
				DefaultStr = ModePin->DefaultValue;
			}
			if (!DefaultStr.IsEmpty())
			{
				FuncModePin->DefaultValue = DefaultStr;
			}
		}
	}

	// 连接其他动态输入引脚
	ConnectDynamicInputPins(CompilerContext, CallFunctionNode);

	// 连接输出引脚
	ConnectOutputPins(CompilerContext, CallFunctionNode);

	// 断开所有原始连接
	BreakAllNodeLinks();
}

bool UK2Node_SmartSort::DetermineSortFunction(const FEdGraphPinType& ConnectedType, FName& OutFunctionName)
{
	UEnum* SortEnum = GetSortModeEnumForType(ConnectedType);

	// 添加调试日志（仅在开发模式下）
	UE_LOG(LogBlueprint, VeryVerbose, TEXT("[智能排序] 数组类型: %s, 子类型: %s, 容器类型: %d"),
		*ConnectedType.PinCategory.ToString(),
		ConnectedType.PinSubCategoryObject.IsValid() ? *ConnectedType.PinSubCategoryObject->GetName() : TEXT("None"),
		(int32)ConnectedType.ContainerType);

	if (SortEnum == StaticEnum<EActorSortMode>())
	{
        UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 使用Actor排序模式"));
		return DetermineActorSortFunction(OutFunctionName);
	}
	else if (SortEnum == StaticEnum<EVectorSortMode>())
	{
        UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 使用Vector排序模式"));
		return DetermineVectorSortFunction(OutFunctionName);
	}
	else if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
			 ConnectedType.PinSubCategoryObject.IsValid())
	{
		UScriptStruct* StructType = Cast<UScriptStruct>(ConnectedType.PinSubCategoryObject.Get());
		if (StructType && !IsVectorType(ConnectedType)) // 排除已处理的FVector
		{
            UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 使用结构体排序模式: %s"), *StructType->GetName());
			// 对于结构体，我们使用通用属性排序函数
			OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortArrayByPropertyInPlace);
			return true;
		}
	}
	else
	{
        UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 使用基础类型排序模式"));
		return DetermineBasicTypeSortFunction(ConnectedType, OutFunctionName);
	}

	// 如果所有条件都不满足，返回false
	FXToolsErrorReporter::Error(
		LogBlueprint,
		FString::Printf(TEXT("[智能排序] 无法确定排序函数，连接类型: %s"), *ConnectedType.PinCategory.ToString()),
		TEXT("K2Node_SmartSort::DetermineSortFunction"));
	return false;
}

bool UK2Node_SmartSort::DetermineActorSortFunction(FName& OutFunctionName)
{
	UEdGraphPin* ModePin = GetSortModePin();
	FString DefaultStr = ModePin ? ModePin->GetDefaultAsString() : TEXT("");

	// 如果默认值为空，使用第一个枚举值
	if (DefaultStr.IsEmpty())
	{
		UEnum* SortEnum = StaticEnum<EActorSortMode>();
		DefaultStr = SortEnum->GetNameStringByIndex(0);
	}

	UEnum* SortEnum = StaticEnum<EActorSortMode>();
	int64 EnumValue = SortEnum->GetValueByNameString(DefaultStr);
	if (EnumValue == INDEX_NONE)
	{
		EnumValue = 0; // 默认使用第一个值
	}

	switch (static_cast<EActorSortMode>(EnumValue))
	{
		case EActorSortMode::ByDistance: OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortActorsByDistance); return true;
		case EActorSortMode::ByHeight:   OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortActorsByHeight);   return true;
		case EActorSortMode::ByAxis:     OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortActorsByAxis);     return true;
		case EActorSortMode::ByAngle:    OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortActorsByAngle);    return true;
		case EActorSortMode::ByAzimuth:  OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortActorsByAzimuth);  return true;
	}

	return false;
}

bool UK2Node_SmartSort::DetermineVectorSortFunction(FName& OutFunctionName)
{
	UEdGraphPin* ModePin = GetSortModePin();
	FString DefaultStr = ModePin ? ModePin->GetDefaultAsString() : TEXT("");

	// 如果默认值为空，使用第一个枚举值
	if (DefaultStr.IsEmpty())
	{
		UEnum* SortEnum = StaticEnum<EVectorSortMode>();
		DefaultStr = SortEnum->GetNameStringByIndex(0);
            UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 向量排序模式为空，使用默认值: %s"), *DefaultStr);
	}

	UEnum* SortEnum = StaticEnum<EVectorSortMode>();
	int64 EnumValue = SortEnum->GetValueByNameString(DefaultStr);
	if (EnumValue == INDEX_NONE)
	{
		EnumValue = 0; // 默认使用第一个值
		UE_LOG(LogBlueprint, Warning, TEXT("[智能排序] 无法解析向量排序模式: %s，使用默认值0"), *DefaultStr);
	}

    UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 向量排序模式: %s (值: %d)"), *DefaultStr, (int32)EnumValue);

	switch (static_cast<EVectorSortMode>(EnumValue))
	{
		case EVectorSortMode::ByLength:
			OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortVectorsByLength);
            UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 选择向量长度排序函数: %s"), *OutFunctionName.ToString());
			return true;
		case EVectorSortMode::ByProjection:
			OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortVectorsByProjection);
            UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 选择向量投影排序函数: %s"), *OutFunctionName.ToString());
			return true;
		case EVectorSortMode::ByAxis:
			OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortVectorsByAxis);
            UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 选择向量坐标轴排序函数: %s"), *OutFunctionName.ToString());
			return true;
	}

	FXToolsErrorReporter::Error(
		LogBlueprint,
		FString::Printf(TEXT("[智能排序] 未找到匹配的向量排序函数，枚举值: %d"), (int32)EnumValue),
		TEXT("K2Node_SmartSort::DetermineVectorSortFunction"));
	return false;
}

bool UK2Node_SmartSort::DetermineBasicTypeSortFunction(const FEdGraphPinType& ConnectedType, FName& OutFunctionName)
{
	if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortIntegerArray);
        UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 选择整数排序函数: %s"), *OutFunctionName.ToString());
		return true;
	}
	else if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Float ||
			 ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Double ||
			 ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortFloatArray);
        UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 选择浮点数排序函数: %s"), *OutFunctionName.ToString());
		return true;
	}
	else if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortStringArray);
        UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 选择字符串排序函数: %s"), *OutFunctionName.ToString());
		return true;
	}
	else if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(USortLibrary, SortNameArray);
        UE_LOG(LogBlueprint, Verbose, TEXT("[智能排序] 选择命名排序函数: %s"), *OutFunctionName.ToString());
		return true;
	}

	FXToolsErrorReporter::Error(
		LogBlueprint,
		FString::Printf(TEXT("[智能排序] 未找到匹配的基础类型排序函数，类型: %s"), *ConnectedType.PinCategory.ToString()),
		TEXT("K2Node_SmartSort::DetermineBasicTypeSortFunction"));
	return false;
}



FProperty* UK2Node_SmartSort::FindPropertyByName(UScriptStruct* StructType, FName PropertyName)
{
	if (!StructType || PropertyName.IsNone())
	{
		return nullptr;
	}

	for (TFieldIterator<FProperty> PropertyIt(StructType); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (Property->GetFName() == PropertyName || Property->GetAuthoredName() == PropertyName.ToString())
		{
			return Property;
		}
	}

	return nullptr;
}

bool UK2Node_SmartSort::ConnectArrayInputPin(FKismetCompilerContext& CompilerContext, UEdGraphPin* ArrayInputPin, UK2Node_CallFunction* CallFunctionNode)
{
	// 定义可能的数组输入引脚名称
	static const TArray<FString> ArrayInputPinNames = {
		TEXT("Actors"),
		TEXT("Vectors"),
		TEXT("InArray"),
		TEXT("TargetArray")  // 添加结构体排序函数的引脚名称
	};

	// 查找匹配的输入引脚
	for (const FString& PinName : ArrayInputPinNames)
	{
		if (UEdGraphPin* FuncInputArrayPin = CallFunctionNode->FindPin(*PinName, EGPD_Input))
		{
			CompilerContext.MovePinLinksToIntermediate(*ArrayInputPin, *FuncInputArrayPin);
			return true;
		}
	}

	return false;
}

void UK2Node_SmartSort::ConnectOutputPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode)
{
	// 找到库函数中对应的数组输出引脚
	UEdGraphPin* SortedArrayOutputPin = this->GetSortedArrayOutputPin();

	UEdGraphPin* FuncOutputArrayPin = CallFunctionNode->FindPin(TEXT("SortedActors"), EGPD_Output);
	if(!FuncOutputArrayPin) FuncOutputArrayPin = CallFunctionNode->FindPin(TEXT("SortedVectors"), EGPD_Output);
	if(!FuncOutputArrayPin) FuncOutputArrayPin = CallFunctionNode->FindPin(TEXT("SortedArray"), EGPD_Output);
	if(!FuncOutputArrayPin) FuncOutputArrayPin = CallFunctionNode->FindPin(TEXT("ReturnValue"), EGPD_Output);

	if(FuncOutputArrayPin && SortedArrayOutputPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*SortedArrayOutputPin, *FuncOutputArrayPin);
	}
	else if (!FuncOutputArrayPin && SortedArrayOutputPin)
	{
		// 对于void函数（如SortArrayByPropertyInPlace），我们需要特殊处理
		// 因为函数原地修改数组，我们需要将智能排序节点的输出连接传递给输入数组的连接
		UEdGraphPin* FuncInputArrayPin = CallFunctionNode->FindPin(TEXT("TargetArray"), EGPD_Input);
		if (FuncInputArrayPin && FuncInputArrayPin->LinkedTo.Num() > 0)
		{
			// 获取输入数组的源连接
			UEdGraphPin* SourceArrayPin = FuncInputArrayPin->LinkedTo[0];

			// 将智能排序节点的输出连接到原始数组源
			// 这样输出就会指向修改后的数组
			if (SortedArrayOutputPin->LinkedTo.Num() > 0)
			{
				for (UEdGraphPin* LinkedPin : SortedArrayOutputPin->LinkedTo)
				{
					LinkedPin->MakeLinkTo(SourceArrayPin);
				}
			}
		}
	}

	// 连接原始索引输出
	UEdGraphPin* IndicesOutputPin = this->FindPin(FSmartSort_Helper::PN_OriginalIndices);
	UEdGraphPin* FuncIndicesOutputPin = CallFunctionNode->FindPin(TEXT("OriginalIndices"), EGPD_Output);
	if(FuncIndicesOutputPin && IndicesOutputPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*IndicesOutputPin, *FuncIndicesOutputPin);
	}
}

void UK2Node_SmartSort::ConnectDynamicInputPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode)
{
	// 连接升序/降序引脚
	UEdGraphPin* AscendingPin = this->FindPin(FSmartSort_Helper::PN_Ascending);
	UEdGraphPin* FuncAscendingPin = CallFunctionNode->FindPin(TEXT("bAscending"), EGPD_Input);
	if(FuncAscendingPin && AscendingPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*AscendingPin, *FuncAscendingPin);
	}

	// 连接其他特定的动态引脚
	UEdGraphPin* LocationPin = this->FindPin(FSmartSort_Helper::PN_Location);
	if(LocationPin)
	{
		// 对于Actor排序，可能是Location或Center
		UEdGraphPin* FuncLocationPin = CallFunctionNode->FindPin(TEXT("Location"), EGPD_Input);
		if(!FuncLocationPin) FuncLocationPin = CallFunctionNode->FindPin(TEXT("Center"), EGPD_Input);
		if(FuncLocationPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*LocationPin, *FuncLocationPin);
		}
	}

	UEdGraphPin* DirectionPin = this->FindPin(FSmartSort_Helper::PN_Direction);
	if(DirectionPin)
	{
		UEdGraphPin* FuncDirectionPin = CallFunctionNode->FindPin(TEXT("Direction"), EGPD_Input);
		if(FuncDirectionPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*DirectionPin, *FuncDirectionPin);
		}
	}

	UEdGraphPin* AxisPin = this->FindPin(FSmartSort_Helper::PN_Axis);
	if(AxisPin)
	{
		UEdGraphPin* FuncAxisPin = CallFunctionNode->FindPin(TEXT("Axis"), EGPD_Input);
		if(FuncAxisPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*AxisPin, *FuncAxisPin);
		}
	}

	// 连接属性名称引脚（用于结构体排序）
	UEdGraphPin* PropertyNamePin = this->FindPin(FSmartSort_Helper::PN_PropertyName);
	if(PropertyNamePin)
	{
		UEdGraphPin* FuncPropertyNamePin = CallFunctionNode->FindPin(TEXT("PropertyName"), EGPD_Input);
		if(FuncPropertyNamePin)
		{
			CompilerContext.MovePinLinksToIntermediate(*PropertyNamePin, *FuncPropertyNamePin);
		}
	}

	// 连接Then执行引脚（只对非Pure函数）
	UFunction* Function = CallFunctionNode->GetTargetFunction();
	bool bIsPureFunction = Function && Function->HasAnyFunctionFlags(FUNC_BlueprintPure);

	if (!bIsPureFunction)
	{
		UEdGraphPin* ThenPin = this->GetThenPin();
		UEdGraphPin* FuncThenPin = CallFunctionNode->GetThenPin();

		if(ThenPin && FuncThenPin)
		{
			CompilerContext.MovePinLinksToIntermediate(*ThenPin, *FuncThenPin);
		}
		else
		{
			UE_LOG(LogBlueprint, Warning, TEXT("[智能排序] 无法连接Then引脚: ThenPin=%s, FuncThenPin=%s"),
				ThenPin ? TEXT("Valid") : TEXT("NULL"),
				FuncThenPin ? TEXT("Valid") : TEXT("NULL"));
		}
	}
	// Pure函数的Then引脚连接在CreatePureFunctionExecutionFlow中处理
}

void UK2Node_SmartSort::EarlyValidation(FCompilerResultsLog& MessageLog) const
{
	Super::EarlyValidation(MessageLog);

	const UEdGraphPin* ArrayPin = GetArrayInputPin();
	if (!ArrayPin || ArrayPin->LinkedTo.Num() == 0)
	{
		MessageLog.Warning(*LOCTEXT("SmartSort_NoArray", "警告：[智能排序] 节点 %% 未连接任何数组。").ToString(), this);
	}
	else if (GetResolvedArrayType().PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		MessageLog.Error(*LOCTEXT("SmartSort_ResolveFailed", "错误：[智能排序] 节点 %% 未能解析出有效的数组类型。").ToString(), this);
	}
}

FText UK2Node_SmartSort::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	// 获取连接的数组类型
	FEdGraphPinType ConnectedType = GetResolvedArrayType();

	// 如果是通配符类型（未连接），显示默认标题
	if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		return LOCTEXT("NodeTitle_Default", "K2_数组排序");
	}

	// 对于结构体类型（排除FVector），显示"结构体属性排序"
	if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Struct && 
		ConnectedType.PinSubCategoryObject.IsValid() &&
		!IsVectorType(ConnectedType))
	{
		return LOCTEXT("NodeTitle_Struct", "K2_结构体属性排序");
	}

	// 使用辅助函数获取类型显示名称
	FString TypeName = GetTypeDisplayName(ConnectedType);

	// 生成最终标题
	return FText::Format(
		LOCTEXT("NodeTitle_Dynamic", "K2_{0}排序"),
		FText::FromString(TypeName)
	);
}

FText UK2Node_SmartSort::GetTooltipText() const
{
	// 获取当前连接的数组类型来生成动态tooltip
	FEdGraphPinType ConnectedType = GetResolvedArrayType();

	if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		// 未连接时显示通用说明
		return LOCTEXT("NodeTooltip_Default",
			"智能数组排序节点\n"
			"• 自动识别数组类型并提供相应的排序选项\n"
			"• 支持Actor、Vector、Integer、Float、String、Name等类型\n"
			"• 提供多种排序模式：距离、高度、坐标轴、角度等\n"
			"• 返回排序后的数组和原始索引映射");
	}
	else
	{
		// 连接后显示特定类型的说明
		FString TypeName = GetTypeDisplayName(ConnectedType);

		if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Object &&
			ConnectedType.PinSubCategoryObject.IsValid())
		{
			if (UClass* Class = Cast<UClass>(ConnectedType.PinSubCategoryObject.Get()))
			{
				if (Class->IsChildOf(AActor::StaticClass()))
				{
					return LOCTEXT("NodeTooltip_Actor",
						"Actor数组排序\n"
						"• 按距离：相对于指定位置的距离排序\n"
						"• 按高度：根据Z坐标排序\n"
						"• 按坐标轴：沿指定轴(X/Y/Z)排序\n"
						"• 按角度：相对于指定方向的夹角排序\n"
						"• 按方位角：相对于指定位置的方位角排序");
				}
			}
		}
		else if (ConnectedType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
				 ConnectedType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
		{
			return LOCTEXT("NodeTooltip_Vector",
				"Vector数组排序\n"
				"• 按长度：根据向量的模长排序\n"
				"• 按投影：在指定方向上的投影长度排序\n"
				"• 按坐标轴：沿指定轴(X/Y/Z)的分量排序");
		}

		// 默认情况：基础类型或其他类型
		return FText::Format(
			LOCTEXT("NodeTooltip_Basic",
				"{0}数组排序\n"
				"• 对{0}类型的数组进行升序或降序排序\n"
				"• 返回排序后的数组和原始索引映射"),
			FText::FromString(TypeName)
		);
	}
}

FText UK2Node_SmartSort::GetMenuCategory() const
{
	return LOCTEXT("SmartSortCategory", "XTools|排序");
}

FName UK2Node_SmartSort::GetCornerIcon() const
{
	return FName();
}

void UK2Node_SmartSort::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UK2Node_SmartSort::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	if (MyPin->PinName == FSmartSort_Helper::PN_TargetArray && OtherPin != nullptr && !OtherPin->PinType.IsArray())
	{
		OutReason = LOCTEXT("InputNotArray", "输入必须是一个数组。").ToString();
		return true;
	}
	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

UEdGraphPin* UK2Node_SmartSort::GetArrayInputPin() const
{
	return this->FindPin(FSmartSort_Helper::PN_TargetArray);
}

UEdGraphPin* UK2Node_SmartSort::GetSortedArrayOutputPin() const
{
	return this->FindPin(FSmartSort_Helper::PN_SortedArray);
}

UEdGraphPin* UK2Node_SmartSort::GetSortModePin() const
{
	return this->FindPin(FSmartSort_Helper::PN_SortMode);
}



void UK2Node_SmartSort::CreatePureFunctionExecutionFlow(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode, UEdGraph* SourceGraph)
{
	// 对于Pure函数，我们需要创建一个中间的执行节点来连接执行流
	// 使用一个简单的"Do Nothing"节点来传递执行流

	// 创建一个"Print String"节点作为中间节点（但不实际打印任何内容）
	UK2Node_CallFunction* PassthroughNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	PassthroughNode->FunctionReference.SetExternalMember(
		GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString),
		UKismetSystemLibrary::StaticClass()
	);
	PassthroughNode->AllocateDefaultPins();

	// 连接执行引脚
	UEdGraphPin* MyExecPin = this->GetExecPin();
	UEdGraphPin* PassthroughExecPin = PassthroughNode->GetExecPin();
	UEdGraphPin* PassthroughThenPin = PassthroughNode->GetThenPin();
	UEdGraphPin* MyThenPin = this->GetThenPin();

	if (MyExecPin && PassthroughExecPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*MyExecPin, *PassthroughExecPin);
	}

	if (PassthroughThenPin && MyThenPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*MyThenPin, *PassthroughThenPin);
	}

	// 设置PrintString的参数为空字符串，这样不会有任何输出
	UEdGraphPin* InStringPin = PassthroughNode->FindPin(TEXT("InString"), EGPD_Input);
	if (InStringPin)
	{
		InStringPin->DefaultValue = TEXT("");
	}

	// 连接Pure函数的输出到我们的输出引脚
	// 这样Pure函数会在输出被使用时自动执行

	UE_LOG(LogBlueprint, Warning, TEXT("[智能排序] Pure函数执行流创建完成，使用中间执行节点"));
}

void UK2Node_SmartSort::SetEnumPinDefaultValue(UEdGraphPin* EnumPin, UEnum* EnumClass)
{
	if (!EnumPin || !EnumClass)
	{
		return;
	}

	// 如果引脚已经有默认值，不要覆盖
	if (!EnumPin->DefaultValue.IsEmpty())
	{
		return;
	}

	// 设置为枚举的第一项（排除MAX值）
	if (EnumClass->NumEnums() > 1)
	{
		FString FirstEnumName = EnumClass->GetNameStringByIndex(0);
		EnumPin->DefaultValue = FirstEnumName;
		UE_LOG(LogBlueprint, Warning, TEXT("[智能排序] 设置枚举默认值: %s = %s"),
			*EnumPin->PinName.ToString(), *FirstEnumName);
	}
}

FString UK2Node_SmartSort::GetTypeDisplayName(const FEdGraphPinType& PinType) const
{
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object && PinType.PinSubCategoryObject.IsValid())
	{
		if (UClass* Class = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
		{
			if (Class->IsChildOf(AActor::StaticClass()))
			{
				return TEXT("Actor");
			}
			return Class->GetDisplayNameText().ToString();
		}
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject.IsValid())
	{
		// 特殊处理 FVector（返回简短名称）
		if (PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
		{
			return TEXT("Vector");
		}

		// 其他结构体返回实际名称
		if (UScriptStruct* StructType = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()))
		{
			// 返回结构体的显示名称，移除 'F' 前缀（如果有）
			FString StructName = StructType->GetDisplayNameText().ToString();
			if (StructName.IsEmpty())
			{
				StructName = StructType->GetName();
				// 移除 'F' 前缀（UE 结构体命名约定）
				if (StructName.StartsWith(TEXT("F")) && StructName.Len() > 1 && FChar::IsUpper(StructName[1]))
				{
					StructName.RemoveAt(0);
				}
			}
			return StructName;
		}
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		return TEXT("Integer");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Float ||
		 PinType.PinCategory == UEdGraphSchema_K2::PC_Double ||
		 PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		return TEXT("Float");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		return TEXT("String");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		return TEXT("Name");
	}

	// 未知类型，使用类型名称
	return PinType.PinCategory.ToString();
}

TArray<FString> UK2Node_SmartSort::GetAvailableProperties(const FEdGraphPinType& ArrayType) const
{
	TArray<FString> PropertyNames;

	if (ArrayType.PinCategory == UEdGraphSchema_K2::PC_Struct && ArrayType.PinSubCategoryObject.IsValid())
	{
		// 结构体数组：遍历结构体属性
		if (UScriptStruct* StructType = Cast<UScriptStruct>(ArrayType.PinSubCategoryObject.Get()))
		{
			for (TFieldIterator<FProperty> PropertyIt(StructType); PropertyIt; ++PropertyIt)
			{
				const FProperty* Property = *PropertyIt;

				// 只添加可排序的属性类型
				if (IsPropertySortable(Property))
				{
					PropertyNames.Add(Property->GetAuthoredName());
				}
			}
		}
	}


	return PropertyNames;
}

TArray<TSharedPtr<FString>> UK2Node_SmartSort::GetPropertyOptions() const
{
	TArray<TSharedPtr<FString>> Options;

	// 获取当前连接的数组类型
	UEdGraphPin* ArrayInputPin = GetArrayInputPin();
	if (ArrayInputPin && ArrayInputPin->LinkedTo.Num() > 0)
	{
		FEdGraphPinType ConnectedType = ArrayInputPin->LinkedTo[0]->PinType;
		TArray<FString> PropertyNames = GetAvailableProperties(ConnectedType);
		Options.Reserve(PropertyNames.Num());

		for (const FString& PropertyName : PropertyNames)
		{
			Options.Add(MakeShareable(new FString(PropertyName)));
		}
	}

	return Options;
}

bool UK2Node_SmartSort::IsGenericPropertySortSupported(const FEdGraphPinType& ArrayType) const
{
	// 只支持结构体数组的通用属性排序
	return ArrayType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
		   ArrayType.PinSubCategoryObject.IsValid();
}

bool UK2Node_SmartSort::IsPropertySortable(const FProperty* Property) const
{
	if (!Property)
	{
		return false;
	}

	// 支持的可排序属性类型
	return Property->IsA<FNumericProperty>() ||  // 数值类型
		   Property->IsA<FBoolProperty>() ||     // 布尔类型
		   Property->IsA<FNameProperty>() ||     // 名称类型
		   Property->IsA<FStrProperty>() ||      // 字符串类型
		   Property->IsA<FTextProperty>() ||     // 文本类型
		   Property->IsA<FEnumProperty>();       // 枚举类型
}

bool UK2Node_SmartSort::IsVectorType(const FEdGraphPinType& PinType) const
{
	return PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
		   PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get();
}

#undef LOCTEXT_NAMESPACE

#endif
