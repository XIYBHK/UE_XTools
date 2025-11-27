#include "K2Node_PointSampling.h"

#if WITH_EDITOR

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "K2Node_PointSamplingPinManager.h"
#include "PointSamplingLibrary.h"
#include "FormationSamplingLibrary.h"

#define LOCTEXT_NAMESPACE "K2Node_PointSampling"

// ============================================================================
// 构造函数和基础接口实现
// ============================================================================

UK2Node_PointSampling::UK2Node_PointSampling(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, bIsReconstructingPins(false)
#endif
{
}

void UK2Node_PointSampling::AllocateDefaultPins()
{
	// 使用引脚管理器创建基础引脚
	FPointSamplingPinManager::CreateBasePins(this);

	Super::AllocateDefaultPins();
}

void UK2Node_PointSampling::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();

	// 先恢复连接，然后再重建动态引脚
	RestoreSplitPins(OldPins);

	// 重建动态引脚（在连接恢复后）
	RebuildDynamicPins();
}

void UK2Node_PointSampling::PostReconstructNode()
{
	Super::PostReconstructNode();

	// 确保动态引脚正确显示
	RebuildDynamicPins();
}

void UK2Node_PointSampling::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

#if WITH_EDITORONLY_DATA
	// 防止递归调用
	if (bIsReconstructingPins)
	{
		return;
	}
#endif

	// 当前不需要根据引脚连接改变行为
	// 所有参数都是独立的
}

void UK2Node_PointSampling::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);
}

void UK2Node_PointSampling::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin && Pin->PinName == FPointSamplingPinNames::PN_SamplingMode)
	{
		// 当采样模式改变时，重建动态引脚
		RebuildDynamicPins();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForNode(this));
	}
}

void UK2Node_PointSampling::EarlyValidation(FCompilerResultsLog& MessageLog) const
{
	Super::EarlyValidation(MessageLog);

	// 基础验证
	const UEdGraphPin* ModePin = FindPin(FPointSamplingPinNames::PN_SamplingMode);
	if (!ModePin || ModePin->DefaultValue.IsEmpty())
	{
		MessageLog.Warning(*LOCTEXT("NoSamplingMode", "警告：[点采样] 节点 %% 未设置采样模式。").ToString(), this);
	}
}

// ============================================================================
// 节点显示相关接口
// ============================================================================

FText UK2Node_PointSampling::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	EPointSamplingMode CurrentMode = GetCurrentSamplingMode();
	FString ModeName = GetModeDisplayName(CurrentMode);

	return FText::Format(
		LOCTEXT("NodeTitle_Dynamic", "K2_{0}点采样"),
		FText::FromString(ModeName)
	);
}

FText UK2Node_PointSampling::GetTooltipText() const
{
	EPointSamplingMode CurrentMode = GetCurrentSamplingMode();

	switch (CurrentMode)
	{
	case EPointSamplingMode::SolidRectangle:
	case EPointSamplingMode::HollowRectangle:
		return LOCTEXT("Tooltip_Rectangle", "生成矩形点阵\n支持实心和空心模式\n可自定义行列数");

	case EPointSamplingMode::SpiralRectangle:
		return LOCTEXT("Tooltip_SpiralRectangle", "生成螺旋矩形点阵\n从中心向外螺旋排列");

	case EPointSamplingMode::SolidTriangle:
	case EPointSamplingMode::HollowTriangle:
		return LOCTEXT("Tooltip_Triangle", "生成三角形点阵\n支持正三角和倒三角\n支持实心和空心模式");

	case EPointSamplingMode::Circle:
		return LOCTEXT("Tooltip_Circle", "生成圆形点阵\n可控制起始角度和旋转方向");

	case EPointSamplingMode::Snowflake:
	case EPointSamplingMode::SnowflakeArc:
		return LOCTEXT("Tooltip_Snowflake", "生成雪花形点阵\n可自定义分支数和层数");

	case EPointSamplingMode::Spline:
		return LOCTEXT("Tooltip_Spline", "沿样条线生成点阵\n支持闭合样条线");

	case EPointSamplingMode::StaticMeshVertices:
		return LOCTEXT("Tooltip_StaticMesh", "基于静态网格体顶点生成点阵\n可选择LOD级别和边界顶点");

	case EPointSamplingMode::SkeletalSockets:
		return LOCTEXT("Tooltip_SkeletalSockets", "基于骨骼网格体插槽生成点阵\n可通过前缀过滤插槽");

	case EPointSamplingMode::TexturePixels:
		return LOCTEXT("Tooltip_TexturePixels", "基于图片像素生成点阵\n可控制采样阈值和缩放");

	default:
		return LOCTEXT("Tooltip_Default", "智能点采样节点\n根据选择的模式动态显示相应参数");
	}
}

FText UK2Node_PointSampling::GetMenuCategory() const
{
	return LOCTEXT("PointSamplingCategory", "XTools|点采样");
}

FName UK2Node_PointSampling::GetCornerIcon() const
{
	return FName();
}

void UK2Node_PointSampling::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UK2Node_PointSampling::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	// 暂无特殊连接限制
	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

// ============================================================================
// 引脚访问器
// ============================================================================

UEdGraphPin* UK2Node_PointSampling::GetSamplingModePin() const
{
	return FindPin(FPointSamplingPinNames::PN_SamplingMode);
}

UEdGraphPin* UK2Node_PointSampling::GetOutputPositionsPin() const
{
	return FindPin(FPointSamplingPinNames::PN_OutputPositions);
}

// ============================================================================
// 私有辅助方法
// ============================================================================

void UK2Node_PointSampling::RebuildDynamicPins()
{
	EPointSamplingMode CurrentMode = GetCurrentSamplingMode();

	// 使用引脚管理器重建动态引脚
	FPointSamplingPinManager::RebuildDynamicPins(this, CurrentMode);
}

EPointSamplingMode UK2Node_PointSampling::GetCurrentSamplingMode() const
{
	const UEdGraphPin* ModePin = GetSamplingModePin();
	if (!ModePin || ModePin->DefaultValue.IsEmpty())
	{
		return EPointSamplingMode::SolidRectangle; // 默认模式
	}

	UEnum* EnumClass = StaticEnum<EPointSamplingMode>();
	int64 EnumValue = EnumClass->GetValueByNameString(ModePin->DefaultValue);
	if (EnumValue == INDEX_NONE)
	{
		return EPointSamplingMode::SolidRectangle;
	}

	return static_cast<EPointSamplingMode>(EnumValue);
}

FString UK2Node_PointSampling::GetModeDisplayName(EPointSamplingMode SamplingMode) const
{
	UEnum* EnumClass = StaticEnum<EPointSamplingMode>();
	if (!EnumClass)
	{
		return TEXT("Unknown");
	}

	int32 EnumIndex = EnumClass->GetIndexByValue(static_cast<int64>(SamplingMode));
	if (EnumIndex == INDEX_NONE)
	{
		return TEXT("Unknown");
	}

	FText DisplayText = EnumClass->GetDisplayNameTextByIndex(EnumIndex);
	return DisplayText.ToString();
}

// ============================================================================
// ExpandNode 实现 - 编译时节点展开
// ============================================================================

void UK2Node_PointSampling::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	// 1. 获取当前采样模式
	EPointSamplingMode CurrentMode = GetCurrentSamplingMode();

	// 2. 根据采样模式确定要调用的函数名
	FName FunctionName;
	if (!DetermineSamplingFunction(CurrentMode, FunctionName))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("PointSampling_NoMatchingFunction", "[点采样] 找不到与采样模式匹配的函数 for node %%.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// 3. 创建函数调用节点
	UK2Node_CallFunction* CallFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFunctionNode->FunctionReference.SetExternalMember(FunctionName, UFormationSamplingLibrary::StaticClass());
	CallFunctionNode->AllocateDefaultPins();

	// 4. 连接执行引脚
	// FormationSamplingLibrary的函数都是BlueprintCallable（有执行引脚），需要正确连接执行流
	UEdGraphPin* MyExecPin = GetExecPin();
	UEdGraphPin* MyThenPin = GetThenPin();
	UEdGraphPin* FuncExecPin = CallFunctionNode->GetExecPin();
	UEdGraphPin* FuncThenPin = CallFunctionNode->GetThenPin();

	// 连接执行流：MyExec → FuncExec → FuncThen → MyThen
	if (MyExecPin && FuncExecPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*MyExecPin, *FuncExecPin);
	}
	if (MyThenPin && FuncThenPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*MyThenPin, *FuncThenPin);
	}

	// 5. 连接通用参数引脚
	ConnectCommonPins(CompilerContext, CallFunctionNode);

	// 6. 连接模式特定的参数引脚
	ConnectModeSpecificPins(CompilerContext, CallFunctionNode, CurrentMode);

	// 7. 连接输出引脚
	ConnectOutputPins(CompilerContext, CallFunctionNode);

	// 8. 断开所有原始连接
	BreakAllNodeLinks();
}

bool UK2Node_PointSampling::DetermineSamplingFunction(EPointSamplingMode SamplingMode, FName& OutFunctionName)
{
	switch (SamplingMode)
	{
	case EPointSamplingMode::SolidRectangle:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateSolidRectangle);
		return true;

	case EPointSamplingMode::HollowRectangle:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateHollowRectangle);
		return true;

	case EPointSamplingMode::SpiralRectangle:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateSpiralRectangle);
		return true;

	case EPointSamplingMode::SolidTriangle:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateSolidTriangle);
		return true;

	case EPointSamplingMode::HollowTriangle:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateHollowTriangle);
		return true;

	case EPointSamplingMode::Circle:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateCircle);
		return true;

	case EPointSamplingMode::Snowflake:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateSnowflake);
		return true;

	case EPointSamplingMode::SnowflakeArc:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateSnowflakeArc);
		return true;

	case EPointSamplingMode::Spline:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateAlongSpline);
		return true;

	case EPointSamplingMode::SplineBoundary:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateSplineBoundary);
		return true;

	case EPointSamplingMode::StaticMeshVertices:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateFromStaticMesh);
		return true;

	case EPointSamplingMode::SkeletalSockets:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateFromSkeletalSockets);
		return true;

	case EPointSamplingMode::TexturePixels:
		OutFunctionName = GET_FUNCTION_NAME_CHECKED(UFormationSamplingLibrary, GenerateFromTexture);
		return true;

	default:
		return false;
	}
}

void UK2Node_PointSampling::ConnectCommonPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode)
{
	// 连接PointCount引脚（大多数函数都有，样条线、网格和纹理除外）
	UEdGraphPin* PointCountPin = FindPin(FPointSamplingPinNames::PN_PointCount);
	UEdGraphPin* FuncPointCountPin = CallFunctionNode->FindPin(TEXT("PointCount"), EGPD_Input);
	if (PointCountPin && FuncPointCountPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*PointCountPin, *FuncPointCountPin);
	}

	// 连接CenterLocation引脚（样条线、网格和骨骼除外）
	UEdGraphPin* CenterLocationPin = FindPin(FPointSamplingPinNames::PN_CenterLocation);
	UEdGraphPin* FuncCenterLocationPin = CallFunctionNode->FindPin(TEXT("CenterLocation"), EGPD_Input);
	if (CenterLocationPin && FuncCenterLocationPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*CenterLocationPin, *FuncCenterLocationPin);
	}

	// 连接Rotation引脚（样条线、网格和骨骼除外）
	UEdGraphPin* RotationPin = FindPin(FPointSamplingPinNames::PN_Rotation);
	UEdGraphPin* FuncRotationPin = CallFunctionNode->FindPin(TEXT("Rotation"), EGPD_Input);
	if (RotationPin && FuncRotationPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*RotationPin, *FuncRotationPin);
	}

	// 连接CoordinateSpace引脚（所有函数都有）
	UEdGraphPin* CoordinateSpacePin = FindPin(FPointSamplingPinNames::PN_CoordinateSpace);
	UEdGraphPin* FuncCoordinateSpacePin = CallFunctionNode->FindPin(TEXT("CoordinateSpace"), EGPD_Input);
	if (CoordinateSpacePin && FuncCoordinateSpacePin)
	{
		CompilerContext.MovePinLinksToIntermediate(*CoordinateSpacePin, *FuncCoordinateSpacePin);
	}

	// 连接Spacing引脚（矩形、三角形和螺旋矩形有）
	UEdGraphPin* SpacingPin = FindPin(FPointSamplingPinNames::PN_Spacing);
	UEdGraphPin* FuncSpacingPin = CallFunctionNode->FindPin(TEXT("Spacing"), EGPD_Input);
	if (SpacingPin && FuncSpacingPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*SpacingPin, *FuncSpacingPin);
	}

	// 连接JitterStrength引脚（除了样条线、网格和纹理）
	UEdGraphPin* JitterStrengthPin = FindPin(FPointSamplingPinNames::PN_JitterStrength);
	UEdGraphPin* FuncJitterStrengthPin = CallFunctionNode->FindPin(TEXT("JitterStrength"), EGPD_Input);
	if (JitterStrengthPin && FuncJitterStrengthPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*JitterStrengthPin, *FuncJitterStrengthPin);
	}

	// 连接RandomSeed引脚（除了样条线和网格）
	UEdGraphPin* RandomSeedPin = FindPin(FPointSamplingPinNames::PN_RandomSeed);
	UEdGraphPin* FuncRandomSeedPin = CallFunctionNode->FindPin(TEXT("RandomSeed"), EGPD_Input);
	if (RandomSeedPin && FuncRandomSeedPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*RandomSeedPin, *FuncRandomSeedPin);
	}
}

void UK2Node_PointSampling::ConnectModeSpecificPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode, EPointSamplingMode SamplingMode)
{
	switch (SamplingMode)
	{
	case EPointSamplingMode::SolidRectangle:
	case EPointSamplingMode::HollowRectangle:
	case EPointSamplingMode::SpiralRectangle:
		{
			// 矩形参数
			UEdGraphPin* RowCountPin = FindPin(FPointSamplingPinNames::PN_RowCount);
			UEdGraphPin* FuncRowCountPin = CallFunctionNode->FindPin(TEXT("RowCount"), EGPD_Input);
			if (RowCountPin && FuncRowCountPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*RowCountPin, *FuncRowCountPin);
			}

			UEdGraphPin* ColumnCountPin = FindPin(FPointSamplingPinNames::PN_ColumnCount);
			UEdGraphPin* FuncColumnCountPin = CallFunctionNode->FindPin(TEXT("ColumnCount"), EGPD_Input);
			if (ColumnCountPin && FuncColumnCountPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*ColumnCountPin, *FuncColumnCountPin);
			}

			// 螺旋矩形特有
			if (SamplingMode == EPointSamplingMode::SpiralRectangle)
			{
				UEdGraphPin* SpiralTurnsPin = FindPin(FPointSamplingPinNames::PN_SpiralTurns);
				UEdGraphPin* FuncSpiralTurnsPin = CallFunctionNode->FindPin(TEXT("SpiralTurns"), EGPD_Input);
				if (SpiralTurnsPin && FuncSpiralTurnsPin)
				{
					CompilerContext.MovePinLinksToIntermediate(*SpiralTurnsPin, *FuncSpiralTurnsPin);
				}
			}
		}
		break;

	case EPointSamplingMode::SolidTriangle:
	case EPointSamplingMode::HollowTriangle:
		{
			// 三角形参数
			UEdGraphPin* InvertedTrianglePin = FindPin(FPointSamplingPinNames::PN_InvertedTriangle);
			UEdGraphPin* FuncInvertedPin = CallFunctionNode->FindPin(TEXT("bInverted"), EGPD_Input);
			if (InvertedTrianglePin && FuncInvertedPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*InvertedTrianglePin, *FuncInvertedPin);
			}
		}
		break;

	case EPointSamplingMode::Circle:
		{
			// 圆形参数
			UEdGraphPin* RadiusPin = FindPin(FPointSamplingPinNames::PN_Radius);
			UEdGraphPin* FuncRadiusPin = CallFunctionNode->FindPin(TEXT("Radius"), EGPD_Input);
			if (RadiusPin && FuncRadiusPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*RadiusPin, *FuncRadiusPin);
			}

			UEdGraphPin* Is3DPin = FindPin(FPointSamplingPinNames::PN_Is3D);
			UEdGraphPin* FuncIs3DPin = CallFunctionNode->FindPin(TEXT("bIs3D"), EGPD_Input);
			if (Is3DPin && FuncIs3DPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*Is3DPin, *FuncIs3DPin);
			}

			UEdGraphPin* DistributionPin = FindPin(FPointSamplingPinNames::PN_DistributionMode);
			UEdGraphPin* FuncDistributionPin = CallFunctionNode->FindPin(TEXT("DistributionMode"), EGPD_Input);
			if (DistributionPin && FuncDistributionPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*DistributionPin, *FuncDistributionPin);
			}

			UEdGraphPin* MinDistancePin = FindPin(FPointSamplingPinNames::PN_MinDistance);
			UEdGraphPin* FuncMinDistancePin = CallFunctionNode->FindPin(TEXT("MinDistance"), EGPD_Input);
			if (MinDistancePin && FuncMinDistancePin)
			{
				CompilerContext.MovePinLinksToIntermediate(*MinDistancePin, *FuncMinDistancePin);
			}

			UEdGraphPin* StartAnglePin = FindPin(FPointSamplingPinNames::PN_StartAngle);
			UEdGraphPin* FuncStartAnglePin = CallFunctionNode->FindPin(TEXT("StartAngle"), EGPD_Input);
			if (StartAnglePin && FuncStartAnglePin)
			{
				CompilerContext.MovePinLinksToIntermediate(*StartAnglePin, *FuncStartAnglePin);
			}

			UEdGraphPin* ClockwisePin = FindPin(FPointSamplingPinNames::PN_Clockwise);
			UEdGraphPin* FuncClockwisePin = CallFunctionNode->FindPin(TEXT("bClockwise"), EGPD_Input);
			if (ClockwisePin && FuncClockwisePin)
			{
				CompilerContext.MovePinLinksToIntermediate(*ClockwisePin, *FuncClockwisePin);
			}
		}
		break;

	case EPointSamplingMode::Snowflake:
	case EPointSamplingMode::SnowflakeArc:
		{
			// 雪花参数
			UEdGraphPin* RadiusPin = FindPin(FPointSamplingPinNames::PN_Radius);
			UEdGraphPin* FuncRadiusPin = CallFunctionNode->FindPin(TEXT("Radius"), EGPD_Input);
			if (RadiusPin && FuncRadiusPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*RadiusPin, *FuncRadiusPin);
			}

			UEdGraphPin* LayersPin = FindPin(FPointSamplingPinNames::PN_SnowflakeLayers);
			UEdGraphPin* FuncLayersPin = CallFunctionNode->FindPin(TEXT("SnowflakeLayers"), EGPD_Input);
			if (LayersPin && FuncLayersPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*LayersPin, *FuncLayersPin);
			}

			// Spacing已在通用引脚中连接

			// 雪花弧形特有
			if (SamplingMode == EPointSamplingMode::SnowflakeArc)
			{
				UEdGraphPin* StartAnglePin = FindPin(FPointSamplingPinNames::PN_StartAngle);
				UEdGraphPin* FuncStartAnglePin = CallFunctionNode->FindPin(TEXT("StartAngle"), EGPD_Input);
				if (StartAnglePin && FuncStartAnglePin)
				{
					CompilerContext.MovePinLinksToIntermediate(*StartAnglePin, *FuncStartAnglePin);
				}

				// 注意：FormationSamplingLibrary.h中雪花弧形使用ArcAngle参数
				// 但PinManager中使用的是SnowflakeBranches，需要调整
				UEdGraphPin* BranchesPin = FindPin(FPointSamplingPinNames::PN_SnowflakeBranches);
				UEdGraphPin* FuncArcAnglePin = CallFunctionNode->FindPin(TEXT("ArcAngle"), EGPD_Input);
				if (BranchesPin && FuncArcAnglePin)
				{
					CompilerContext.MovePinLinksToIntermediate(*BranchesPin, *FuncArcAnglePin);
				}
			}
		}
		break;

	case EPointSamplingMode::Spline:
		{
			// 样条线参数
			UEdGraphPin* ControlPointsPin = FindPin(FPointSamplingPinNames::PN_SplineControlPoints);
			UEdGraphPin* FuncControlPointsPin = CallFunctionNode->FindPin(TEXT("SplineControlPoints"), EGPD_Input);
			if (ControlPointsPin && FuncControlPointsPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*ControlPointsPin, *FuncControlPointsPin);
			}

			UEdGraphPin* ClosedSplinePin = FindPin(FPointSamplingPinNames::PN_ClosedSpline);
			UEdGraphPin* FuncClosedSplinePin = CallFunctionNode->FindPin(TEXT("bClosedSpline"), EGPD_Input);
			if (ClosedSplinePin && FuncClosedSplinePin)
			{
				CompilerContext.MovePinLinksToIntermediate(*ClosedSplinePin, *FuncClosedSplinePin);
			}
		}
		break;

	case EPointSamplingMode::SplineBoundary:
		{
			// 样条线边界参数
			// TargetPointCount（从PointCount引脚连接）
			UEdGraphPin* PointCountPin = FindPin(FPointSamplingPinNames::PN_PointCount);
			UEdGraphPin* FuncTargetPointCountPin = CallFunctionNode->FindPin(TEXT("TargetPointCount"), EGPD_Input);
			if (PointCountPin && FuncTargetPointCountPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*PointCountPin, *FuncTargetPointCountPin);
			}

			UEdGraphPin* ControlPointsPin = FindPin(FPointSamplingPinNames::PN_SplineControlPoints);
			UEdGraphPin* FuncControlPointsPin = CallFunctionNode->FindPin(TEXT("SplineControlPoints"), EGPD_Input);
			if (ControlPointsPin && FuncControlPointsPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*ControlPointsPin, *FuncControlPointsPin);
			}

			UEdGraphPin* MinDistancePin = FindPin(FPointSamplingPinNames::PN_MinDistance);
			UEdGraphPin* FuncMinDistancePin = CallFunctionNode->FindPin(TEXT("MinDistance"), EGPD_Input);
			if (MinDistancePin && FuncMinDistancePin)
			{
				CompilerContext.MovePinLinksToIntermediate(*MinDistancePin, *FuncMinDistancePin);
			}
		}
		break;

	case EPointSamplingMode::StaticMeshVertices:
		{
			// 静态网格体参数
			UEdGraphPin* StaticMeshPin = FindPin(FPointSamplingPinNames::PN_StaticMesh);
			UEdGraphPin* FuncStaticMeshPin = CallFunctionNode->FindPin(TEXT("StaticMesh"), EGPD_Input);
			if (StaticMeshPin && FuncStaticMeshPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*StaticMeshPin, *FuncStaticMeshPin);
			}

			// Transform - 需要从Rotation和CenterLocation构建（暂时简化，直接传递）
			// 实际应该创建MakeTransform节点

			UEdGraphPin* LODLevelPin = FindPin(FPointSamplingPinNames::PN_LODLevel);
			UEdGraphPin* FuncLODLevelPin = CallFunctionNode->FindPin(TEXT("LODLevel"), EGPD_Input);
			if (LODLevelPin && FuncLODLevelPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*LODLevelPin, *FuncLODLevelPin);
			}

			UEdGraphPin* BoundaryVerticesOnlyPin = FindPin(FPointSamplingPinNames::PN_BoundaryVerticesOnly);
			UEdGraphPin* FuncBoundaryVerticesOnlyPin = CallFunctionNode->FindPin(TEXT("bBoundaryVerticesOnly"), EGPD_Input);
			if (BoundaryVerticesOnlyPin && FuncBoundaryVerticesOnlyPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*BoundaryVerticesOnlyPin, *FuncBoundaryVerticesOnlyPin);
			}
		}
		break;

	case EPointSamplingMode::SkeletalSockets:
		{
			// 骨骼网格体参数
			UEdGraphPin* SkeletalMeshPin = FindPin(FPointSamplingPinNames::PN_SkeletalMesh);
			UEdGraphPin* FuncSkeletalMeshPin = CallFunctionNode->FindPin(TEXT("SkeletalMesh"), EGPD_Input);
			if (SkeletalMeshPin && FuncSkeletalMeshPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*SkeletalMeshPin, *FuncSkeletalMeshPin);
			}

			UEdGraphPin* SocketNamePrefixPin = FindPin(FPointSamplingPinNames::PN_SocketNamePrefix);
			UEdGraphPin* FuncSocketNamePrefixPin = CallFunctionNode->FindPin(TEXT("SocketNamePrefix"), EGPD_Input);
			if (SocketNamePrefixPin && FuncSocketNamePrefixPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*SocketNamePrefixPin, *FuncSocketNamePrefixPin);
			}
		}
		break;

	case EPointSamplingMode::TexturePixels:
		{
			// 纹理参数
			UEdGraphPin* TexturePin = FindPin(FPointSamplingPinNames::PN_Texture);
			UEdGraphPin* FuncTexturePin = CallFunctionNode->FindPin(TEXT("Texture"), EGPD_Input);
			if (TexturePin && FuncTexturePin)
			{
				CompilerContext.MovePinLinksToIntermediate(*TexturePin, *FuncTexturePin);
			}

			UEdGraphPin* PixelThresholdPin = FindPin(FPointSamplingPinNames::PN_PixelThreshold);
			UEdGraphPin* FuncPixelThresholdPin = CallFunctionNode->FindPin(TEXT("PixelThreshold"), EGPD_Input);
			if (PixelThresholdPin && FuncPixelThresholdPin)
			{
				CompilerContext.MovePinLinksToIntermediate(*PixelThresholdPin, *FuncPixelThresholdPin);
			}

			UEdGraphPin* TextureScalePin = FindPin(FPointSamplingPinNames::PN_TextureScale);
			UEdGraphPin* FuncTextureScalePin = CallFunctionNode->FindPin(TEXT("TextureScale"), EGPD_Input);
			if (TextureScalePin && FuncTextureScalePin)
			{
				CompilerContext.MovePinLinksToIntermediate(*TextureScalePin, *FuncTextureScalePin);
			}
		}
		break;
	}
}

void UK2Node_PointSampling::ConnectOutputPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode)
{
	// 连接输出位置数组引脚
	UEdGraphPin* OutputPositionsPin = GetOutputPositionsPin();
	UEdGraphPin* FuncReturnValuePin = CallFunctionNode->GetReturnValuePin();

	if (OutputPositionsPin && FuncReturnValuePin)
	{
		CompilerContext.MovePinLinksToIntermediate(*OutputPositionsPin, *FuncReturnValuePin);
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
